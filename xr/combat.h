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
