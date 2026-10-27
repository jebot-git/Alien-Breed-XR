#include "native.h"
#include "domed.hpp"
#include "game.hpp"
#include "render.hpp"
#include "xr.hpp"
#include <SDL.h>
#include <SDL_opengl.h>
#include <algorithm>
#include <cmath>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>

namespace {
using namespace tempest;
std::unique_ptr<DomeLink> dome;
std::unique_ptr<XR> xr;
Renderer renderer;
DomeLaunch launch;
SDL_Window *window = nullptr;
bool wantsXR = false, paused = false, quitting = false, initialized = false, everFocused = false,
     snapHeld = false;
int failedStatus = 0;
int level = 0, frameLimit = 0, frames = 0, suspends = 0, entries = 0, shots = 0;
volatile std::sig_atomic_t stopRequested = 0;
std::string capture, report;
PlayerState shotBackup;
int16_t aimBackup = 0;
bool lookBackup = false;
bool shotOverride = false;
float angle(const GameState &state) { return state.plr1.angpos * 6.283185307f / 8192; }
void release() {
    SDL_SetRelativeMouseMode(SDL_FALSE);
    if (window)
        SDL_SetWindowGrab(window, SDL_FALSE);
}
void suspend() {
    paused = true;
    ++suspends;
    release();
    if (xr)
        xr->setVisible(false);
    xr.reset();
    if (dome)
        dome->suspend();
    SDL_GL_SetSwapInterval(1);
}
void enter() {
    try {
        dome->ready();
        auto next = std::make_unique<XR>();
        next->initialize(window);
        dome->begin();
        xr = std::move(next);
        paused = false;
        everFocused = false;
        ++entries;
        wantsXR = true;
        SDL_GL_SetSwapInterval(0);
        SDL_SetRelativeMouseMode(SDL_TRUE);
        SDL_SetWindowGrab(window, SDL_TRUE);
    } catch (const std::exception &e) {
        std::cerr << "XR unavailable: " << e.what() << '\n';
        dome->end();
        xr.reset();
        wantsXR = false;
        release();
        paused = false;
    }
}
void captureFrame(int w, int h) {
    if (capture.empty())
        return;
    std::vector<unsigned char> pixels(w * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    std::ofstream out(capture, std::ios::binary);
    out << "P6\n" << w << ' ' << h << "\n255\n";
    for (int y = h - 1; y >= 0; --y)
        out.write(reinterpret_cast<char *>(pixels.data() + y * w * 3), w * 3);
}
} // namespace
static int startup(int argc, char **argv) {
    std::signal(SIGTERM, [](int) { stopRequested = 1; });
    std::signal(SIGINT, [](int) { stopRequested = 1; });
    std::string mode = "auto";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto value = [&]() {
            if (++i >= argc)
                throw std::runtime_error("Missing argument for " + arg);
            return std::string(argv[i]);
        };
        if (arg == "--xr")
            mode = "xr";
        else if (arg == "--desktop")
            mode = "desktop";
        else if (arg == "--level")
            level = std::clamp(std::stoi(value()) - 1, 0, 15);
        else if (arg == "--frames")
            frameLimit = std::max(1, std::stoi(value()));
        else if (arg == "--capture")
            capture = value();
        else if (arg == "--report")
            report = value();
        else if (arg == "--help") {
            std::cout
                << "Alien Breed XR [--xr|--desktop] [--level 1..16] [--frames N] [--capture PPM] [--report "
                   "JSON]\nF8: XR/desktop | Escape: pause | Enter: resume | R: recenter | Ctrl+Q: quit\nXR: "
                   "left stick move, right stick snap turn, right trigger fire, A/X use, left menu pause\n";
            return 1;
        }
    }
    launch = domeLaunch(mode);
    wantsXR = mode == "xr" || (mode == "auto" && launch.xr);
    if (!getenv("SDL_VIDEODRIVER") && getenv("WAYLAND_DISPLAY"))
        SDL_setenv("SDL_VIDEODRIVER", "wayland", 0);
    SDL_setenv("SDL_RENDER_DRIVER", "opengl", 1);
    dome = std::make_unique<DomeLink>(launch);
    return 0;
}
extern "C" int abxr_startup(int argc, char **argv) {
    try {
        return startup(argc, argv);
    } catch (const std::exception &error) {
        std::cerr << "Alien Breed XR: " << error.what() << '\n';
        return -1;
    }
}
extern "C" int abxr_level() { return level; }
extern "C" int abxr_status() { return failedStatus; }
extern "C" int abxr_quitting() { return quitting; }
extern "C" int abxr_poll(void *data) {
    auto &state = *static_cast<GameState *>(data);
    auto keys = SDL_GetKeyboardState(nullptr);
    static bool f8 = false, escape = false, enterKey = false, recenter = false, menu = false;
    bool key = keys[SDL_SCANCODE_F8];
    if (key && !f8 && initialized) {
        if (wantsXR) {
            wantsXR = false;
            xr.reset();
            dome->end(true);
            release();
            paused = false;
        } else
            enter();
    }
    f8 = key;
    key = keys[SDL_SCANCODE_ESCAPE];
    if (key && !escape) {
        if (paused) {
            if (wantsXR)
                enter();
            else
                paused = false;
        } else
            suspend();
    }
    escape = key;
    input_consume_key_press(0x45);
    state.key_map[0x45] = 0;
    key = keys[SDL_SCANCODE_RETURN];
    if (key && !enterKey && paused && (!dome->managed() || dome->focused())) {
        if (wantsXR)
            enter();
        else
            paused = false;
    }
    enterKey = key;
    key = keys[SDL_SCANCODE_R];
    if (key && !recenter && xr)
        xr->recenter();
    recenter = key;
    if (stopRequested || input_quit_requested() ||
        (keys[SDL_SCANCODE_Q] && (SDL_GetModState() & KMOD_CTRL)) || quitting) {
        quitting = true;
        state.running = false;
        return 0;
    }
    if (xr) {
        everFocused = everFocused || xr->focused();
        if (dome->suspended() || (everFocused && !xr->focused()))
            suspend();
    }
    if (initialized && !dome->managed() && !(SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) &&
        getenv("SDL_VIDEODRIVER") && std::string(getenv("SDL_VIDEODRIVER")) != "offscreen") {
        if (!paused)
            suspend();
    }
    if (xr && !paused) {
        auto t = xr->tracking();
        if (t.menu && !menu)
            suspend();
        menu = t.menu;
        if (xr && !paused) {
            // Use the original acceleration, collision, doors, pickups and AI.
            state.plr1_control.keys = true;
            state.plr1_control.mouse_kbd = false;
            state.plr1_control.mouse = false;
            state.plr1_control.joy = false;
            state.key_map[0x4c] = t.move.y > .25f;
            state.key_map[0x4d] = t.move.y < -.25f;
            state.key_map[0x39] = t.move.x < -.25f;
            state.key_map[0x3a] = t.move.x > .25f;
            state.key_map[0x65] = t.fire;
            state.key_map[0x40] = t.use;
            if (std::abs(t.turn.x) > .65f && !snapHeld) {
                int turn = t.turn.x > 0 ? -683 : 683;
                state.plr1.s_angpos = (state.plr1.s_angpos + turn) & 8191;
                state.plr1.angpos = state.plr1.s_angpos;
                snapHeld = true;
            }
            if (std::abs(t.turn.x) < .25f)
                snapHeld = false;
        }
    }
    if (paused || (xr && xr->focused() && !xr->tracking().headValid)) {
        std::fill(std::begin(state.key_map), std::end(state.key_map), 0);
        state.plr1.fire = state.plr1.p_fire = 0;
        SDL_Delay(8);
        return 1;
    }
    return 0;
}
extern "C" void abxr_begin_shot(void *data) {
    if (!xr || paused || !xr->focused())
        return;
    auto &s = *static_cast<GameState *>(data);
    const auto t = xr->tracking();
    if (!t.valid[1] || !t.fire)
        return;
    auto pose = t.hands[1];
    auto q = pose.orientation;
    float dx = -2 * (q.x * q.z + q.w * q.y), dy = -2 * (q.y * q.z - q.w * q.x),
          dz = -(1 - 2 * (q.x * q.x + q.y * q.y));
    float yaw = angle(s) - t.anchorYaw * 3.14159265f / 180, c = cosf(yaw), v = sinf(yaw);
    float x = pose.position.x - t.anchor.x, z = pose.position.z - t.anchor.z,
          y = pose.position.y - t.anchor.y;
    if (x * x + z * z + y * y > 2.25f)
        return;
    float wx = s.plr1.xoff / 65536.f + (c * x + v * z) * 32,
          wz = s.plr1.zoff / 65536.f + (-v * x + c * z) * 32;
    // Do not spawn shots through a wall into a neighbouring room.
    if (level_find_zone_for_point(&s.level, int(wx), int(wz), s.plr1.zone) != s.plr1.zone)
        return;
    shotBackup = s.plr1;
    aimBackup = s.plr1_p_aim_speed;
    lookBackup = s.cfg_mouse_look;
    s.cfg_mouse_look = true;
    shotOverride = true;
    float ax = c * dx + v * dz, az = -v * dx + c * dz;
    s.plr1.xoff = int32_t(wx * 65536);
    s.plr1.zoff = int32_t(wz * 65536);
    s.plr1.yoff -= int32_t(y * 8192);
    s.plr1.angpos = int(std::atan2(-ax, -az) * 8192 / 6.283185307f) & 8191;
    s.plr1.sinval = sin_lookup(s.plr1.angpos);
    s.plr1.cosval = cos_lookup(s.plr1.angpos);
    s.plr1_p_aim_speed =
        int16_t(std::clamp(-dy / std::max(.1f, std::hypot(dx, dz)) * 32768.f, -32760.f, 32760.f));
    ++shots;
}
extern "C" void abxr_end_shot(void *data) {
    if (!shotOverride)
        return;
    auto &s = *static_cast<GameState *>(data);
    // Preserve shooting side effects (ammo, animation, cooldown), restore only pose.
    s.plr1.xoff = shotBackup.xoff;
    s.plr1.zoff = shotBackup.zoff;
    s.plr1.yoff = shotBackup.yoff;
    s.plr1.angpos = shotBackup.angpos;
    s.plr1.sinval = shotBackup.sinval;
    s.plr1.cosval = shotBackup.cosval;
    s.plr1_p_aim_speed = aimBackup;
    s.cfg_mouse_look = lookBackup;
    shotOverride = false;
}
extern "C" void abxr_present(void *data) {
    auto &state = *static_cast<GameState *>(data);
    try {
        if (!initialized) {
            window = SDL_GL_GetCurrentWindow();
            if (!window)
                throw std::runtime_error("No native OpenGL window");
            SDL_SetWindowTitle(window, "Alien Breed XR");
            initialized = true;
            std::cerr << "GPU: " << glGetString(GL_RENDERER) << " / SDL " << SDL_GetCurrentVideoDriver()
                      << '\n';
            // Commit the SDL surface before asking DomeD to identify this PID.
            SDL_GL_SwapWindow(window);
            if (wantsXR)
                enter();
        }
        glUseProgram(0);
        glActiveTexture(GL_TEXTURE0);
        glDisable(GL_SCISSOR_TEST);
        int w, h;
        SDL_GL_GetDrawableSize(window, &w, &h);
        if (xr) {
            if (!xr->frame(renderer, Game{&state})) {
                suspend();
            }
        }
        renderer.desktop(Game{&state}, w, h);
        ++frames;
        if (frameLimit && frames >= frameLimit) {
            captureFrame(w, h);
            quitting = true;
            state.running = false;
        }
        SDL_GL_SwapWindow(window);
        if (!xr)
            SDL_Delay(1);
        if (!report.empty())
            std::ofstream(report) << nlohmann::json{{"frames", frames},
                                                    {"xr_entries", entries},
                                                    {"xr_suspends", suspends},
                                                    {"controller_shots", shots},
                                                    {"gpu_triangles", renderer.triangles},
                                                    {"renderer", reinterpret_cast<const char *>(
                                                                     glGetString(GL_RENDERER))},
                                                    {"video_driver", SDL_GetCurrentVideoDriver()},
                                                    {"domed_managed", dome->managed()},
                                                    {"paused", paused},
                                                    {"energy", state.plr1.energy},
                                                    {"level", state.current_level + 1}}
                                         .dump(2)
                                  << '\n';
    } catch (const std::exception &e) {
        failedStatus = 1;
        std::cerr << "Native renderer: " << e.what() << '\n';
        quitting = true;
        state.running = false;
    }
}
extern "C" void abxr_shutdown() {
    release();
    xr.reset();
    if (dome)
        dome->end();
    dome.reset();
    renderer.clear();
    std::cerr << "Native frames=" << frames << " controller_shots=" << shots << "\n";
}
