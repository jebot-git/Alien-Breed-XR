#include "weapon.hpp"
#include "game.hpp"
#include "stb_image.h"
#include <SDL.h>
#include <SDL_opengl.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace tempest {
namespace {
using Json=nlohmann::json;
constexpr const char *names[8]={"pistol","plasma","rocket",nullptr,"grenade",nullptr,nullptr,"shotgun"};
void require(bool test,const char *message) { if(!test)throw std::runtime_error(std::string("Weapon glTF: ")+message); }
uint32_t word(const std::vector<uint8_t>&b,size_t at) {
    require(at<=b.size() && b.size()-at>=4,"truncated GLB");
    return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;
}
struct Vertex { float pos[3],normal[3],uv[2]; };
struct Material { unsigned texture=0; bool blend=false; float color[4]={1,1,1,1}; float emission=0; };
struct Primitive { unsigned vbo=0,ebo=0; int count=0,material=0; };
struct Pose {
    std::array<float,3> translation{0,0,0},scale{1,1,1};
    std::array<float,4> rotation{0,0,0,1};
};
struct Node {
    Pose rest;
    std::vector<int> children;
    int mesh=-1;
    std::vector<float> matrix;
    std::string name;
};
struct Channel { int node=0,components=0;std::string path,interpolation;std::vector<float> times,values; };
struct Model {
    Json json;
    std::vector<uint8_t> binary;
    std::vector<unsigned> images;
    std::vector<Material> materials;
    std::vector<std::vector<Primitive>> meshes;
    std::vector<Node> nodes;
    std::vector<int> roots;
    std::vector<Channel> channels;
    std::vector<Pose> pose;
    std::array<float,3> muzzle{0,0,-.3f};
    int muzzleNode=-1;
    float duration=0;
    int sampledTick=std::numeric_limits<int>::min();
    ~Model() {
        for(auto &mesh:meshes)for(auto &primitive:mesh){
            glDeleteBuffers(1,&primitive.vbo);glDeleteBuffers(1,&primitive.ebo);
        }
        for(auto image:images)glDeleteTextures(1,&image);
    }
    std::pair<const uint8_t*,size_t> view(int index) const {
        const auto &v=json.at("bufferViews").at(index);
        require(v.value("buffer",0)==0,"external buffer is unsupported");
        size_t offset=v.value("byteOffset",size_t(0)),size=v.at("byteLength");
        require(offset<=binary.size() && size<=binary.size()-offset,"buffer view outside GLB");
        return {binary.data()+offset,size};
    }
    std::vector<float> accessor(int index,const char *type) const {
        const auto &a=json.at("accessors").at(index);
        require(!a.contains("sparse") && a.at("type")==type,"unsupported accessor layout");
        unsigned components=std::strcmp(type,"SCALAR")==0?1:std::strcmp(type,"VEC2")==0?2:std::strcmp(type,"VEC3")==0?3:4;
        unsigned format=a.at("componentType"),width=format==5126||format==5125?4:format==5123?2:format==5121?1:0;
        require(width && !a.value("normalized",false),"unsupported component encoding");
        auto data=view(a.at("bufferView"));size_t count=a.at("count"),offset=a.value("byteOffset",size_t(0));
        size_t stride=json.at("bufferViews").at(int(a.at("bufferView"))).value("byteStride",size_t(width*components));
        require(count>0 && count<1000000 && stride>=width*components && offset<=data.second,"invalid accessor size");
        require(width*components<=data.second-offset && (count-1)<=(data.second-offset-width*components)/stride,"accessor outside view");
        std::vector<float> result(count*components);
        for(size_t i=0;i<count;++i)for(unsigned k=0;k<components;++k){
            const auto *p=data.first+offset+i*stride+k*width;
            if(format==5126)std::memcpy(&result[i*components+k],p,4);
            else { unsigned n=0;for(unsigned b=0;b<width;++b)n|=unsigned(p[b])<<(8*b);result[i*components+k]=float(n); }
            require(std::isfinite(result[i*components+k]),"non-finite attribute");
        }
        return result;
    }
    void load(const std::string &path) {
        std::ifstream file(path,std::ios::binary);
        require(bool(file),("missing model "+path).c_str());
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});
        require(word(bytes,0)==0x46546c67 && word(bytes,4)==2 && word(bytes,8)==bytes.size(),"invalid GLB header");
        size_t length=word(bytes,12),end=20+length;
        require(word(bytes,16)==0x4e4f534a && end<=bytes.size(),"missing GLB JSON");
        json=Json::parse(bytes.begin()+20,bytes.begin()+end);
        require(word(bytes,end+4)==0x004e4942 && word(bytes,end)==bytes.size()-end-8,"missing GLB binary");
        binary.assign(bytes.begin()+end+8,bytes.end());
        require(!json.contains("skins") && !json.contains("extensionsRequired"),"skin/extension requires a different loader");
        for(const auto &image:json.at("images")) {
            auto data=view(image.at("bufferView"));int w,h,channels;
            auto pixels=stbi_load_from_memory(data.first,int(data.second),&w,&h,&channels,4);
            require(pixels && w>0 && h>0 && w<=4096 && h<=4096,"invalid embedded PNG");
            unsigned texture;glGenTextures(1,&texture);images.push_back(texture);glBindTexture(GL_TEXTURE_2D,texture);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
            glPixelStorei(GL_UNPACK_ROW_LENGTH,0);glPixelStorei(GL_UNPACK_SKIP_ROWS,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);stbi_image_free(pixels);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
        }
        for(const auto &source:json.at("materials")) {
            Material m;const auto &pbr=source.at("pbrMetallicRoughness");
            if(pbr.contains("baseColorTexture")) {
                int texture=pbr.at("baseColorTexture").at("index");m.texture=images.at(json.at("textures").at(texture).at("source"));
            }
            if(pbr.contains("baseColorFactor"))for(int i=0;i<4;++i)m.color[i]=pbr.at("baseColorFactor").at(i);
            m.blend=source.value("alphaMode",std::string("OPAQUE"))=="BLEND";
            if(source.contains("emissiveFactor"))for(auto v:source.at("emissiveFactor"))m.emission=std::max(m.emission,float(v));
            materials.push_back(m);
        }
        for(const auto &source:json.at("meshes")) {
            meshes.emplace_back();
            for(const auto &p:source.at("primitives")) {
                require(p.value("mode",4)==4,"only triangle primitives are supported");
                auto positions=accessor(p.at("attributes").at("POSITION"),"VEC3");
                auto normals=accessor(p.at("attributes").at("NORMAL"),"VEC3");
                auto uvs=accessor(p.at("attributes").at("TEXCOORD_0"),"VEC2");
                auto sourceIndices=accessor(p.at("indices"),"SCALAR");
                size_t count=positions.size()/3;require(normals.size()==positions.size() && uvs.size()==count*2,"attribute lengths differ");
                std::vector<Vertex> vertices(count);
                for(size_t i=0;i<count;++i) {
                    std::copy_n(positions.data()+i*3,3,vertices[i].pos);std::copy_n(normals.data()+i*3,3,vertices[i].normal);std::copy_n(uvs.data()+i*2,2,vertices[i].uv);
                }
                std::vector<uint32_t> indices;
                for(float index:sourceIndices){require(index>=0 && index<count && std::floor(index)==index,"invalid triangle index");indices.push_back(uint32_t(index));}
                require(indices.size()%3==0,"incomplete triangle");
                Primitive primitive;primitive.count=int(indices.size());primitive.material=p.at("material");require(primitive.material>=0 && size_t(primitive.material)<materials.size(),"invalid material index");
                glGenBuffers(1,&primitive.vbo);glGenBuffers(1,&primitive.ebo);
                glBindBuffer(GL_ARRAY_BUFFER,primitive.vbo);glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(Vertex),vertices.data(),GL_STATIC_DRAW);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,primitive.ebo);glBufferData(GL_ELEMENT_ARRAY_BUFFER,indices.size()*4,indices.data(),GL_STATIC_DRAW);
                meshes.back().push_back(primitive);
            }
        }
        glBindBuffer(GL_ARRAY_BUFFER,0);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
        for(const auto &source:json.at("nodes")) {
            Node n;n.mesh=source.value("mesh",-1);n.name=source.value("name",std::string());
            if(source.contains("translation"))n.rest.translation=source.at("translation").get<std::array<float,3>>();
            if(source.contains("rotation"))n.rest.rotation=source.at("rotation").get<std::array<float,4>>();
            if(source.contains("scale"))n.rest.scale=source.at("scale").get<std::array<float,3>>();
            if(source.contains("matrix"))n.matrix=source.at("matrix").get<std::vector<float>>();
            require(n.matrix.empty()||n.matrix.size()==16,"invalid node matrix");
            if(source.contains("children"))n.children=source.at("children").get<std::vector<int>>();
            if(n.name=="muzzle"){muzzle=n.rest.translation;muzzleNode=int(nodes.size());}
            nodes.push_back(n);
        }
        roots=json.at("scenes").at(json.value("scene",0)).at("nodes").get<std::vector<int>>();
        for(const auto &animation:json.at("animations"))if(animation.value("name",std::string())=="Fire") {
            for(const auto &source:animation.at("channels")) {
                Channel c;c.node=source.at("target").at("node");require(c.node>=0 && size_t(c.node)<nodes.size(),"invalid animated node");c.path=source.at("target").at("path");
                require(c.path=="translation"||c.path=="rotation"||c.path=="scale","unsupported animation target");
                const auto &sampler=animation.at("samplers").at(int(source.at("sampler")));
                c.interpolation=sampler.value("interpolation",std::string("LINEAR"));require(c.interpolation=="STEP"||c.interpolation=="LINEAR","unsupported interpolation");
                c.components=c.path=="rotation"?4:3;c.times=accessor(sampler.at("input"),"SCALAR");c.values=accessor(sampler.at("output"),c.components==4?"VEC4":"VEC3");
                require(c.values.size()==c.times.size()*c.components && std::is_sorted(c.times.begin(),c.times.end()),"invalid animation samples");
                duration=std::max(duration,c.times.back());channels.push_back(std::move(c));
            }
        }
        require(!channels.empty(),"missing Fire animation");
        for(auto &n:nodes)pose.push_back(n.rest);
        binary.clear();binary.shrink_to_fit();json=Json();
    }
    void sample(int remaining,int maxTicks) {
        if(remaining==sampledTick)return;
        sampledTick=remaining;for(size_t i=0;i<nodes.size();++i)pose[i]=nodes[i].rest;
        if(remaining<=0)return;
        float time=std::clamp((maxTicks-remaining)/50.f,0.f,duration);
        for(auto &c:channels) {
            auto upper=std::upper_bound(c.times.begin(),c.times.end(),time+1e-7f);
            size_t lo=upper==c.times.begin()?0:size_t(upper-c.times.begin()-1),hi=std::min(lo+1,c.times.size()-1);
            float fraction=c.interpolation=="STEP"||hi==lo?0:std::clamp((time-c.times[lo])/(c.times[hi]-c.times[lo]),0.f,1.f);
            auto &p=pose[c.node];float *out=c.path=="rotation"?p.rotation.data():c.path=="translation"?p.translation.data():p.scale.data();
            const auto *a=c.values.data()+lo*c.components,*b=c.values.data()+hi*c.components;
            float sign=1,wa=1-fraction,wb=fraction;
            if(c.components==4) {
                float dot=0;for(int i=0;i<4;++i)dot+=a[i]*b[i];if(dot<0){sign=-1;dot=-dot;}
                if(dot<.9995f){float theta=std::acos(std::clamp(dot,-1.f,1.f));wa=std::sin((1-fraction)*theta)/std::sin(theta);wb=std::sin(fraction*theta)/std::sin(theta);}
            }
            for(int i=0;i<c.components;++i)out[i]=wa*a[i]+wb*sign*b[i];
            if(c.components==4){float norm=0;for(int i=0;i<4;++i)norm+=out[i]*out[i];require(norm>0,"zero quaternion");for(int i=0;i<4;++i)out[i]/=std::sqrt(norm);}
        }
    }
    using Matrix=std::array<float,16>;
    static Matrix multiply(const Matrix &a,const Matrix &b){
        Matrix out{};
        for(int c=0;c<4;++c)for(int r=0;r<4;++r)for(int k=0;k<4;++k)out[c*4+r]+=a[k*4+r]*b[c*4+k];
        return out;
    }
    bool socketMatrix(int index,const Matrix &parent,Matrix &result,int depth=0)const{
        require(depth<128 && index>=0 && size_t(index)<nodes.size(),"invalid socket hierarchy");
        const auto &n=nodes[index];const auto &p=pose[index];auto q=p.rotation;
        float x=q[0],y=q[1],z=q[2],w=q[3];
        Matrix local{1-2*(y*y+z*z),2*(x*y+w*z),2*(x*z-w*y),0,
            2*(x*y-w*z),1-2*(x*x+z*z),2*(y*z+w*x),0,
            2*(x*z+w*y),2*(y*z-w*x),1-2*(x*x+y*y),0,
            p.translation[0],p.translation[1],p.translation[2],1};
        for(int c=0;c<3;++c)for(int r=0;r<3;++r)local[c*4+r]*=p.scale[c];
        if(!n.matrix.empty())std::copy_n(n.matrix.begin(),16,local.begin());
        auto world=multiply(parent,local);
        if(index==muzzleNode){result=world;return true;}
        for(int child:n.children)if(socketMatrix(child,world,result,depth+1))return true;
        return false;
    }
    WeaponSocket socket()const{
        Matrix identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},world{};
        for(int root:roots)if(socketMatrix(root,identity,world)){
            float length=std::sqrt(world[8]*world[8]+world[9]*world[9]+world[10]*world[10]);
            require(length>.00001f,"invalid muzzle direction");
            return {{world[12],world[13],world[14]},{-world[8]/length,-world[9]/length,-world[10]/length}};
        }
        throw std::runtime_error("Weapon glTF: missing muzzle socket");
    }
    unsigned long node(int index,bool transparent,int depth=0) {
        require(depth<128 && index>=0 && size_t(index)<nodes.size(),"invalid scene graph");
        auto &n=nodes[index];auto &p=pose[index];
        if(std::abs(p.scale[0]*p.scale[1]*p.scale[2])<1e-12f)return 0;
        glPushMatrix();
        if(!n.matrix.empty())glMultMatrixf(n.matrix.data());
        else {
            glTranslatef(p.translation[0],p.translation[1],p.translation[2]);auto q=p.rotation;float x=q[0],y=q[1],z=q[2],w=q[3];
            float matrix[]={1-2*(y*y+z*z),2*(x*y+w*z),2*(x*z-w*y),0,2*(x*y-w*z),1-2*(x*x+z*z),2*(y*z+w*x),0,2*(x*z+w*y),2*(y*z-w*x),1-2*(x*x+y*y),0,0,0,0,1};
            glMultMatrixf(matrix);glScalef(p.scale[0],p.scale[1],p.scale[2]);
        }
        unsigned long triangles=0;
        if(n.mesh>=0)for(auto &primitive:meshes.at(n.mesh)) {
            auto &m=materials[primitive.material];if(m.blend!=transparent)continue;
            if(m.blend){glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);glDisable(GL_LIGHTING);}
            else {glDisable(GL_BLEND);glDepthMask(GL_TRUE);glEnable(GL_LIGHTING);}
            if(m.texture){glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,m.texture);}else glDisable(GL_TEXTURE_2D);
            glColor4fv(m.color);float emit[]={m.emission*.18f,m.emission*.18f,m.emission*.18f,1};glMaterialfv(GL_FRONT_AND_BACK,GL_EMISSION,emit);
            glBindBuffer(GL_ARRAY_BUFFER,primitive.vbo);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,primitive.ebo);
            glVertexPointer(3,GL_FLOAT,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,pos)));
            glNormalPointer(GL_FLOAT,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,normal)));
            glTexCoordPointer(2,GL_FLOAT,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,uv)));
            glDrawElements(GL_TRIANGLES,primitive.count,GL_UNSIGNED_INT,nullptr);triangles+=primitive.count/3;
        }
        for(int child:n.children)triangles+=node(child,transparent,depth+1);
        glPopMatrix();return triangles;
    }
};
}
struct WeaponRenderer::Impl { std::array<std::unique_ptr<Model>,8> models; };
WeaponRenderer::WeaponRenderer():p(std::make_unique<Impl>()){}
WeaponRenderer::~WeaponRenderer()=default;
void WeaponRenderer::clear(){for(auto &model:p->models)model.reset();}
unsigned WeaponRenderer::loaded()const {unsigned result=0;for(auto &model:p->models)if(model)++result;return result;}
void WeaponRenderer::loadAll(){
    char *base=SDL_GetBasePath();std::string directory=std::string(base?base:"")+"data/weapons/";SDL_free(base);
    for(int slot=0;slot<8;++slot)if(names[slot]&&!p->models[slot]){
        auto model=std::make_unique<Model>();model->load(directory+names[slot]+".glb");p->models[slot]=std::move(model);
    }
    std::cerr<<"Weapon glTF: "<<loaded()<<" models uploaded to GPU\n";
}
std::array<float,3> WeaponRenderer::muzzle(int slot)const {
    require(slot>=0&&slot<8&&bool(p->models[slot]),"selected model is not loaded");return p->models[slot]->muzzle;
}
WeaponSocket WeaponRenderer::muzzlePose(int slot,int remainingTicks){
    require(slot>=0&&slot<8&&bool(p->models[slot]),"selected model is not loaded");
    auto &model=*p->models[slot];model.sample(remainingTicks,gun_anims[slot].num_frames);return model.socket();
}
void WeaponRenderer::draw(int slot,int remainingTicks){
    require(slot>=0&&slot<8&&bool(p->models[slot]),"selected model is not loaded");auto &model=*p->models[slot];
    model.sample(remainingTicks,gun_anims[slot].num_frames);
    spriteFrame=remainingTicks>0?gun_anims[slot].frames[std::clamp(remainingTicks,0,gun_anims[slot].num_frames)]:0;
    glPushAttrib(GL_ALL_ATTRIB_BITS);glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDisable(GL_CULL_FACE);glDisable(GL_FOG);
    glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.1f);glEnable(GL_NORMALIZE);glEnable(GL_COLOR_MATERIAL);glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
    // Soft fill preserves the original painted metal detail on inferred side
    // faces while a restrained key light still describes the 3D volume.
    glEnable(GL_LIGHT0);float ambient[]={.72f,.72f,.72f,1},diffuse[]={.5f,.5f,.5f,1},light[]={-.3f,.7f,.6f,0};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT,ambient);glLightfv(GL_LIGHT0,GL_DIFFUSE,diffuse);glLightfv(GL_LIGHT0,GL_POSITION,light);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    glEnableClientState(GL_VERTEX_ARRAY);glEnableClientState(GL_NORMAL_ARRAY);glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    triangles=0;for(int pass=0;pass<2;++pass)for(int root:model.roots)triangles+=model.node(root,pass!=0);
    glPopClientAttrib();glPopAttrib();++draws;slotsDrawn|=1u<<slot;
}
}
