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
            weapons.draw(slot,remaining);assert(weapons.triangles>100 && weapons.triangles<2500);
            auto muzzle=weapons.muzzle(slot);assert(muzzle[2]<-.25f && muzzle[2]>-1 && std::abs(muzzle[0])<.01f);
            auto socket=weapons.muzzlePose(slot,remaining);
            float length=0;for(float value:socket.direction)length+=value*value;
            assert(std::abs(length-1)<.0001f);
            if(remaining==0)assert(std::abs(socket.position[2]-muzzle[2])<.0001f && socket.direction[2]<-.99f);
            if(slot==7 && remaining>0 && gun_anims[slot].frames[remaining]==1)assert(socket.direction[1]<-.3f);
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
        // Review the inferred surfaces in the real game renderer, including
        // the opposite side, underside, bores and the held aiming view.
        for(int view=0;view<4;++view){
            const char *names[]={"aim","opposite","underside","muzzle"};
            glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            glMatrixMode(GL_MODELVIEW);glLoadIdentity();
            if(view==0)glTranslatef(.14f,-.19f,-.38f);
            else{
                glTranslatef(0,-.025f,view==3?-1.5f:-.9f);
                glRotatef(view==3?180.f:55.f,0,1,0);
                glRotatef(view==2?155.f:12.f,1,0,0);
            }
            weapons.draw(slot,0);assert(glGetError()==GL_NO_ERROR);
            std::vector<unsigned char> pixels(640*480*3);
            glReadPixels(0,0,640,480,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
            std::ofstream file(out/(std::to_string(slot)+"-"+names[view]+".ppm"),std::ios::binary);file<<"P6\n640 480\n255\n";
            for(int y=479;y>=0;--y)file.write(reinterpret_cast<char*>(pixels.data()+y*640*3),640*3);
        }
    }
    assert(weapons.slotsDrawn==0x97);weapons.clear();SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
}
