#include "two_hand.hpp"
#include <cassert>
#include <limits>
using namespace tempest;
static float distance(XrVector3f a,XrVector3f b){return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z));}
static XrVector3f target(XrPosef p,const std::array<float,3> &socket){
    auto v=rotateVector(p.orientation,{socket[0],socket[1],socket[2]});
    return {p.position.x+v.x,p.position.y+v.y,p.position.z+v.z};
}
int main(){
    const std::array<float,3> socket{0,.05f,-.3f};
    XrPosef right{{0,0,0,1},{.2f,1.3f,-.2f}};
    TwoHandHold hold;
    auto left=target(right,socket);
    auto update=[&](bool squeeze,bool leftValid=true,bool enabled=true,bool menu=false,int weapon=0){
        return hold.update(right,true,left,leftValid,squeeze,enabled,menu,weapon,socket);
    };
    auto pose=update(false);assert(!hold.active && hold.near);
    pose=update(true);assert(hold.active && hold.grabs==1);
    // Aligned hands produce the same ray: no automatic aim or recoil bonus.
    auto before=weaponRay(right,{0,.1f,-.5f}),after=weaponRay(pose,{0,.1f,-.5f});
    assert(distance(before.origin,after.origin)<.00001f && distance(before.direction,after.direction)<.00001f);
    left.x+=.07f;left.y+=.04f;
    pose=update(true);assert(hold.active && distance(pose.position,right.position)==0);
    auto from=rotateVector(pose.orientation,{socket[0],socket[1],socket[2]});
    XrVector3f delta{left.x-right.position.x,left.y-right.position.y,left.z-right.position.z};
    float ratio=std::sqrt((from.x*from.x+from.y*from.y+from.z*from.z)/(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z));
    assert(distance(from,{delta.x*ratio,delta.y*ratio,delta.z*ratio})<.00001f);
    assert(std::abs(pose.orientation.y)>.02f); // physical offhand changes direction
    pose=update(false);assert(!hold.active && pose.orientation.w==1);
    left={-1,1,-.5f};update(true);assert(!hold.active); // cannot grab at a distance
    left=target(right,socket);update(true);assert(hold.active);
    update(true,false);assert(!hold.active);update(true);assert(!hold.active); // lost tracking requires release
    update(false);update(true);assert(hold.active);
    update(true,true,true,true);assert(!hold.active);update(true);assert(!hold.active); // pause cannot silently re-grab
    update(false);update(true);assert(hold.active);
    update(true,true,false);assert(!hold.active);update(true);assert(!hold.active); // option toggled off/on
    update(false);update(true);assert(hold.active);
    update(true,true,true,false,2);assert(!hold.active);update(true,true,true,false,2);assert(!hold.active);
    update(false,true,true,false,2);update(true,true,true,false,2);assert(hold.active);
    left=right.position;pose=update(true,true,true,false,2);assert(!hold.active && pose.orientation.w==1); // no coincident-hand singularity
    update(false,true,true,false,2);
    // A rolled firing hand preserves its roll when the support aligns.
    right.orientation={0,0,.70710678f,.70710678f};left=target(right,socket);
    pose=update(true,true,true,false,2);assert(hold.active && std::abs(pose.orientation.z-right.orientation.z)<.00001f);
    left.x=std::numeric_limits<float>::quiet_NaN();pose=update(true,true,true,false,2);
    assert(!hold.active && std::isfinite(pose.orientation.w));
}
