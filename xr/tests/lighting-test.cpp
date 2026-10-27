/* Actual original palettes through the production GPU lookup, including
 * changing point-light animation and upper/lower sections. No headset. */
#include "lighting.hpp"
extern "C" {
#include "sb_decompress.h"
}
#include <SDL.h>
#include <SDL_opengl.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <vector>
static std::vector<uint8_t> load(const std::filesystem::path &path) {
    uint8_t *data=nullptr;size_t size=0;assert(sb_load_file(path.c_str(),&data,&size)==0);
    std::vector<uint8_t> result(data,data+size);free(data);return result;
}
int main(int argc,char **argv) {
    assert(argc==2);std::filesystem::path root=argv[1];
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
    auto window=SDL_CreateWindow("Original lighting GPU check",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    assert(window);auto context=SDL_GL_CreateContext(window);assert(context);
    glViewport(0,0,64,64);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_DITHER);
    glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(-1,1,-1,1,.01,100);
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();
    auto wall=load(root/"includes/walls/GreenMechanic.wad"),floor=load(root/"pal/FloorPalScaled"),sprite=load(root/"pal/Pickups.pal"),polygon=load(root/"texturemaps/OldTexturePalScaled");
    unsigned texture=0;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
    uint8_t pixel[]={12,0,0,255};glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    tempest::PaletteLighting lighting;using L=tempest::PaletteLighting;
    auto check=[&](const uint8_t *palette,size_t bytes,int colors,L::Material mode,float bright,float z,int bank,bool points=false,int upper=0) {
        glClear(GL_COLOR_BUFFER_BIT);glBindTexture(GL_TEXTURE_2D,texture);
        lighting.material(palette,bytes,colors,mode,bright,points,upper);
        glBegin(GL_QUADS);
        glTexCoord2f(0,0);glMultiTexCoord1f(GL_TEXTURE1,0);glVertex3f(-1,-1,-z);glVertex3f(1,-1,-z);glVertex3f(1,1,-z);glVertex3f(-1,1,-z);
        glEnd();uint8_t actual[4]{};glReadPixels(32,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,actual);
        unsigned color=uint16_t(obj_w(palette+(bank*colors+12)*2));
        assert(actual[0]==((color>>8)&15)*17 && actual[1]==((color>>4)&15)*17 && actual[2]==(color&15)*17);
        assert(glGetError()==GL_NO_ERROR);
    };
    check(wall.data(),2048,32,L::Wall,0,1,0);check(wall.data(),2048,32,L::Wall,8,1,8);
    check(wall.data(),2048,32,L::Wall,31,1,31);check(wall.data(),2048,32,L::Wall,0,20,3);
    check(floor.data(),floor.size(),256,L::Floor,0,1,3);check(floor.data(),floor.size(),256,L::Floor,8,1,7);
    check(floor.data(),floor.size(),256,L::Floor,30,1,14);check(floor.data(),floor.size(),256,L::Floor,-20,1,0);
    check(sprite.data(),sprite.size(),32,L::Sprite,0,1,0);check(sprite.data(),sprite.size(),32,L::Sprite,6,1,2);
    check(sprite.data(),sprite.size(),32,L::Sprite,54,1,14);check(sprite.data(),sprite.size(),32,L::Sprite,-128,20,0);
    glMultiTexCoord4f(GL_TEXTURE2,-.5f,-.5f,-1,1e9f);
    glMultiTexCoord3f(GL_TEXTURE3,.5f,-.5f,-1);glMultiTexCoord3f(GL_TEXTURE4,.5f,.5f,-1);
    glMultiTexCoord1f(GL_TEXTURE5,0);
    check(polygon.data(),polygon.size(),256,L::Polygon,0,1,4);
    glMultiTexCoord4f(GL_TEXTURE2,-.5f,-.5f,-1,1);
    check(polygon.data(),polygon.size(),256,L::Polygon,0,1,0);
    glMultiTexCoord1f(GL_TEXTURE5,1);
    check(polygon.data(),polygon.size(),256,L::Polygon,0,1,13);
    uint8_t header[16]{},point[4]={0xF1,0,0,5};
    LevelState level{};level.data=header;level.point_brights=point;level.bright_anim_values[0]=12;
    assert(level_point_count(&level)==1 && level_get_point_brightness(&level,0,0)==12);
    assert(level_get_point_brightness(&level,1,0)==0); // one past the last point
    lighting.updatePoints(level);check(wall.data(),2048,32,L::Wall,0,1,12,true);
    check(wall.data(),2048,32,L::Wall,0,1,5,true,1);
    level.bright_anim_values[0]=-10;lighting.updatePoints(level);check(wall.data(),2048,32,L::Wall,0,1,0,true);
    assert(lighting.pointUpdates==2 && lighting.paletteUploads==4);
    lighting.clear();glDeleteTextures(1,&texture);SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
    puts("PASS original GPU wall/floor/sprite palettes, distance bands, emissive effects and animated upper/lower point brightness");
}
