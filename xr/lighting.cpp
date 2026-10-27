#include "lighting.hpp"
#include <SDL_opengl.h>
#include <algorithm>
#include <stdexcept>
#include <vector>
namespace tempest {
namespace {
void unpack() {
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
}
unsigned shader(GLenum kind,const char *source) {
    unsigned result=glCreateShader(kind);glShaderSource(result,1,&source,nullptr);glCompileShader(result);
    GLint ok=0;glGetShaderiv(result,GL_COMPILE_STATUS,&ok);
    if(!ok){char log[2048]{};glGetShaderInfoLog(result,sizeof(log),nullptr,log);glDeleteShader(result);throw std::runtime_error(std::string("Original lighting shader: ")+log);}
    return result;
}
void parameters() {
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
}
}
int PaletteLighting::location(const char *name) {
    auto found=uniforms.find(name);
    if(found!=uniforms.end())return found->second;
    return uniforms.emplace(name,glGetUniformLocation(program,name)).first->second;
}
void PaletteLighting::scalar(const char *name,float value) {
    int loc=location(name);auto found=values.find(loc);
    if(found!=values.end() && found->second==value)return;
    glUniform1f(loc,value);values[loc]=value;
}
void PaletteLighting::initialize() {
    if(program)return;
    const char *vertex=R"GLSL(#version 120
uniform sampler2D pointData;
uniform float pointCount, upper, brightness, perPoint, mode;
varying vec2 uv;
varying float light, depth, gouraud;
void main() {
    gl_Position=ftransform(); uv=gl_MultiTexCoord0.xy;
    depth=-(gl_ModelViewMatrix*gl_Vertex).z*32.0;
    light=brightness;
    gouraud=0.0;
    if(perPoint>0.5) {
        vec2 value=texture2D(pointData,vec2((gl_MultiTexCoord1.x+0.5)/pointCount,(upper+0.5)/2.0)).rg;
        light+=floor(value.r*255.0+0.5)*256.0+floor(value.g*255.0+0.5)-32768.0;
    }
    if(mode>2.5) {
        gouraud=gl_MultiTexCoord5.x;
        if(gouraud>0.5) {
            // ObjDraw3 boxbrights, before translating the object centre.
            float localZ=-(gl_ModelViewMatrix*vec4(gl_Vertex.xyz,0.0)).z*32.0;
            light=clamp(floor((localZ+20.0)/4.0),0.0,13.0);
        } else {
            // The original face shade uses projected area / authored divisor.
            // Evaluate it on the GPU in the reference 768x640 projection,
            // independent of eye resolution and headset field of view.
            vec3 a=(gl_ModelViewMatrix*vec4(gl_MultiTexCoord2.xyz,1.0)).xyz;
            vec3 b=(gl_ModelViewMatrix*vec4(gl_MultiTexCoord3.xyz,1.0)).xyz;
            vec3 c=(gl_ModelViewMatrix*vec4(gl_MultiTexCoord4.xyz,1.0)).xyz;
            vec2 pa=a.xy*vec2(512.0,1128.0)/max(0.03125,-a.z);
            vec2 pb=b.xy*vec2(512.0,1128.0)/max(0.03125,-b.z);
            vec2 pc=c.xy*vec2(512.0,1128.0)/max(0.03125,-c.z);
            float area=abs((pc.x-pb.x)*(pa.y-pb.y)-(pa.x-pb.x)*(pc.y-pb.y));
            light+=14.0-floor(area/(max(1.0,abs(gl_MultiTexCoord2.w))*8.0));
        }
    }
})GLSL";
    const char *fragment=R"GLSL(#version 120
uniform sampler2D indices, palette;
uniform float colors, levels, mode, maximum;
varying vec2 uv;
varying float light, depth, gouraud;
void main() {
    vec4 texel=texture2D(indices,uv);
    if(texel.a<0.5)discard;
    float bank;
    if(mode<0.5) { // Macros.i SCALE: 65 distance/light entries -> 32 palettes
        float raw=clamp(floor(depth/128.0)+light*2.0,0.0,maximum);
        bank=min(31.0,floor((raw+1.0)/2.0));
    } else if(mode<1.5) { // AB3DI.s floorbright
        float raw=clamp(light+5.0+floor(depth/256.0),0.0,28.0);
        bank=floor((raw+1.0)/2.0);
    } else if(mode>2.5 && gouraud>0.5) {
        bank=floor(light);
    } else { // ObjDraw3.ChipRam.s objscalecols
        float raw=max(0.0,floor(depth/128.0)+light);
        bank=raw<2.0?0.0:min(14.0,floor((raw+2.0)/4.0));
    }
    bank=clamp(bank,0.0,levels-1.0);
    float index=floor(texel.r*255.0+0.5);
    gl_FragColor=texture2D(palette,vec2((index+0.5)/colors,(bank+0.5)/levels));
})GLSL";
    auto v=shader(GL_VERTEX_SHADER,vertex),f=shader(GL_FRAGMENT_SHADER,fragment);
    program=glCreateProgram();glAttachShader(program,v);glAttachShader(program,f);glLinkProgram(program);
    glDeleteShader(v);glDeleteShader(f);GLint ok=0;glGetProgramiv(program,GL_LINK_STATUS,&ok);
    if(!ok){char log[2048]{};glGetProgramInfoLog(program,sizeof(log),nullptr,log);glDeleteProgram(program);program=0;throw std::runtime_error(std::string("Original lighting link: ")+log);}
    glUseProgram(program);
    active=true;
    glUniform1i(location("indices"),0);
    glUniform1i(location("palette"),1);
    glUniform1i(location("pointData"),2);
}
void PaletteLighting::updatePoints(const LevelState &level) {
    initialize();
    std::array<int16_t,3> current{level.bright_anim_values[0],level.bright_anim_values[1],level.bright_anim_values[2]};
    if(pointSource==level.point_brights && current==animations && points)return;
    pointSource=level.point_brights;animations=current;pointCount=std::max(1,level_point_count(&level));
    std::vector<uint8_t> data(pointCount*2*4,0);
    for(int upper=0;upper<2;++upper)for(int i=0;i<pointCount;++i){
        unsigned value=int(level_get_point_brightness(&level,i,upper))+32768;
        auto p=data.data()+(upper*pointCount+i)*4;p[0]=(value>>8)&255;p[1]=value&255;p[3]=255;
    }
    if(!points)glGenTextures(1,&points);
    glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,points);unpack();parameters();
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,pointCount,2,0,GL_RGBA,GL_UNSIGNED_BYTE,data.data());
    glActiveTexture(GL_TEXTURE0);++pointUpdates;
}
void PaletteLighting::material(const uint8_t *palette,size_t bytes,int colors,Material mode,
                               float brightness,bool perPoint,int upper,int maximum) {
    initialize();
    int levels=int(bytes/(colors*2));
    if(!palette || levels<1){stop();return;}
    unsigned texture=0;
    if(auto found=palettes.find(palette);found!=palettes.end())texture=found->second;
    else {
        std::vector<uint8_t> pixels(colors*levels*4);
        for(int i=0;i<colors*levels;++i){unsigned color=uint16_t(obj_w(palette+i*2));auto p=pixels.data()+i*4;
            p[0]=((color>>8)&15)*17;p[1]=((color>>4)&15)*17;p[2]=(color&15)*17;p[3]=255;}
        glGenTextures(1,&texture);glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,texture);unpack();parameters();
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,colors,levels,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        palettes.emplace(palette,texture);++paletteUploads;
    }
    if(!active){glUseProgram(program);active=true;glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,points);}
    if(boundPalette!=texture){glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,texture);boundPalette=texture;}
    glActiveTexture(GL_TEXTURE0);
    scalar("colors",colors);scalar("levels",levels);scalar("mode",mode);scalar("maximum",maximum);
    scalar("brightness",brightness);scalar("perPoint",perPoint);scalar("upper",upper);scalar("pointCount",pointCount);
}
void PaletteLighting::stop() { glUseProgram(0);glActiveTexture(GL_TEXTURE0);active=false;boundPalette=0; }
void PaletteLighting::clear() {
    stop();if(program)glDeleteProgram(program);program=0;
    for(auto [key,texture]:palettes)glDeleteTextures(1,&texture);
    palettes.clear();uniforms.clear();values.clear();
    if(points)glDeleteTextures(1,&points);
    points=0;pointSource=nullptr;
}
}
