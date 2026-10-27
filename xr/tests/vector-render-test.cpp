/* Exercise the production GPU vector-object renderer with all original
 * models/materials. No headset, software rasterizer or synthetic substitute. */
#include "render.hpp"
extern "C" {
#include "renderer_3dobj.h"
}
#include <SDL.h>
#include <SDL_opengl.h>
#include <cassert>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
extern "C" int ab3d_log_printf(const char *format,...){
    va_list args;va_start(args,format);int result=vprintf(format,args);va_end(args);return result;
}
static std::vector<uint8_t> read(const std::filesystem::path &path){
    std::ifstream file(path,std::ios::binary);assert(file);
    return {std::istreambuf_iterator<char>(file),{}};
}
int main(int argc,char **argv){
    assert(argc==3);std::filesystem::path data=argv[1],out=argv[2];std::filesystem::create_directories(out);
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
    auto window=SDL_CreateWindow("Original polygon GPU check",0,0,512,512,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    assert(window);auto context=SDL_GL_CreateContext(window);assert(context);
    auto maps=read(data/"texturemaps/TextureMaps"),palette=read(data/"texturemaps/OldTexturePalScaled");
    poly_obj_set_texture_assets(maps.data(),maps.size(),palette.data(),palette.size());
    const char *names[]={"Robot","MediPac","ExitSign","Crate","Terminal","BlueInd","GreenInd","RedInd","YellowInd","GasPipe"};
    std::vector<uint8_t> assets[10];
    tempest::Renderer renderer;
    for(int slot=0;slot<10;++slot){
        assets[slot]=read(data/"vectorobjects"/(std::string(names[slot])+".vec"));
        assert(poly_obj_load(slot,assets[slot].data(),assets[slot].size()));
        const auto &mesh=g_poly_objects[slot];
        for(int frame=0;frame<mesh.num_frames;++frame){
            float radius=1;
            for(int p=0;p<mesh.num_points;++p)for(int axis=0;axis<3;++axis)
                radius=std::max(radius,std::abs(obj_w(mesh.data+mesh.frame_off[frame]+p*6+axis*2))/32.f);
            glViewport(0,0,512,512);glClearColor(.02,.03,.05,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            glEnable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);
            glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(-radius*1.8,radius*1.8,-radius*1.8,radius*1.8,.1,1000);
            glMatrixMode(GL_MODELVIEW);glLoadIdentity();glTranslatef(0,0,-radius*5);
            glRotatef(20,1,0,0);glRotatef(30,0,1,0);
            glScalef(1,1,-1); // production original-world to OpenGL conversion
            GameObject object{};object.raw[6]=OBJ_3D_SPRITE;obj_sw(object.raw+8,slot);obj_sw(object.raw+10,frame);
            renderer.vectorObject(&object,0,0);assert(glGetError()==GL_NO_ERROR);
            std::vector<uint8_t> pixels(512*512*3);glPixelStorei(GL_PACK_ALIGNMENT,1);
            glReadPixels(0,0,512,512,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
            int visible=0;std::set<unsigned> colors;
            for(size_t i=0;i<pixels.size();i+=3){
                if(pixels[i]>20 || pixels[i+1]>20 || pixels[i+2]>20){
                    ++visible;colors.insert((unsigned(pixels[i])<<16)|(pixels[i+1]<<8)|pixels[i+2]);
                }
            }
            assert(visible>100 && colors.size()>1);
            if(frame==0){
                std::ofstream image(out/(std::string(names[slot])+".ppm"),std::ios::binary);image<<"P6\n512 512\n255\n";
                for(int y=511;y>=0;--y)image.write(reinterpret_cast<char*>(pixels.data()+y*512*3),512*3);
            }
        }
        printf("PASS %s: all %d original polygon frame(s), visible textured GPU pixels\n",names[slot],mesh.num_frames);
    }
    assert(renderer.vectorMeshesUploaded>=10);renderer.clear();
    SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
}
