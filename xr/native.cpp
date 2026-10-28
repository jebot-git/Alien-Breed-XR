#include "native.h"
#include "combat.h"
#include "content_audit.hpp"
#include "aim.hpp"
#include "shot_origin.hpp"
#include "domed.hpp"
#include "game.hpp"
#include "locomotion.hpp"
#include "render.hpp"
#include "xr.hpp"
#include "openxr_video.hpp"
#include "stb_image.h"
#include <SDL.h>
#include "gl_platform.hpp"
#include <algorithm>
#include <climits>
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
nlohmann::json originalContent;
PlayerState shotBackup;
int16_t aimBackup = 0;
bool lookBackup = false;
bool shotOverride = false;
XrVector3f shotDirection{};
int8_t heldFireBackup=0,clickedBackup=0;
bool shotInputSuppressed=false;
unsigned long simulationTicks=0,logicPasses=0,actualShots=0;
Uint32 firstLogicMs=0,lastShotMs=0;
Uint32 minimumShotIntervalMs=0;
int lastShotGun=-1;
bool useHeld = false, pendingUse = false;
bool fireHeld=false,pendingFireClick=false;
int useActions = 0;
XrVector2f lastMovement{}, firstMovement{};
int movementSamples = 0;
Overlay overlay;
bool forcePresentation = false, explicitLevel = false, presentationHeld = true, presentationReleasePending = false;
int presentedLevel = -1, titleFrames = 0, storyFrames = 0, endingFrames = 0;
int presentedLoads = 0;
Uint32 presentationStarted = 0;
bool previousHeld = false, nextHeld = false, selectHeld = false, navHeld = false, suppressActions = false,
     requireNeutral = false;
int pendingWeapons = 0, previousActions = 0, nextActions = 0, menuOpens = 0, saves = 0, loads = 0;
float turnFraction = 0;
Uint64 lastControlTime = 0;
float physicalTarget = 1.5f, minPhysicalTarget = 1.5f, minEngineHeight = 1.5f;
int previousGun = -1, weaponChanges = 0;
int headPushbackFrames=0,muzzleRetractions=0, heightResets=0,twoHandChanges=0;
float maximumHeadPushback=0;
float crouchCompensation(const GameState &state) {
    return xr ? crouchViewCompensation(state.plr1.height / 8192.f, physicalTarget) : 0;
}
Game sceneGame(GameState &state) { return Game{&state, crouchCompensation(state), &overlay}; }
void setWindowIcon(SDL_Window *target){
    char path[512];io_make_data_path(path,sizeof(path),"alien-breed-xr.png");
    int w=0,h=0,channels=0;
    auto *pixels=stbi_load(path,&w,&h,&channels,4);
    if(!pixels)return;
    auto *surface=SDL_CreateRGBSurfaceWithFormatFrom(pixels,w,h,32,w*4,SDL_PIXELFORMAT_RGBA32);
    if(surface){SDL_SetWindowIcon(target,surface);SDL_FreeSurface(surface);}
    stbi_image_free(pixels);
}
void menuState(GameState &state, bool open) {
    overlay.menu = open;
    if (open)
        ++menuOpens;
    state.plr1.s_xspdval = state.plr1.s_zspdval = 0;
    std::fill(std::begin(state.key_map), std::end(state.key_map), 0);
    state.plr1.fire = state.plr1.p_fire = 0;
    state.plr1.clicked=state.plr1.p_clicked=0;
    pendingUse = false;
    pendingFireClick=false;
    pendingWeapons = 0;
    suppressActions = true;
    requireNeutral = true;
    lastMovement = {};
    snapHeld = false;
    turnFraction = 0;
}
void menuAction(GameState &state) {
    switch (overlay.selection) {
    case PauseResume:
        menuState(state, false);
        break;
    case PauseTurning:
        overlay.smoothTurn = !overlay.smoothTurn;
        turnFraction = 0;
        overlay.message = overlay.smoothTurn ? "SMOOTH TURN ENABLED" : "SNAP TURN ENABLED";
        break;
    case PauseHeight:
        if(!xr || !xr->resetHeight()) {
            overlay.message="HEAD TRACKING UNAVAILABLE";
            break;
        }
        physicalTarget=PLAYER_HEIGHT/8192.f;
        player_xr_reset_height(&state);
        ++heightResets;
        overlay.message="HEIGHT / FLOOR RESET";
        break;
    case PauseSave:
        if (player_xr_save(&state)) {
            ++saves;
            overlay.message = "GAME SAVED";
        } else
            overlay.message = "SAVE FAILED";
        break;
    case PauseLoad: {
        auto result = player_load_save_from_file(&state);
        if (result == PLAYER_SAVE_LOAD_FAILED) {
            overlay.message = "NO VALID SAVE TO LOAD";
            break;
        }
        ++loads;
        menuState(state, false);
        overlay.message = "GAME LOADED";
        if (result == PLAYER_SAVE_LOAD_NEED_LEVEL_RELOAD) {
            state.debug_f9_need_level_reload = true;
            state.running = false;
        }
        break;
    }
    case PauseTwoHand:
        overlay.twoHand=!overlay.twoHand;
        ++twoHandChanges;
        overlay.message=overlay.twoHand?"LEFT GRIP NEAR FORE-END TO HOLD":"TWO-HAND HOLD DISABLED";
        break;
    case PauseQuit:
        quitting = true;
        state.running = false;
        break;
    }
}
void presentPage(GameState &state, Presentation page) {
    menuState(state, false);
    overlay.presentation = page;
    presentationHeld = true;
    presentationStarted = SDL_GetTicks();
}
bool presentationEnabled() { return !frameLimit || forcePresentation; }
float angle(const GameState &state) { return state.plr1.angpos * 6.283185307f / 8192; }
void release() {
    useHeld = pendingUse = false;
    fireHeld=pendingFireClick=false;
    snapHeld = false;
    lastMovement = {};
    previousHeld = nextHeld = selectHeld = navHeld = false;
    pendingWeapons = 0;
    suppressActions = true;
    requireNeutral = true;
    turnFraction = 0;
    lastControlTime = 0;
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
    SDL_SetHint(SDL_HINT_APP_NAME,"Alien Breed XR");
    SDL_SetHint("SDL_APP_ID","alien-breed-xr");
    SDL_SetHint("SDL_VIDEO_WAYLAND_WMCLASS","alien-breed-xr");
    SDL_SetHint("SDL_VIDEO_X11_WMCLASS","alien-breed-xr");
    std::signal(SIGTERM, [](int) { stopRequested = 1; });
    std::signal(SIGINT, [](int) { stopRequested = 1; });
    std::string mode = "auto";
#ifdef _WIN32
    char *pref = SDL_GetPrefPath("de-xr", "alien-breed-xr");
    if (!pref) throw std::runtime_error("Cannot locate user data directory");
    auto user = std::filesystem::u8path(pref);
    SDL_free(pref);
    if (!getenv("ABXR_CONFIG")) {
        auto config = user / "ab3d.ini";
        if (!std::filesystem::exists(config)) {
            char *base = SDL_GetBasePath();
            if (!base) throw std::runtime_error("Cannot locate game data");
            auto source = std::filesystem::u8path(base) / "ab3d.ini.template";
            SDL_free(base);
            std::filesystem::copy_file(source, config);
        }
        SDL_setenv("ABXR_CONFIG", config.u8string().c_str(), 1);
    }
#endif
    if (!getenv("ABXR_SAVE_DIR")) {
#ifdef _WIN32
        auto directory = user / "saves";
#else
        const char *state = getenv("XDG_STATE_HOME"), *home = getenv("HOME");
        if (!state && !home)
            throw std::runtime_error("Cannot locate user save directory");
        auto directory =
            (state ? std::filesystem::path(state) : std::filesystem::path(home) / ".local/state") /
            "alien-breed-xr";
#endif
        std::filesystem::create_directories(directory);
        SDL_setenv("ABXR_SAVE_DIR", directory.u8string().c_str(), 1);
    } else
        std::filesystem::create_directories(std::filesystem::u8path(getenv("ABXR_SAVE_DIR")));
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
        else if (arg == "--level") {
            level = std::clamp(std::stoi(value()) - 1, 0, 15);
            explicitLevel = true;
        }
        else if (arg == "--presentation")
            forcePresentation = true;
        else if (arg == "--frames")
            frameLimit = std::max(1, std::stoi(value()));
        else if (arg == "--capture")
            capture = value();
        else if (arg == "--report")
            report = value();
        else if (arg == "--help") {
            std::cout
                << "Alien Breed XR [--xr|--desktop] [--level 1..16] [--frames N] [--capture PPM] [--report "
                   "JSON] [--presentation]\nBounded --frames runs skip title/story unless --presentation is set.\nF8: XR/desktop | Escape: pause | Enter: resume | R: recenter | Ctrl+Q: quit\nXR: "
                   "left stick move, right stick turn, right trigger fire, A/X use, left menu pause\n";
            return 1;
        }
    }
    launch = domeLaunch(mode);
    wantsXR = mode == "xr" || (mode == "auto" && launch.xr);
    xr_platform::selectVideo(wantsXR);
    SDL_SetMainReady();
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
    const auto *keys = SDL_GetKeyboardState(nullptr);
    static bool f8 = false, escape = false, enterKey = false, recenter = false, upKey = false,
                downKey = false;
    Uint64 now = SDL_GetPerformanceCounter();
    float dt =
        lastControlTime ? std::min(.05f, float(now - lastControlTime) / SDL_GetPerformanceFrequency()) : 0;
    lastControlTime = now;
    bool enterPressed = keys[SDL_SCANCODE_RETURN] && !enterKey;
    enterKey = keys[SDL_SCANCODE_RETURN];
    bool upPressed = keys[SDL_SCANCODE_UP] && !upKey;
    upKey = keys[SDL_SCANCODE_UP];
    bool downPressed = keys[SDL_SCANCODE_DOWN] && !downKey;
    downKey = keys[SDL_SCANCODE_DOWN];
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
        } else if (overlay.presentation == Presentation::Gameplay)
            menuState(state, !overlay.menu);
    }
    escape = key;
    input_consume_key_press(0x45);
    state.key_map[0x45] = 0;
    if (enterPressed && paused && (!dome->managed() || dome->focused())) {
        if (wantsXR)
            enter();
        else
            paused = false;
        enterPressed = false;
    }
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
        everFocused |= xr->focused();
        if (dome->suspended() || (everFocused && !xr->focused()))
            suspend();
    }
    if (initialized && !dome->managed() && !(SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) &&
        getenv("SDL_VIDEODRIVER") && std::string(getenv("SDL_VIDEODRIVER")) != "offscreen") {
        if (!paused)
            suspend();
    }
    if (overlay.presentation != Presentation::Gameplay) {
        bool selecting = keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_SPACE];
        if (xr && !paused) {
            const auto &tracking = xr->tracking();
            selecting |= tracking.use || tracking.fire;
        }
        if (!paused && selecting && !presentationHeld && SDL_GetTicks() - presentationStarted >= 250) {
            if (overlay.presentation == Presentation::Title)
                presentPage(state, Presentation::Level);
            else {
                overlay.presentation = Presentation::Gameplay;
                menuState(state, false);
                presentationReleasePending = true;
            }
        }
        presentationHeld = selecting;
        std::fill(std::begin(state.key_map), std::end(state.key_map), 0);
        state.plr1.fire = state.plr1.p_fire = 0;
        state.plr1.clicked = state.plr1.p_clicked = 0;
        if (!xr) SDL_Delay(8);
        return 1;
    }
    if (presentationReleasePending) {
        bool held = keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_SPACE];
        if (xr) held |= xr->tracking().use || xr->tracking().fire;
        presentationReleasePending = held;
        if (held) {
            std::fill(std::begin(state.key_map), std::end(state.key_map), 0);
            state.plr1.fire = state.plr1.p_fire = 0;
            if (!xr) SDL_Delay(8);
            return 1;
        }
    }
    if (xr && !paused) {
        auto t = xr->tracking();
        static bool menuHeld = false;
        bool opening = t.menu && !menuHeld;
        menuHeld = t.menu;
        if (opening)
            menuState(state, !overlay.menu);
        bool previous = t.previousWeapon && !previousHeld, next = t.nextWeapon && !nextHeld;
        previousHeld = t.previousWeapon;
        nextHeld = t.nextWeapon;
        bool selecting = t.use || t.fire || enterPressed;
        if (overlay.menu) {
            bool nav = std::abs(t.move.y) > .55f || std::abs(t.turn.y) > .55f;
            float axis = std::abs(t.move.y) > .55f ? t.move.y : t.turn.y;
            if ((nav && !navHeld) || upPressed || downPressed)
                overlay.selection =
                    (overlay.selection + (upPressed || (!downPressed && axis > 0) ? PauseCount-1 : 1)) % PauseCount;
            navHeld = nav;
            if (!opening && selecting && !selectHeld)
                menuAction(state);
            selectHeld = selecting;
        } else {
            navHeld = false;
            selectHeld = selecting;
            // XR snapshots can be shorter than a 50Hz simulation tick. Keep
            // one trigger edge through cooldown; never turn it into autofire.
            if(!suppressActions && t.fire && !fireHeld)pendingFireClick=true;
            fireHeld=t.fire;
            if(!t.valid[1])pendingFireClick=false;
            if (!t.use && !t.fire)
                suppressActions = false;
            if (std::hypot(t.move.x, t.move.y) < .2f)
                requireNeutral = false;
            if (previous) {
                pendingFireClick=false;
                pendingWeapons = std::max(-8, pendingWeapons - 1);
                ++previousActions;
            }
            if (next) {
                pendingFireClick=false;
                pendingWeapons = std::min(8, pendingWeapons + 1);
                ++nextActions;
            }
            state.plr1_control.keys = true;
            state.plr1_control.mouse_kbd = false;
            state.plr1_control.mouse = false;
            state.plr1_control.joy = false;
            // Legacy key fields now only supply footsteps/trigger state. Actual
            // analog movement is injected at the original acceleration boundary.
            state.key_map[0x4c] = t.move.y > .25f;
            state.key_map[0x4d] = t.move.y < -.25f;
            state.key_map[0x39] = t.move.x < -.25f;
            state.key_map[0x3a] = t.move.x > .25f;
            state.key_map[0x4f] = state.key_map[0x4e] = 0;
            state.key_map[0x65] = t.fire && !suppressActions;
            if (!suppressActions && t.use && !useHeld)
                pendingUse = true;
            useHeld = t.use;
            state.key_map[0x40] = !suppressActions && (t.use || pendingUse);
            if (overlay.smoothTurn) {
                float value =
                    std::abs(t.turn.x) > .2f ? std::copysign((std::abs(t.turn.x) - .2f) / .8f, t.turn.x) : 0;
                turnFraction += value * 90.f * dt * 8192.f / 360.f;
                int delta = int(turnFraction);
                turnFraction -= delta;
                state.plr1.s_angpos = (state.plr1.s_angpos + delta) & 8191;
                state.plr1.angpos = state.plr1.s_angpos;
            } else {
                if (std::abs(t.turn.x) > .65f && !snapHeld) {
                    state.plr1.s_angpos = (state.plr1.s_angpos + (t.turn.x > 0 ? 683 : -683)) & 8191;
                    state.plr1.angpos = state.plr1.s_angpos;
                    snapHeld = true;
                }
                if (std::abs(t.turn.x) < .25f)
                    snapHeld = false;
            }
        }
    } else if (overlay.menu && !paused) {
        if (upPressed || downPressed)
            overlay.selection = (overlay.selection + (upPressed ? PauseCount-1 : 1)) % PauseCount;
        if (enterPressed)
            menuAction(state);
    }
    if (paused || overlay.menu || (xr && xr->focused() && !xr->tracking().headValid)) {
        std::fill(std::begin(state.key_map), std::end(state.key_map), 0);
        state.plr1.fire = state.plr1.p_fire = 0;
        pendingUse = useHeld = false;
        lastMovement = {};
        if (!xr)
            SDL_Delay(8);
        return 1;
    }
    return 0;
}
extern "C" int abxr_active_controls(const void *data,const void *player){
    const auto &state=*static_cast<const GameState*>(data);
    return xr&&!paused&&!overlay.menu&&xr->focused()&&player==&state.plr1;
}
extern "C" int abxr_crouch_height(const void *data, const void *player) {
    auto &state = *static_cast<const GameState *>(data);
    if (!xr || paused || player != &state.plr1 || !xr->focused() || !xr->tracking().headValid)
        return -1;
    auto &t = xr->tracking();
    physicalTarget = trackedBodyHeight(t.headPosition.y, t.anchor.y);
    minPhysicalTarget = std::min(minPhysicalTarget, physicalTarget);
    return int(std::lround(physicalTarget * 8192));
}
extern "C" int abxr_weapon_steps(const void *data, const void *player) {
    auto &state = *static_cast<const GameState *>(data);
    if (!xr || paused || player != &state.plr1)
        return INT_MIN;
    int steps = pendingWeapons;
    pendingWeapons = 0;
    return steps;
}
extern "C" void abxr_level_loaded(void *data) {
    auto &state = *static_cast<GameState *>(data);
    renderer.invalidateLevel();
    if(!report.empty())originalContent=auditOriginalContent(state);
    pendingWeapons = 0;
    pendingUse = false;
    pendingFireClick=false;
    requireNeutral = true;
    suppressActions = true;
    // Save reloads must resume directly; advancing a level restores its
    // original narrative before the first simulation update.
    if (presentationEnabled() && presentedLevel != state.current_level && loads == presentedLoads)
        presentPage(state, presentedLevel < 0 && !explicitLevel ? Presentation::Title : Presentation::Level);
    presentedLevel = state.current_level;
    presentedLoads = loads;
}
extern "C" void abxr_show_ending(void *data) {
    if (!presentationEnabled() || quitting) return;
    auto &state = *static_cast<GameState *>(data);
    presentPage(state, Presentation::Ending);
    while (!quitting && overlay.presentation != Presentation::Gameplay) {
        input_update(state.key_map, &state.last_pressed);
        abxr_poll(&state);
        abxr_present(&state);
    }
}
extern "C" int abxr_accelerate(void *data, void *player, int32_t *x, int32_t *z, int ticks, int height,
                               int runPercent) {
    auto &state = *static_cast<GameState *>(data);
    if (!xr || paused || player != &state.plr1)
        return 0;
    if (state.plr1.spctap)
        ++useActions;
    pendingUse = false; // original always-keys handler has consumed the Use edge
    const auto &tracking = xr->tracking();
    if (!xr->focused() || !tracking.headValid) {
        lastMovement = {};
        return 1;
    }
    lastMovement = headOrientedMovement(requireNeutral ? XrVector2f{} : tracking.move,
                                        tracking.headYawRadians, tracking.anchorYaw * 3.14159265f / 180,
                                        state.plr1.s_angpos * 6.283185307f / 8192);
    if (std::hypot(lastMovement.x, lastMovement.y) > .001f) {
        if (movementSamples++ == 0)
            firstMovement = lastMovement;
    }
    // Keep the original acceleration/friction, run/crouch speed, integration
    // and collision; replace only the direction with planar head-relative input.
    minEngineHeight = std::min(minEngineHeight, height / 8192.f);
    float scale =
        16384.f * (3.f * ticks) * std::clamp(height / float(PLAYER_HEIGHT), .5f, 1.f) * runPercent / 100.f;
    *x += int32_t(std::lround(lastMovement.x * scale));
    *z += int32_t(std::lround(lastMovement.y * scale));
    return 1;
}
extern "C" int abxr_aim_active(const void *data,const void *player){
    return shotOverride && player==&static_cast<const GameState*>(data)->plr1;
}
extern "C" int abxr_shot_direction(const void *data,const void *player,float *x,float *y,float *z){
    if(!abxr_aim_active(data,player))return 0;
    *x=shotDirection.x;*y=shotDirection.y;*z=shotDirection.z;return 1;
}
extern "C" void abxr_logic_tick(void *data){
    auto &s=*static_cast<GameState*>(data);
    if(!logicPasses)firstLogicMs=s.current_ticks_ms;
    ++logicPasses;simulationTicks+=s.temp_frames;
}
extern "C" void abxr_shot_fired(void *data,const void *player,int gun){
    auto &s=*static_cast<GameState*>(data);if(player!=&s.plr1)return;
    if(actualShots && gun==lastShotGun){
        auto delta=s.current_ticks_ms-lastShotMs;
        if(!minimumShotIntervalMs || delta<minimumShotIntervalMs)minimumShotIntervalMs=delta;
    }
    ++actualShots;lastShotMs=s.current_ticks_ms;lastShotGun=gun;
}
extern "C" void abxr_begin_shot(void *data) {
    if (!xr || paused || !xr->focused())
        return;
    auto &s = *static_cast<GameState *>(data);
    const auto t = xr->tracking();
    bool buffered=pendingFireClick;
    heldFireBackup=s.plr1.p_fire;clickedBackup=(s.plr1.p_clicked || buffered)?-1:0;shotInputSuppressed=true;
    s.plr1.p_fire=0;s.plr1.p_clicked=0;
    if (suppressActions || !t.valid[1] || (!t.fire && !clickedBackup))
        return;
    auto pose = t.weaponPose;
    if(!handInReach(pose,t.headPosition)){pendingFireClick=false;return;}
    auto socket=renderer.weapons.muzzlePose(s.plr1.gun_selected,s.plr1.gun_frame);
    auto origin=resolveShotOrigin(sceneGame(s),pose,t.anchor,t.anchorYaw*3.14159265f/180,
                                  socket.position,socket.direction,t.viewOffset);
    if(origin.clipped)++muzzleRetractions;
    s.plr1.p_fire=(heldFireBackup || buffered)?-1:0;s.plr1.p_clicked=clickedBackup;shotInputSuppressed=false;
    if(s.plr1.time_to_shoot==0)pendingFireClick=false;
    shotBackup = s.plr1;
    aimBackup = s.plr1_p_aim_speed;
    lookBackup = s.cfg_mouse_look;
    s.cfg_mouse_look = true;
    shotOverride = true;
    shotDirection=origin.direction;
    float ax=shotDirection.x,az=shotDirection.z;
    s.plr1.xoff = int32_t(origin.point.x * 65536);
    s.plr1.zoff = int32_t(origin.point.z * 65536);
    s.plr1.yoff = int32_t(origin.point.y);
    s.plr1.zone=(int16_t)origin.zone;
    auto room=level_get_zone_data_ptr(&s.level,s.plr1.zone);
    if(room){s.plr1.roompt=int32_t(room-s.level.data);s.plr1.stood_in_top=level_zone_has_upper_layer(&s.level,s.plr1.zone)&&s.plr1.yoff<obj_l(room+6);}
    s.plr1.angpos = abxr_engine_shot_yaw(ax,az);
    s.plr1.sinval = sin_lookup(s.plr1.angpos);
    s.plr1.cosval = cos_lookup(s.plr1.angpos);
    s.plr1_p_aim_speed =
        int16_t(std::clamp(-shotDirection.y / std::max(.1f, std::hypot(ax, az)) * 32768.f, -32760.f, 32760.f));
    ++shots;
}
extern "C" void abxr_end_shot(void *data) {
    if(shotInputSuppressed){
        auto &s=*static_cast<GameState*>(data);s.plr1.p_fire=heldFireBackup;s.plr1.p_clicked=0;shotInputSuppressed=false;
    }
    if (!shotOverride)
        return;
    auto &s = *static_cast<GameState *>(data);
    // Preserve shooting side effects (ammo, animation, cooldown), restore only pose.
    s.plr1.xoff = shotBackup.xoff;
    s.plr1.zoff = shotBackup.zoff;
    s.plr1.yoff = shotBackup.yoff;
    s.plr1.zone=shotBackup.zone;s.plr1.roompt=shotBackup.roompt;s.plr1.stood_in_top=shotBackup.stood_in_top;
    s.plr1.angpos = shotBackup.angpos;
    s.plr1.sinval = shotBackup.sinval;
    s.plr1.cosval = shotBackup.cosval;
    s.plr1_p_aim_speed = aimBackup;
    s.cfg_mouse_look = lookBackup;
    s.plr1.p_fire=heldFireBackup;s.plr1.p_clicked=0;
    shotOverride = false;
}
extern "C" void abxr_present(void *data) {
    auto &state = *static_cast<GameState *>(data);
    try {
        if (!initialized) {
#ifdef _WIN32
            glewExperimental = GL_TRUE;
            if (glewInit() != GLEW_OK) throw std::runtime_error("Cannot load OpenGL functions");
            glGetError();
#endif
            window = SDL_GL_GetCurrentWindow();
            if (!window)
                throw std::runtime_error("No native OpenGL window");
            SDL_SetWindowTitle(window, "Alien Breed XR");
            setWindowIcon(window);
            renderer.weapons.loadAll();
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
            if (!xr->frame(renderer, sceneGame(state))) {
                suspend();
            }
            else if(xr->tracking().headPushback>.0001f){
                ++headPushbackFrames;maximumHeadPushback=std::max(maximumHeadPushback,xr->tracking().headPushback);
            }
        }
        renderer.desktop(sceneGame(state), w, h);
        titleFrames += overlay.presentation == Presentation::Title;
        storyFrames += overlay.presentation == Presentation::Level;
        endingFrames += overlay.presentation == Presentation::Ending;
        if (previousGun >= 0 && previousGun != state.plr1.gun_selected)
            ++weaponChanges;
        previousGun = state.plr1.gun_selected;
        ++frames;
        if ((frameLimit && frames >= frameLimit) || quitting) {
            captureFrame(w, h);
            quitting = true;
            state.running = false;
        }
        SDL_GL_SwapWindow(window);
        if (!xr)
            SDL_Delay(1);
        if (!report.empty())
            std::ofstream(report)
                << nlohmann::json{{"frames", frames},
                                  {"original_content",originalContent},
                                  {"lighting_palette_uploads",renderer.lighting.paletteUploads},
                                  {"lighting_point_updates",renderer.lighting.pointUpdates},
                                  {"title_frames", titleFrames}, {"story_frames", storyFrames}, {"ending_frames", endingFrames},
                                  {"presentation", int(overlay.presentation)},
                                  {"xr_entries", entries},
                                  {"xr_suspends", suspends},
                                  {"controller_shots", shots},
                                  {"controller_aim_updates", shots},
                                  {"actual_shots_fired", actualShots},
                                  {"minimum_shot_interval_ms", minimumShotIntervalMs},
                                  {"logic_passes", logicPasses},
                                  {"simulation_vblanks", simulationTicks},
                                  {"simulation_elapsed_ms", state.current_ticks_ms-firstLogicMs},
                                  {"shot_billboards_rendered", renderer.shotBillboardsRendered},
                                  {"explosion_billboards_rendered", renderer.explosionBillboardsRendered},
                                  {"switch_panels_rendered", renderer.switchPanelsRendered},
                                  {"switch_panels_mounted", renderer.switchPanelsMounted},
                                  {"switch_mount_errors", renderer.switchMountErrors},
                                  {"vector_objects_rendered", renderer.vectorObjectsRendered},
                                  {"vector_meshes_uploaded", renderer.vectorMeshesUploaded},
                                  {"aim_laser_draws", renderer.laserDraws},
                                  {"damage_flash_draws", renderer.damageFlashDraws},
                                  {"weapon_models_loaded", renderer.weapons.loaded()},
                                  {"weapon_draws", renderer.weapons.draws},
                                  {"weapon_gpu_triangles", renderer.weapons.triangles},
                                  {"weapon_slots_drawn", renderer.weapons.slotsDrawn},
                                  {"weapon_sprite_frame", renderer.weapons.spriteFrame},
                                  {"head_pushback_frames",headPushbackFrames},
                                  {"head_pushback_m",xr?xr->tracking().headPushback:0},
                                  {"maximum_head_pushback_m",maximumHeadPushback},
                                  {"muzzle_retractions",muzzleRetractions},
                                  {"use_actions", useActions},
                                  {"weapon_previous_actions", previousActions},
                                  {"weapon_next_actions", nextActions},
                                  {"weapon", state.plr1.gun_selected},
                                  {"weapon_changes", weaponChanges},
                                  {"engine_height_min_m", minEngineHeight},
                                  {"physical_height_min_m", minPhysicalTarget},
                                  {"engine_height_m", state.plr1.s_height / 8192.f},
                                  {"menu_open", overlay.menu},
                                  {"menu_selection", overlay.selection},
                                  {"smooth_turn", overlay.smoothTurn},
                                  {"two_hand_enabled",overlay.twoHand},
                                  {"two_hand_changes",twoHandChanges},
                                  {"two_hand_active",xr?xr->tracking().twoHandActive:false},
                                  {"two_hand_grabs",xr?xr->tracking().twoHandGrabs:0},
                                  {"two_hand_releases",xr?xr->tracking().twoHandReleases:0},
                                  {"two_hand_frames",xr?xr->tracking().twoHandFrames:0},
                                  {"menu_opens", menuOpens},
                                  {"height_resets", heightResets},
                                  {"height_calibration_y", xr?xr->tracking().anchor.y:0},
                                  {"head_height_offset_m", xr?xr->tracking().headPosition.y-xr->tracking().anchor.y:0},
                                  {"weapon_name", gun_names[std::clamp(int(state.plr1.gun_selected),0,MAX_GUNS-1)]},
                                  {"flamethrower_enabled", state.cfg_flamethrower_weapon},
                                  {"selectable_weapon_mask", [&]{unsigned mask=0;for(int g=0;g<MAX_GUNS;++g)if(state.plr1.gun_data[g].visible)mask|=1u<<g;return mask;}()},
                                  {"saves", saves},
                                  {"loads", loads},
                                  {"status", overlay.message},
                                  {"movement_world_xz", {lastMovement.x, lastMovement.y}},
                                  {"movement_first_world_xz", {firstMovement.x, firstMovement.y}},
                                  {"movement_samples", movementSamples},
                                  {"player_position_xz",
                                   {state.plr1.xoff / 65536.f, state.plr1.zoff / 65536.f}},
                                  {"gpu_triangles", renderer.triangles},
                                  {"renderer", reinterpret_cast<const char *>(glGetString(GL_RENDERER))},
                                  {"video_driver", SDL_GetCurrentVideoDriver()},
                                  {"domed_managed", dome->managed()},
                                  {"paused", paused || overlay.menu},
                                  {"virtual_yaw_deg", state.plr1.angpos * 360.f / 8192},
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
