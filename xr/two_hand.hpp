#pragma once
#include "aim.hpp"
#include <algorithm>

namespace tempest {
/* A second physical point controls orientation only. No temporal smoothing,
 * recoil/spread adjustment, target selection or movement changes. */
class TwoHandHold {
    bool waitForRelease=false,previousSqueeze=false;
    int previousWeapon=-1;
    void release(){if(active)++releases;active=false;}
    static float length(XrVector3f v){return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);}
    static bool finite(XrVector3f v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
  public:
    bool active=false,near=false;
    unsigned grabs=0,releases=0,frames=0;
    XrPosef update(XrPosef primary,bool primaryValid,XrVector3f support,bool supportValid,
                   bool squeezed,bool enabled,bool blocked,int weapon,const std::array<float,3> &socket){
        near=false;
        bool changed=previousWeapon>=0 && weapon!=previousWeapon;previousWeapon=weapon;
        auto q=primary.orientation;
        float norm=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
        bool valid=primaryValid && supportValid && finite(primary.position) && finite(support) && std::isfinite(norm) && norm>.001f;
        if(!enabled || blocked || !valid || changed){
            release();waitForRelease=waitForRelease || squeezed || previousSqueeze;
            previousSqueeze=squeezed;return primary;
        }
        previousSqueeze=squeezed;
        if(!squeezed){release();waitForRelease=false;}
        XrVector3f local{socket[0],socket[1],socket[2]};
        if(!finite(local)){release();waitForRelease=true;return primary;}
        q.x/=norm;q.y/=norm;q.z/=norm;q.w/=norm;
        auto forward=rotateVector(q,local);
        XrVector3f delta{support.x-primary.position.x,support.y-primary.position.y,support.z-primary.position.z};
        XrVector3f error{delta.x-forward.x,delta.y-forward.y,delta.z-forward.z};
        near=length(error)<=.16f;
        if(!squeezed || waitForRelease)return primary;
        float reach=length(local),distance=length(delta);
        if(reach<.10f || distance<.10f || distance>.85f || std::abs(distance-reach)>.20f){
            if(active){release();waitForRelease=true;}return primary;
        }
        float dot=(forward.x*delta.x+forward.y*delta.y+forward.z*delta.z)/(reach*distance);
        if(dot<-.17f){if(active){release();waitForRelease=true;}return primary;}
        if(!active){if(length(error)>.12f)return primary;active=true;++grabs;}
        // Shortest arc from the right-hand foregrip direction to the measured
        // left grip. The right hand supplies roll and the fixed position anchor.
        XrVector3f a{forward.x/reach,forward.y/reach,forward.z/reach};
        XrVector3f b{delta.x/distance,delta.y/distance,delta.z/distance};
        XrQuaternionf turn{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x,1+std::clamp(dot,-1.f,1.f)};
        float n=std::sqrt(turn.x*turn.x+turn.y*turn.y+turn.z*turn.z+turn.w*turn.w);
        turn.x/=n;turn.y/=n;turn.z/=n;turn.w/=n;
        primary.orientation={turn.w*q.x+turn.x*q.w+turn.y*q.z-turn.z*q.y,
            turn.w*q.y-turn.x*q.z+turn.y*q.w+turn.z*q.x,
            turn.w*q.z+turn.x*q.y-turn.y*q.x+turn.z*q.w,
            turn.w*q.w-turn.x*q.x-turn.y*q.y-turn.z*q.z};
        ++frames;return primary;
    }
};
}
