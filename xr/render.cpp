#include "render.hpp"
#include <SDL_opengl.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
extern "C" {
#include "renderer_3dobj.h"
}
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
        return sprites[key]=0;
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
unsigned Renderer::wallTexture(int id,int rows,int shift,int mask,int offset,bool holes){
    if(id<0 || id>=MAX_WALL_TILES || !g_renderer.walltiles[id] || !g_renderer.wall_palettes[id])return 0;
    shift=std::clamp(shift,0,8);rows=std::clamp(rows,1,256);mask=std::clamp(mask,0,1023);
    std::array<int,6> key{id,rows,shift,mask,offset,int(holes)};
    if(auto found=wallTextures.find(key);found!=wallTextures.end())return found->second;
    int width=mask+1,stride=1<<shift;
    std::vector<uint8_t> pixels(width*rows*4);
    for(int y=0;y<rows;++y)for(int x=0;x<width;++x){
        int column=x+offset;size_t byte=(size_t)(std::max(0,column)/3)*stride*2+y*2;
        if(column<0 || byte+2>g_renderer.wall_pixel_bytes[id])continue;
        int index=(uint16_t(w(g_renderer.walltiles[id]+byte))>>((column%3)*5))&31;
        unsigned color=uint16_t(w(g_renderer.wall_palettes[id]+index*2));
        auto p=pixels.data()+(y*width+x)*4;
        p[0]=((color>>8)&15)*17;p[1]=((color>>4)&15)*17;p[2]=(color&15)*17;
        p[3]=(holes && !index)?0:255;
    }
    unsigned tex;glGenTextures(1,&tex);glBindTexture(GL_TEXTURE_2D,tex);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH,0);glPixelStorei(GL_UNPACK_SKIP_ROWS,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,rows,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    return wallTextures[key]=tex;
}
void Renderer::clear() {
    weapons.clear();
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
    for(auto [key,list]:vectorMeshes)glDeleteLists(list,1);
    vectorMeshes.clear();
    for(auto [key,tex]:vectorTextures)glDeleteTextures(1,&tex);
    vectorTextures.clear();
    for(auto [key,tex]:wallTextures)glDeleteTextures(1,&tex);
    wallTextures.clear();
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
    glScalef(1,1,-1); // original +Z view depth -> OpenGL -Z
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
                    int shift=ptr[15],mask=w(ptr+16),from=w(ptr+8)*16;
                    if(!shift && id>=0 && id<MAX_WALL_TILES){shift=g_renderer.wall_valshift[id];rows=g_renderer.wall_valand[id]+1;}
                    auto tex = wallTexture(id,rows,shift,mask,from,type==13);
                    if (tex) {
                        glEnable(GL_TEXTURE_2D);
                        glBindTexture(GL_TEXTURE_2D, tex);
                    } else
                        glDisable(GL_TEXTURE_2D);
                    float period=std::clamp(mask,0,1023)+1.f;
                    float u1=w(ptr+4)/period,u2=w(ptr+6)/period;
                    float v1=(w(ptr+10)&(rows-1))/float(rows);
                    float texHeight=std::max(1.f,(l(ptr+22)-l(ptr+18))/256.f);
                    if(id==11)texHeight=rows;
                    else if(rows<64 && texHeight<64)texHeight=64;
                    float v2=v1+texHeight/rows;
                    if(type==13){glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.4f);}
                    glBegin(GL_QUADS);
                    point(level, a, top, u1, v1);
                    point(level, b, top, u2, v1);
                    point(level, b, bottom, u2, v2);
                    point(level, a, bottom, u1, v2);
                    glEnd();
                    if(type==13)glDisable(GL_ALPHA_TEST);
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
            if (OBJ_ZONE(obj) < 0 ||
                obj == reinterpret_cast<GameObject *>(level.plr1_obj))
                continue;
            int id = OBJ_CID(obj);
            if (id < 0 || id >= level.num_object_points)
                continue;
            auto pos = level.object_points + id * 8;
            float x = w(pos) / 32.f, z = w(pos + 4) / 32.f;
            if(obj->raw[6]==OBJ_3D_SPRITE){vectorObject(obj,x,z);continue;}
            XrBillboardInfo info{};
            if(!renderer_xr_billboard_info(obj,&info))continue;
            billboard(game,info,x,z);
        }
    // Projectiles, impact puffs, gibs and blast particles occupy separate pools
    // in the original engine. They are not entries in level.object_data.
    for(int pool=0;pool<2;++pool){
        const uint8_t *data=pool?level.nasty_shot_data:level.player_shot_data;
        int count=pool?NASTY_SHOT_SLOT_COUNT:PLAYER_SHOT_SLOT_COUNT;
        if(!data || !level.object_points)continue;
        for(int i=0;i<count;++i){
            auto object=reinterpret_cast<const GameObject*>(data+i*OBJECT_SIZE);
            int id=OBJ_CID(object);
            if(OBJ_ZONE(object)<0 || object->obj.number!=OBJ_NBR_BULLET || id<0 || id>=level.num_object_points)continue;
            XrBillboardInfo info{};if(!renderer_xr_shot_info(object,&info))continue;
            auto point=level.object_points+id*8;
            if(billboard(game,info,w(point)/32.f,w(point+4)/32.f))++shotBillboardsRendered;
        }
    }
    for(int i=0;i<state.num_explosions;++i){
        XrBillboardInfo info{};if(!renderer_xr_explosion_info(&state,i,&info))continue;
        if(billboard(game,info,state.explosions[i].x/32.f,state.explosions[i].z/32.f))++explosionBillboardsRendered;
    }
    glDisable(GL_TEXTURE_2D);
    glPopMatrix();
}
unsigned Renderer::vectorTexture(int map,bool holes){
    int key=map+(holes?65536:0);
    if(auto found=vectorTextures.find(key);found!=vectorTextures.end())return found->second;
    std::array<uint8_t,64*64*4> pixels{};
    if(!poly_obj_xr_texture(uint16_t(map),holes,pixels.data()))return 0;
    unsigned tex;glGenTextures(1,&tex);glBindTexture(GL_TEXTURE_2D,tex);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH,0);glPixelStorei(GL_UNPACK_SKIP_ROWS,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    return vectorTextures[key]=tex;
}
void Renderer::vectorObject(const GameObject *obj,float x,float z){
    int slot=w(obj->raw+8);
    if(slot<0 || slot>=POLY_OBJECTS_COUNT)return;
    const auto &mesh=g_poly_objects[slot];
    int frame=w(obj->raw+10);if(frame<0 || frame>=mesh.num_frames)frame=0;
    if(!mesh.data || mesh.num_frames<1 || mesh.num_points<1)return;
    size_t pointOffset=mesh.frame_off[frame];
    if(pointOffset+mesh.num_points*6>mesh.size)return;
    int key=slot*MAX_POLY_FRAMES+frame;
    auto found=vectorMeshes.find(key);
    if(found==vectorMeshes.end()){
        struct Face { unsigned texture;std::vector<std::array<float,5>> points; };
        std::vector<Face> faces;
        for(int part=0;part<mesh.num_parts;++part){
            size_t offset=mesh.part_poly_off[part];
            for(int count=0;count<1024 && offset+2<=mesh.size;++count){
                const uint8_t *p=mesh.data+offset;int edges=w(p);
                if(edges<0)break;
                size_t size=18+edges*4;
                if(offset+size>mesh.size || edges<2 || edges>7)break;
                Face face{vectorTexture(uint16_t(w(p+12+edges*4)),(w(p+2)&255)!=0),{}};
                bool valid=true;
                for(int i=0;i<=edges;++i){
                    const uint8_t *v=p+4+i*4;int id=w(v);
                    if(id<0 || id>=mesh.num_points){valid=false;break;}
                    const uint8_t *position=mesh.data+pointOffset+id*6;
                    face.points.push_back({w(position)/32.f,-w(position+2)/32.f,w(position+4)/32.f,v[2]/64.f,v[3]/64.f});
                }
                if(valid)faces.push_back(std::move(face));
                offset+=size;
            }
        }
        unsigned list=glGenLists(1);if(!list)return;
        glNewList(list,GL_COMPILE);
        glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.4f);
        for(const auto &face:faces){
            if(face.texture){glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,face.texture);}else glDisable(GL_TEXTURE_2D);
            glBegin(GL_TRIANGLE_FAN);
            for(auto v:face.points){glTexCoord2f(v[3],v[4]);glVertex3f(v[0],v[1],v[2]);}
            glEnd();
        }
        glDisable(GL_ALPHA_TEST);glEndList();
        found=vectorMeshes.emplace(key,list).first;++vectorMeshesUploaded;
    }
    glPushMatrix();glTranslatef(x,-w(obj->raw+4)/64.f,z);
    glRotatef(w(obj->raw+30)*360.f/8192.f-180.f,0,1,0);
    float light=std::clamp(1.f-w(obj->raw+2)/32.f,.2f,1.f);glColor3f(light,light,light);
    glCallList(found->second);glPopMatrix();++vectorObjectsRendered;
}
bool Renderer::billboard(const Game &game,const XrBillboardInfo &info,float x,float z){
    auto &p=game.state->plr1;
    float half=info.width/64.f,height=info.height/32.f;
    // ObjDraw3's authored Y is the centre, not the feet. Preserve the
    // centre of flying objects too; do not force every sprite to a floor.
    float y=-info.center_y/8192.f-height*.5f;
    auto tex = sprite(info.vector,info.frame,info.columns,info.rows);
    float yaw = std::atan2(p.xoff / 65536.f / 32 - x, p.zoff / 65536.f / 32 - z),
          rx = -std::cos(yaw) * half, rz = std::sin(yaw) * half;
    if (tex) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, tex);
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GREATER, .4f);
        float distance=std::hypot(p.xoff/65536.f-x*32,p.zoff/65536.f-z*32);
        float light=std::clamp(1.f-(info.brightness+distance/128.f)/64.f,.2f,1.f);
        glColor3f(light,light,light);
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
    return tex!=0;
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
