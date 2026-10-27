#include "content_audit.hpp"
extern "C" {
#include "renderer_3dobj.h"
#include "sb_decompress.h"
}
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <vector>
namespace tempest {
nlohmann::json auditOriginalContent(const GameState &state) {
    using nlohmann::json;
    json result={{"objects",json::array()},{"placement_changes",0},{"unrenderable",0},
                 {"pickups",0},{"enemies",0},{"decorations",0},
                 {"doors",state.level.num_doors},{"lifts",state.level.num_lifts},{"switches",0}};
    if(state.level.switch_data)
        for(int i=0;i<8;++i)if(obj_w(state.level.switch_data+i*14)>=0)
            result["switches"]=result["switches"].get<int>()+1;
    char relative[80],path[512];
    snprintf(relative,sizeof(relative),"levels/level_%c/twolev.bin",'a'+state.current_level);
    io_make_data_path(path,sizeof(path),relative);
    uint8_t *raw=nullptr;size_t bytes=0;
    if(sb_load_file(path,&raw,&bytes) || !raw || bytes<54){result["error"]="Original level file unavailable";return result;}
    std::unique_ptr<uint8_t,decltype(&free)> original(raw,free);
    int objects=obj_l(raw+30),points=obj_l(raw+42);
    for(int i=0;i<256;++i){
        size_t offset=size_t(objects)+i*OBJECT_SIZE;
        if(offset+OBJECT_SIZE>bytes){result["error"]="Truncated original object table";break;}
        const uint8_t *source=raw+offset;
        int cid=obj_w(source);if(cid<0)break;
        const auto &obj=*reinterpret_cast<const GameObject*>(state.level.object_data+i*OBJECT_SIZE);
        int zone=OBJ_ZONE(&obj),type=obj.obj.number;
        int x=0,z=0;bool placement=true;
        if(cid>=state.level.num_object_points || size_t(points)+cid*8+8>bytes)placement=false;
        else {
            auto point=state.level.object_points+cid*8;
            x=obj_w(point);z=obj_w(point+4);
            auto sourcePoint=raw+points+cid*8;
            placement=OBJ_CID(&obj)==cid && x==obj_w(sourcePoint) && z==obj_w(sourcePoint+4)
                && obj_w(obj.raw+4)==obj_w(source+4) && zone==obj_w(source+12)
                && bool(obj.obj.in_top)==bool(source[63]);
        }
        if(!placement)result["placement_changes"]=result["placement_changes"].get<int>()+1;
        std::string kind="decoration",graphics="inactive";
        if(type==OBJ_NBR_MEDIKIT || type==OBJ_NBR_AMMO || type==OBJ_NBR_BIG_GUN || type==OBJ_NBR_KEY)kind="pickup";
        else if(type==OBJ_NBR_ALIEN || type==OBJ_NBR_ROBOT || type==OBJ_NBR_BIG_NASTY || type==OBJ_NBR_FLYING_NASTY ||
                (type>=OBJ_NBR_MARINE && type<=OBJ_NBR_FLAME_MARINE && type!=OBJ_NBR_BARREL))kind="enemy";
        if(zone>=0){
            const char *counter=kind=="pickup"?"pickups":kind=="enemy"?"enemies":"decorations";
            result[counter]=result[counter].get<int>()+1;
            if(obj.raw[6]==OBJ_3D_SPRITE){
                int slot=obj_w(obj.raw+8);
                graphics=slot>=0 && slot<POLY_OBJECTS_COUNT && g_poly_objects[slot].data?"polygon":"missing polygon";
            } else {
                XrBillboardInfo info{};
                if(renderer_xr_billboard_info(&obj,&info)){
                    std::vector<uint8_t> pixels(info.columns*info.rows*4);
                    bool decoded=renderer_xr_decode_sprite_indices(info.vector,info.frame,info.columns,info.rows,pixels.data());
                    bool visible=false;for(size_t n=3;n<pixels.size();n+=4)visible|=pixels[n]!=0;
                    graphics=decoded && visible?"sprite":"missing sprite";
                } else graphics="missing metadata";
            }
            if(graphics.rfind("missing",0)==0)result["unrenderable"]=result["unrenderable"].get<int>()+1;
        }
        result["objects"].push_back({{"slot",i},{"cid",cid},{"type",type},{"zone",zone},{"upper",bool(obj.obj.in_top)},
            {"x",x},{"z",z},{"y",obj_w(obj.raw+4)},{"kind",kind},{"graphics",graphics},{"original_placement",placement}});
    }
    return result;
}
}
