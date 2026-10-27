#include "render.hpp"
#include "stb_image.h"
#include <SDL.h>
#include <SDL_opengl.h>
#include <algorithm>

namespace tempest {
namespace {
unsigned upload(const unsigned char *rgba, int w, int h) {
    unsigned texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return texture;
}
unsigned image(const char *name, int &w, int &h) {
    char *base = SDL_GetBasePath();
    std::string path = std::string(base ? base : "") + name;
    SDL_free(base);
    int channels;
    auto pixels = stbi_load(path.c_str(), &w, &h, &channels, 4);
    if (!pixels)
        return 0;
    auto texture = upload(pixels, w, h);
    stbi_image_free(pixels);
    return texture;
}
void quad(unsigned texture, float x, float y, float width, float height, float u0 = 0, float v0 = 0,
          float u1 = 1, float v1 = 1) {
    if (texture) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texture);
    } else
        glDisable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    glTexCoord2f(u0, v1);
    glVertex3f(x, y, -1.8f);
    glTexCoord2f(u1, v1);
    glVertex3f(x + width, y, -1.8f);
    glTexCoord2f(u1, v0);
    glVertex3f(x + width, y + height, -1.8f);
    glTexCoord2f(u0, v0);
    glVertex3f(x, y + height, -1.8f);
    glEnd();
}
} // namespace
void Renderer::loadOverlay() {
    if (!fontTexture)
        fontTexture = image("fonts/ascii_font/ascii_font_green_black_atlas_v2.png", fontWidth, fontHeight);
    if (!digits[0])
        digits[0] = image("fonts/health_digits.png", digitWidth[0], digitHeight[0]);
    if (!digits[1])
        digits[1] = image("fonts/ammo_digits.png", digitWidth[1], digitHeight[1]);
    for (int frame = 0; frame < 4; ++frame)
        if (!cards[frame]) {
            uint32_t argb[32 * 32]{};
            if (!renderer_key_sprite_rasterize_frame_argb(frame, argb, 32))
                continue;
            unsigned char rgba[32 * 32 * 4];
            for (unsigned i = 0; i < 32 * 32; ++i) {
                rgba[i * 4] = (argb[i] >> 16) & 255;
                rgba[i * 4 + 1] = (argb[i] >> 8) & 255;
                rgba[i * 4 + 2] = argb[i] & 255;
                rgba[i * 4 + 3] = argb[i] >> 24;
            }
            cards[frame] = upload(rgba, 32, 32);
        }
}
void Renderer::text(const std::string &value, float x, float y, float height) {
    if (!fontTexture || !fontWidth || !fontHeight)
        return;
    float width = height * 8 / 13;
    for (unsigned char ch : value) {
        if (ch < 32 || ch > 126)
            ch = '?';
        int glyph = ch - 32, col = glyph % 16, row = glyph / 16;
        quad(fontTexture, x, y, width, height, (col * 12 + 2.f) / fontWidth, (row * 17 + 2.f) / fontHeight,
             (col * 12 + 10.f) / fontWidth, (row * 17 + 15.f) / fontHeight);
        x += width;
    }
}
void Renderer::number(int kind, int value, float x, float y, float height) {
    if (!digits[kind] || !digitHeight[kind]) {
        text(std::to_string(value), x, y, height);
        return;
    }
    int scale = std::max(1, digitHeight[kind] / 11);
    float width = height * 9 / 11;
    for (char ch : std::to_string(std::max(0, value))) {
        int digit = ch - '0';
        quad(digits[kind], x, y, width, height, digit * 11.f * scale / digitWidth[kind], 0,
             (digit * 11 + 9.f) * scale / digitWidth[kind], 1);
        x += width + .005f;
    }
}
void Renderer::finishFrame(const Game &game, int, int, int, int, int) {
    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_TEXTURE_BIT);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    loadOverlay();
    const auto &s = *game.state;
    const auto &p = s.plr1;
    glColor4f(.01f, .025f, .035f, .8f);
    quad(0, -.83f, -.68f, 1.66f, .19f);
    glColor4f(1, 1, 1, 1);
    text("VITALITY", -.79f, -.535f, .032f);
    text("AMMO", -.37f, -.535f, .032f);
    text("CARDS", .13f, -.535f, .032f);
    number(0, std::clamp(int(p.energy), 0, PLAYER_MAX_ENERGY) * 100 / PLAYER_MAX_ENERGY, -.79f, -.64f, .075f);
    int gun = std::clamp(int(p.gun_selected), 0, MAX_GUNS - 1);
    int ammo = std::max(0, int(p.gun_data[gun].ammo));
    if (gun != AB3D_GUN_FLAMETHROWER)
        ammo >>= 3;
    if (s.infinite_ammo)
        text("INF", -.37f, -.64f, .075f);
    else
        number(1, ammo, -.37f, -.64f, .075f);
    for (int bit = 0; bit < 4; ++bit) {
        int frame = std::clamp(renderer_key_sprite_frame_for_condition_bit(&s, bit), 0, 3);
        bool owned = (uint16_t(game_conditions) & (1u << bit)) != 0;
        glColor4f(1, 1, 1, owned ? 1.f : .18f);
        quad(cards[frame], .12f + bit * .15f, -.65f, .12f, .12f);
        if (owned) {
            glColor4f(.5f, 1, .6f, 1);
            quad(0, .12f + bit * .15f, -.66f, .12f, .008f);
        }
    }
    glColor4f(1, 1, 1, 1);
    if (game.overlay && !game.overlay->message.empty())
        text(game.overlay->message, -.8f, -.74f, .03f);
    if (game.overlay && game.overlay->menu) {
        glColor4f(.005f, .015f, .025f, .94f);
        quad(0, -.66f, -.38f, 1.32f, .91f);
        glColor4f(1, 1, 1, 1);
        text("ALIEN BREED - PAUSED", -.53f, .4f, .055f);
        const char *items[] = {"RESUME", game.overlay->smoothTurn ? "TURN: SMOOTH" : "TURN: SNAP",
                               "SAVE GAME", "LOAD GAME", "QUIT"};
        for (int i = 0; i < 5; ++i) {
            float y = .24f - i * .115f;
            if (i == game.overlay->selection) {
                glColor4f(.07f, .24f, .17f, .95f);
                quad(0, -.59f, y - .018f, 1.18f, .09f);
            }
            glColor4f(1, 1, 1, 1);
            text(i == game.overlay->selection ? std::string("> ") + items[i] : std::string("  ") + items[i],
                 -.54f, y, .055f);
        }
        text("STICK: SELECT  A / TRIGGER: ACTIVATE", -.58f, -.33f, .03f);
    }
    glPopAttrib();
}
} // namespace tempest
