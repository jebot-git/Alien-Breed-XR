#pragma once
#include "aim.hpp"
#include "game.hpp"
#include "combat.h"
#include <algorithm>

namespace tempest {
struct ShotOrigin {
    XrVector3f point{},direction{},localRetraction{};
    int zone=-1;
    bool clipped=false;
};
/* Resolve the same safe muzzle for the visible weapon, laser and live shots.
 * A barrel pushed into masonry/floor or a close enemy retracts; it never
 * disables the trigger or teleports a shot beyond the obstruction. */
inline ShotOrigin resolveShotOrigin(const Game &game,XrPosef pose,XrVector3f anchor,
                                    float anchorYaw,const std::array<float,3> &muzzle,
                                    const std::array<float,3> &direction,
                                    XrVector3f viewOffset={}){
    const auto &s=*game.state;const auto &p=s.plr1;
    float yaw=p.angpos*(6.283185307f/8192);
    auto ray=weaponRay(pose,muzzle,direction);
    auto offset=engineVector({ray.origin.x-anchor.x,ray.origin.y-anchor.y,ray.origin.z-anchor.z},yaw,anchorYaw);
    // The sweep reference remains the collision-safe simulated body. Applying
    // head pushback to that reference could put it on the far side of a wall.
    double start[3]={p.xoff/65536.,p.yoff-game.crouchCompensation*8192.,p.zoff/65536.};
    double desired[3]={start[0]+(offset.x+viewOffset.x)*32,
        start[1]-(offset.y+viewOffset.y)*8192,start[2]+(offset.z+viewOffset.z)*32},safe[3];
    ShotOrigin result;
    result.direction=engineVector(ray.direction,yaw,anchorYaw);
    result.clipped=view_clip_move(&s.level,p.zone,start,desired,.15,false,safe);
    // Do not spawn past or inside an enemy when its collision cylinder meets
    // the physical barrel reach. This also covers short enemies below the hand.
    double first=1;
    if(s.level.object_data && s.level.object_points)for(int i=0;i<MAX_OBJECTS;++i){
        auto *obj=reinterpret_cast<const GameObject*>(s.level.object_data+i*OBJECT_SIZE);
        int cid=OBJ_CID(obj),type=obj->obj.number;
        if(cid<0)break;
        if(cid>=s.level.num_object_points || OBJ_ZONE(obj)<0 || type>20 || type<0 ||
            !(0x003FFDC1u&(1u<<type)) || (NASTY_LIVES(*obj)<=0 && type!=OBJ_NBR_BARREL))continue;
        auto *point=s.level.object_points+cid*8;
        double hit;
        if(abxr_segment_cylinder(start[0],start[1],start[2],safe[0],safe[1],safe[2],
            obj_w(point),obj_w(obj->raw+4)*128.,obj_w(point+4),
            col_box_table[type].width*.5,col_box_table[type].half_height*.5,&hit))first=std::min(first,hit);
    }
    if(first<1){
        double length=std::sqrt(std::pow(safe[0]-start[0],2)+std::pow((safe[1]-start[1])/256,2)+std::pow(safe[2]-start[2],2));
        first=std::max(0.,first-.3/std::max(.001,length));
        for(int k=0;k<3;++k)safe[k]=start[k]+(safe[k]-start[k])*first;
        result.clipped=true;
    }
    result.point={float(safe[0]),float(safe[1]),float(safe[2])};
    // The reflection/rotation in engineVector is its own inverse.
    auto local=engineVector({float((safe[0]-desired[0])/32),float(-(safe[1]-desired[1])/8192),float((safe[2]-desired[2])/32)},yaw,anchorYaw);
    result.localRetraction=local;
    result.zone=level_find_zone_for_point(&s.level,int(safe[0]),int(safe[2]),p.zone);
    if(result.zone<0)result.zone=p.zone;
    return result;
}
}
