#include "render.hpp"
#include <SDL_opengl.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
namespace tempest {
namespace {
int w(const uint8_t *p) { return obj_w(p); }
int l(const uint8_t *p) { return obj_l(p); }
void point(const LevelState &level, int id, float y, float u = 0, float v = 0) {
    if (id < 0 || id >= w(level.data + 14))
        throw std::runtime_error("Invalid GPU mesh point");
    auto p = level.points + id * 4;
    glTexCoord2f(u, v);
    glVertex3f(w(p) / 32.f, y, w(p + 2) / 32.f);
}
} // namespace
unsigned Renderer::texture(int id, int rows) {
    int key = id * 1024 + rows;
    if (textures.count(key))
        return textures[key];
    const uint8_t *pixels = nullptr, *palette = nullptr;
    int width = 256, height = 256;
    if (id >= 0 && id < MAX_WALL_TILES) {
        pixels = g_renderer.walltiles[id];
        palette = g_renderer.wall_palettes[id];
        height = g_renderer.wall_valshift[id] ? 1 << g_renderer.wall_valshift[id] : rows;
        width = int(g_renderer.wall_pixel_bytes[id] / (height * 2)) * 3;
    } else if (id < 0) {
        pixels = g_renderer.floor_tile;
        palette = g_renderer.floor_pal ? g_renderer.floor_pal + 7 * 512 : nullptr;
        if (size_t(-id - 1) + 64764 >= g_renderer.floor_pixel_bytes)
            return 0;
        width = height = 64;
    }
    if (!pixels || !palette || width < 1 || width > 2048)
        return 0;
    std::vector<uint8_t> rgba(width * height * 4);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            // Walls store three 5-bit texels per 16-bit word, in column-major strips.
            unsigned index = id >= 0
                                 ? ((static_cast<unsigned short>(w(pixels + (x / 3) * height * 2 + y * 2)) >>
                                     ((x % 3) * 5)) &
                                    31)
                                 : pixels[-id - 1 + ((y << 8) | x) * 4];
            unsigned color = static_cast<unsigned short>(w(palette + index * 2));
            int at = (y * width + x) * 4;
            rgba[at] = ((color >> 8) & 15) * 17;
            rgba[at + 1] = ((color >> 4) & 15) * 17;
            rgba[at + 2] = (color & 15) * 17;
            rgba[at + 3] = 255;
        }
    unsigned tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    // SDL's 2D renderer leaves row strides/PBO bindings in the shared context.
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    return textures[key] = tex;
}
unsigned Renderer::sprite(int vect, int frame, int width, int height) {
    if (width < 1 || height < 1 || width > 256 || height > 256 || vect < 0 || vect >= MAX_SPRITE_TYPES ||
        frame < 0 || frame > 1024)
        return 0;
    unsigned long long key = (static_cast<unsigned long long>(vect) << 48) |
                             (static_cast<unsigned long long>(frame) << 32) | (width << 16) | height;
    if (sprites.count(key))
        return sprites[key];
    std::vector<uint8_t> rgba(width * height * 4);
    if (!renderer_xr_decode_sprite(vect, frame, width, height, rgba.data()))
        return 0;
    unsigned tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    // SDL's 2D renderer leaves row strides/PBO bindings in the shared context.
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return sprites[key] = tex;
}
void Renderer::clear() {
    for (auto [key, list] : polygons)
        glDeleteLists(list, 1);
    polygons.clear();
    for (auto [key, texture] : textures)
        glDeleteTextures(1, &texture);
    textures.clear();
    if (fontTexture)
        glDeleteTextures(1, &fontTexture);
    fontTexture = 0;
    glDeleteTextures(2, digits);
    digits[0] = digits[1] = 0;
    glDeleteTextures(4, cards);
    for (auto &card : cards)
        card = 0;
    for (auto [key, texture] : sprites)
        glDeleteTextures(1, &texture);
    sprites.clear();
}
void Renderer::world(const Game &game, bool immersive) {
    auto &state = *game.state;
    auto &level = state.level;
    auto &p = state.plr1;
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glPushMatrix();
    glRotatef(-p.angpos * 360.f / 8192, 0, 1, 0);
    glTranslatef(-p.xoff / 65536.f / 32, p.yoff / 8192.f - (immersive ? game.crouchCompensation : 0.f),
                 -p.zoff / 65536.f / 32);
    if (cachedLevel != state.current_level) {
        for (auto [key, list] : polygons)
            glDeleteLists(list, 1);
        polygons.clear();
        cachedLevel = state.current_level;
    }
    triangles = 0;
    for (int zone = 0; zone < level_zone_slot_count(&level); ++zone)
        for (int upper = 0; upper < 2; ++upper) {
            if (zone >= level.num_zone_graph_entries)
                continue;
            ZoneInfo info;
            if (level_get_zone_info(&level, zone, &info))
                continue;
            int offset = l(level.zone_graph_adds + zone * 8 + upper * 4);
            if (offset <= 0 || size_t(offset + 2) > level.graphics_byte_count)
                continue;
            const uint8_t *ptr = level.graphics + offset + 2,
                          *end = level.graphics + level.graphics_byte_count;
            float bright = std::clamp(1.f - level_get_zone_brightness(&level, zone, upper) / 32.f, .2f, 1.f);
            glColor3f(bright, bright, bright);
            for (int entries = 0; entries < 1000 && ptr + 2 <= end; ++entries) {
                int type = w(ptr);
                ptr += 2;
                if (type < 0)
                    break;
                if (type == 0 || type == 13) {
                    if (ptr + 28 > end)
                        break;
                    int a = w(ptr), b = w(ptr + 2), id = w(ptr + 12),
                        rows = std::clamp(int(ptr[14]) + 1, 1, 256);
                    // Some authored streams contain sentinel/unused wall points;
                    // the original renderer clips their unrotated zero entries.
                    if (a < 0 || b < 0 || a >= w(level.data + 14) || b >= w(level.data + 14)) {
                        ptr += 28;
                        continue;
                    }
                    float top = -l(ptr + 18) / 8192.f, bottom = -l(ptr + 22) / 8192.f;
                    auto tex = texture(id, rows);
                    if (tex) {
                        glEnable(GL_TEXTURE_2D);
                        glBindTexture(GL_TEXTURE_2D, tex);
                    } else
                        glDisable(GL_TEXTURE_2D);
                    float u1 = w(ptr + 4) / 64.f, u2 = w(ptr + 6) / 64.f;
                    glBegin(GL_QUADS);
                    point(level, a, top, u1, 0);
                    point(level, b, top, u2, 0);
                    point(level, b, bottom, u2, 1);
                    point(level, a, bottom, u1, 1);
                    glEnd();
                    triangles += 2;
                    ptr += 28;
                } else if (type == 1 || type == 2 || (type >= 7 && type <= 11)) {
                    if (ptr + 4 > end)
                        break;
                    int height = w(ptr), count = w(ptr + 2) + 1;
                    ptr += 4;
                    if (count < 3 || count > 100 || ptr + count * 2 + 8 > end)
                        break;
                    float y = -height / 128.f;
                    if (type == 1)
                        y = -(upper ? info.upper_floor_y : info.floor_y) / 8192.f;
                    if (type == 2)
                        y = -(upper ? info.upper_roof_y : info.roof_y) / 8192.f;
                    int tile = w(ptr + count * 2 + 4);
                    auto tex = texture(tile >= 0 ? -1 - tile : -1);
                    if (tex) {
                        glEnable(GL_TEXTURE_2D);
                        glBindTexture(GL_TEXTURE_2D, tex);
                    } else
                        glDisable(GL_TEXTURE_2D);
                    // GLU tessellates concave authored polygons, instead of a triangle fan.
                    floor(level, ptr, count, y);
                    triangles += count - 2;
                    ptr += count * 2 + 8;
                } else if (type == 4) {
                    if (ptr + 2 > end)
                        break;
                    ptr += 2;
                } else if (type == 5) {
                    if (ptr + 28 > end)
                        break;
                    ptr += 28;
                } else if (type == 6) {
                    if (ptr + 4 > end)
                        break;
                    ptr += 4;
                } else if (type != 3 && type != 12)
                    break;
            }
        }
    glDisable(GL_TEXTURE_2D);
    // Decode each original sprite frame once, upload it, then depth-test the
    // GPU billboard in the same 3D coordinate system as walls and both eyes.
    if (level.object_data && level.object_points)
        for (int i = 0; i < level.num_object_points; ++i) {
            auto obj = reinterpret_cast<GameObject *>(level.object_data + i * OBJECT_SIZE);
            if (OBJ_CID(obj) < 0)
                break;
            if (OBJ_ZONE(obj) < 0 || obj->obj.number < 0 ||
                obj == reinterpret_cast<GameObject *>(level.plr1_obj))
                continue;
            int id = OBJ_CID(obj);
            if (id < 0 || id >= level.num_object_points)
                continue;
            auto pos = level.object_points + id * 8;
            float x = w(pos) / 32.f, z = w(pos + 4) / 32.f, y = -w(obj->raw + 4) / 64.f;
            int vect = w(obj->raw + 8), frame = w(obj->raw + 10);
            int cols = obj->raw[14] ? obj->raw[14] * 2 : 64, rows = obj->raw[15] ? obj->raw[15] * 2 : 64;
            auto tex = sprite(vect, frame, cols, rows);
            float half = std::max(4, int(obj->raw[6])) / 64.f, height = std::max(8, int(obj->raw[7])) / 32.f;
            float yaw = std::atan2(p.xoff / 65536.f / 32 - x, p.zoff / 65536.f / 32 - z),
                  rx = std::cos(yaw) * half, rz = -std::sin(yaw) * half;
            if (tex) {
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, tex);
                glEnable(GL_ALPHA_TEST);
                glAlphaFunc(GL_GREATER, .4f);
                glColor3f(1, 1, 1);
            } else {
                glDisable(GL_TEXTURE_2D);
                glColor3f(1, .25f, .12f);
            }
            glBegin(GL_QUADS);
            glTexCoord2f(0, 1);
            glVertex3f(x - rx, y, z - rz);
            glTexCoord2f(1, 1);
            glVertex3f(x + rx, y, z + rz);
            glTexCoord2f(1, 0);
            glVertex3f(x + rx, y + height, z + rz);
            glTexCoord2f(0, 0);
            glVertex3f(x - rx, y + height, z - rz);
            glEnd();
            glDisable(GL_ALPHA_TEST);
        }
    glDisable(GL_TEXTURE_2D);
    glPopMatrix();
}
void Renderer::desktop(const Game &game, int width, int height) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glClearColor(.025f, .035f, .06f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    double aspect = double(width) / std::max(1, height);
    glFrustum(-.05 * aspect, .05 * aspect, -.05, .05, .05, 500);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    world(game, false);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1, 1, -.75, .75, .05, 10);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    finishFrame(game, 0, 0, 0, width, height);
}
} // namespace tempest
#include <GL/glu.h>
namespace tempest {
namespace {
void begin(GLenum type) { glBegin(type); }
void end() { glEnd(); }
void vertex(void *v) {
    auto p = static_cast<GLdouble *>(v);
    glTexCoord2d(p[0] / 2, p[2] / 2);
    glVertex3dv(p);
}
} // namespace
void polygon(const LevelState &level, const uint8_t *indices, int count, float y) {
    GLUtesselator *tess = gluNewTess();
    if (!tess)
        return;
    gluTessCallback(tess, GLU_TESS_BEGIN, reinterpret_cast<void (*)()>(begin));
    gluTessCallback(tess, GLU_TESS_END, reinterpret_cast<void (*)()>(end));
    gluTessCallback(tess, GLU_TESS_VERTEX, reinterpret_cast<void (*)()>(vertex));
    std::vector<std::array<GLdouble, 3>> points(count);
    for (int i = 0; i < count; ++i) {
        int id = w(indices + i * 2);
        if (id < 0 || id >= w(level.data + 14)) {
            gluDeleteTess(tess);
            return;
        }
        auto p = level.points + id * 4;
        points[i] = {w(p) / 32., y, w(p + 2) / 32.};
    }
    gluTessBeginPolygon(tess, nullptr);
    gluTessBeginContour(tess);
    for (auto &v : points)
        gluTessVertex(tess, v.data(), v.data());
    gluTessEndContour(tess);
    gluTessEndPolygon(tess);
    gluDeleteTess(tess);
}
} // namespace tempest

namespace tempest {
void Renderer::floor(const LevelState &level, const uint8_t *indices, int count, float y) {
    auto found = polygons.find(indices);
    if (found == polygons.end()) {
        unsigned list = glGenLists(1);
        if (!list)
            throw std::runtime_error("Cannot allocate GPU floor mesh");
        glNewList(list, GL_COMPILE);
        polygon(level, indices, count, 0);
        glEndList();
        found = polygons.emplace(indices, list).first;
    }
    glPushMatrix();
    glTranslatef(0, y, 0);
    glCallList(found->second);
    glPopMatrix();
}
} // namespace tempest

namespace tempest {
void Renderer::invalidateLevel() {
    for (auto [key, list] : polygons)
        glDeleteLists(list, 1);
    polygons.clear();
    cachedLevel = -1;
}
} // namespace tempest
