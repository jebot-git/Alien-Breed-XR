#include "shot_origin.hpp"
#include <cassert>
#include <cstdio>
using namespace tempest;
static GameState state;
static uint8_t room[64],adds[4],wall[16],points[8];
static GameObject objects[2];
int main(){
    auto &l=state.level;l.data=room;l.data_byte_count=sizeof(room);l.zone_adds=adds;
    l.floor_lines=wall;l.num_floor_lines=1;l.num_zones=l.num_zone_slots=1;
    obj_sl(room+6,-32768);obj_sw(room+32,48);obj_sw(room+48,0);obj_sw(room+50,-1);obj_sw(room+52,-2);
    obj_sw(wall,32);obj_sw(wall+2,64);obj_sw(wall+6,-128);obj_sw(wall+8,-1);obj_sw(wall+10,128);
    auto &p=state.plr1;p.yoff=-12288;p.zone=0;
    Game game{&state};XrVector3f anchor{0,1.6f,0};
    XrPosef pose{{0,0,0,1},{.2f,1.35f,-.25f}};
    std::array<float,3> muzzle{0,.1f,-.35f};
    auto a=resolveShotOrigin(game,pose,anchor,0,muzzle,{0,0,-1});
    assert(!a.clipped && a.zone==0 && std::abs(a.point.z-19.2f)<.001f);
    // Aiming at a solid wall retracts to its near side, rather than cancelling.
    pose.orientation={0,-.70710678f,0,.70710678f};
    a=resolveShotOrigin(game,pose,anchor,0,{0,.1f,-1},{0,0,-1});
    assert(a.clipped && a.point.x<31.86f && a.point.x>31.7f);
    auto retracted=pose;retracted.position.x+=a.localRetraction.x;retracted.position.y+=a.localRetraction.y;retracted.position.z+=a.localRetraction.z;
    auto visible=weaponRay(retracted,{0,.1f,-1});
    auto offset=engineVector({visible.origin.x-anchor.x,visible.origin.y-anchor.y,visible.origin.z-anchor.z},0,0);
    assert(std::abs(offset.x*32-a.point.x)<.001f && std::abs(offset.z*32-a.point.z)<.001f);
    // The barrel crossing the floor keeps its downward direction and a safe
    // launch point above the floor, even with the hand below the reference.
    pose={{-.5f,0,0,.8660254f},{.2f,.30f,-.25f}};
    a=resolveShotOrigin(game,pose,anchor,0,muzzle,{0,0,-1});
    assert(a.clipped && a.point.y<=-38 && a.direction.y<-.86f);
    // Close enemy cylinders stop the muzzle reach before it passes the target.
    l.object_data=(uint8_t*)objects;l.object_points=points;l.num_object_points=1;
    OBJ_SET_CID(&objects[0],0);OBJ_SET_ZONE(&objects[0],0);objects[0].obj.number=OBJ_NBR_WORM;
    NASTY_LIVES(objects[0])=80;OBJ_SET_CID(&objects[1],-1);
    obj_sw(points,0);obj_sw(points+4,55);obj_sw(objects[0].raw+4,-96);
    pose={{0,0,0,1},{0,1.5f,-.25f}};
    a=resolveShotOrigin(game,pose,anchor,0,{0,.1f,-.824f},{0,0,-1});
    assert(a.clipped && a.point.z<55-col_box_table[OBJ_NBR_WORM].width*.5 && a.point.z>0);
    // The same contact below the hand remains hittable by a downward ray.
    obj_sw(objects[0].raw+4,-32);pose={{-.5f,0,0,.8660254f},{0,1.f,-.25f}};
    a=resolveShotOrigin(game,pose,anchor,0,{0,.1f,-.824f},{0,0,-1});
    assert(a.direction.y<-.86f && a.zone==0 && std::isfinite(a.point.y));
    // A large head correction must not move the safety sweep's reference
    // outside the room and manufacture a shot behind the solid wall.
    l.object_data=nullptr;pose={{0,0,0,1},{.2f,1.35f,-.25f}};
    a=resolveShotOrigin(game,pose,anchor,0,muzzle,{0,0,-1},{3,0,0});
    assert(a.clipped && a.point.x<31.86f && a.point.x>31.7f);
    puts("PASS wall/floor/close-target muzzle retraction, visible muzzle agreement and unchanged downward aim");
}
