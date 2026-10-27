#pragma once
#include <math.h>
#include <stdint.h>
/* OpenXR forward is -Z; the original engine fires along (sin(yaw),cos(yaw)).
 * This conversion is deliberately different from the GPU camera yaw. */
static inline int16_t abxr_engine_shot_yaw(float x,float z){
    return (int16_t)((int)lroundf(atan2f(x,z)*(8192.f/6.283185307179586f))&8191);
}
/* Two PAL VBlanks per player weapon check reproduces the common 25fps Amiga
 * release/check quantization without slowing 50Hz movement or headset poses.
 * The original had a variable render-bound game loop, not a fixed 25Hz cap. */
static inline int abxr_weapon_clock(int *pending,int elapsed){
    *pending+=elapsed;
    if(*pending<2)return 0;
    *pending-=2;return 2;
}
/* Input snapshots arrive at 50Hz. Preserve a short click until the next
 * weapon check, then consume it once; held fire remains a separate state. */
static inline int abxr_weapon_click(int *pending,int pressed,int consume){
    *pending|=pressed!=0;
    int clicked=*pending;if(consume)*pending=0;return clicked;
}
/* First intersection of a 3D segment with an upright target cylinder. Y uses
 * the engine's 256:1 vertical scale. Intersect horizontal AND vertical time
 * intervals, rather than testing the projectile's final height only. */
static inline int abxr_segment_cylinder(double sx,double sy,double sz,double ex,double ey,double ez,
                                       double tx,double ty,double tz,double radius,double half_height,double *hit){
    double dx=ex-sx,dz=ez-sz,dy=(ey-sy)/256.,ox=sx-tx,oz=sz-tz,oy=(sy-ty)/256.;
    double lo=0,hi=1,a=dx*dx+dz*dz,b=2*(ox*dx+oz*dz),c=ox*ox+oz*oz-radius*radius;
    if(a<1e-12){if(c>0)return 0;}
    else{
        double discriminant=b*b-4*a*c;if(discriminant<0)return 0;
        double root=sqrt(discriminant),enter=(-b-root)/(2*a),leave=(-b+root)/(2*a);
        lo=fmax(lo,enter);hi=fmin(hi,leave);
    }
    if(fabs(dy)<1e-12){if(fabs(oy)>half_height)return 0;}
    else{
        double t0=(-half_height-oy)/dy,t1=(half_height-oy)/dy;
        lo=fmax(lo,fmin(t0,t1));hi=fmin(hi,fmax(t0,t1));
    }
    if(lo>hi)return 0;
    *hit=lo;return 1;
}
