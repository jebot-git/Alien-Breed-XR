#pragma once
#include <array>
#include <cmath>
#include <openxr/openxr.h>
namespace tempest {
struct WeaponRay { XrVector3f origin,direction; };
/* Original world/view coordinates are left handed (+Z forward); OpenXR is
 * right handed (-Z forward). Reflect exactly once, including anchor yaw. */
inline XrVector3f engineVector(XrVector3f local,float bodyYaw,float anchorYaw){
    float yaw=bodyYaw+anchorYaw,c=std::cos(yaw),s=std::sin(yaw);
    return {c*local.x-s*local.z,local.y,-s*local.x-c*local.z};
}
inline XrVector3f rotateVector(XrQuaternionf q,XrVector3f v){
    return {(1-2*(q.y*q.y+q.z*q.z))*v.x+2*(q.x*q.y-q.w*q.z)*v.y+2*(q.x*q.z+q.w*q.y)*v.z,
            2*(q.x*q.y+q.w*q.z)*v.x+(1-2*(q.x*q.x+q.z*q.z))*v.y+2*(q.y*q.z-q.w*q.x)*v.z,
            2*(q.x*q.z-q.w*q.y)*v.x+2*(q.y*q.z+q.w*q.x)*v.y+(1-2*(q.x*q.x+q.y*q.y))*v.z};
}
inline bool handInReach(XrPosef hand,XrVector3f head){
    float x=hand.position.x-head.x,y=hand.position.y-head.y,z=hand.position.z-head.z;
    return std::isfinite(x+y+z) && x*x+y*y+z*z<=2.25f;
}
/* Shared muzzle and -Z barrel ray for both shooting and the short laser. */
inline WeaponRay weaponRay(XrPosef pose,const std::array<float,3> &socket,const std::array<float,3> &direction={0,0,-1}){
    auto offset=rotateVector(pose.orientation,{socket[0],socket[1],socket[2]});
    return {{pose.position.x+offset.x,pose.position.y+offset.y,pose.position.z+offset.z},
            rotateVector(pose.orientation,{direction[0],direction[1],direction[2]})};
}
}
