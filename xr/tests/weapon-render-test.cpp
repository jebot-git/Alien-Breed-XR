#include "weapon.hpp"
#include "game.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <SDL.h>
#include <SDL_opengl.h>
#include <cassert>
#include <fstream>
#include <iostream>
#include <set>
#include <vector>
#include <filesystem>
int main(int argc,char **argv){
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    auto window=SDL_CreateWindow("Weapon GPU validation",0,0,640,480,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    assert(window);auto context=SDL_GL_CreateContext(window);assert(context);
    tempest::WeaponRenderer weapons;weapons.loadAll();assert(weapons.loaded()==5);
    std::filesystem::path out=argc>1?argv[1]:".";std::filesystem::create_directories(out);
    for(int slot:{0,1,2,4,7}){
        std::set<unsigned long long> hashes;
        for(int remaining=0;remaining<=gun_anims[slot].num_frames;++remaining){
            glViewport(0,0,640,480);glClearColor(.04,.05,.07,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            glMatrixMode(GL_PROJECTION);glLoadIdentity();glFrustum(-.08,.08,-.06,.06,.1,10);
            glMatrixMode(GL_MODELVIEW);glLoadIdentity();glTranslatef(0,-.04,-.8);glRotatef(-55,0,1,0);glRotatef(18,1,0,0);
            weapons.draw(slot,remaining);assert(weapons.triangles>100 && weapons.triangles<1200);
            auto muzzle=weapons.muzzle(slot);assert(muzzle[2]<-.25f && muzzle[2]>-1 && std::abs(muzzle[0])<.01f);
            assert(glGetError()==GL_NO_ERROR);
            std::vector<unsigned char> pixels(640*480*3);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,640,480,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
            unsigned long long hash=1469598103934665603ULL;unsigned visible=0;
            for(size_t i=0;i<pixels.size();i+=3){for(int k=0;k<3;++k){hash^=pixels[i+k];hash*=1099511628211ULL;}if(pixels[i]>30||pixels[i+1]>30||pixels[i+2]>30)++visible;}
            assert(visible>300);hashes.insert(hash);
            if(remaining==0||remaining==gun_anims[slot].num_frames){
                std::ofstream file(out/(std::to_string(slot)+(remaining?"-fire.ppm":"-rest.ppm")),std::ios::binary);file<<"P6\n640 480\n255\n";
                for(int y=479;y>=0;--y)file.write(reinterpret_cast<char*>(pixels.data()+y*640*3),640*3);
            }
        }
        assert(hashes.size()>1);std::cout<<"PASS weapon "<<slot<<" visible geometry and "<<hashes.size()<<" distinct GPU animation frames\n";
    }
    assert(weapons.slotsDrawn==0x97);weapons.clear();SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
}
