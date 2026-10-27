/* Production hitscan, damage, projectile launch and cooldown tests, with
 * deterministic accuracy and an XR aim flag instead of a headset connection. */
#define main legacy_checks_main
#include "../../tests/feedback_shooting.c"
#undef main
#include "combat.h"
static int xr_aim=0,fired=0;
static int use_ray=0;
static float direction[3];
int audio_distance_volume(int volume,int x,int z){(void)volume;(void)x;(void)z;return 255;}
int abxr_aim_active(const void *state,const void *player){(void)state;(void)player;return xr_aim;}
int abxr_shot_direction(const void *state,const void *player,float *x,float *y,float *z){
 (void)state;(void)player;if(!xr_aim || !use_ray)return 0;
 *x=direction[0];*y=direction[1];*z=direction[2];return 1;
}
void abxr_shot_fired(void *state,const void *player,int gun){(void)state;(void)player;(void)gun;++fired;}
static GameState state;
static GameObject targets[2],shots[PLAYER_SHOT_SLOT_COUNT];
static uint8_t points[(PLAYER_SHOT_SLOT_COUNT+1)*8];
static void setup(int gun){
 memset(&state,0,sizeof(state));memset(targets,0,sizeof(targets));memset(shots,0,sizeof(shots));memset(points,0,sizeof(points));
 state.temp_frames=1;state.nasty=1;state.cfg_mouse_look=1;state.cfg_y_proj_scale=100;
 state.level.object_data=(uint8_t*)targets;state.level.player_shot_data=(uint8_t*)shots;state.level.object_points=points;state.level.num_object_points=PLAYER_SHOT_SLOT_COUNT+1;
 OBJ_SET_CID(&targets[0],0);OBJ_SET_ZONE(&targets[0],0);targets[0].obj.number=OBJ_NBR_WORM;targets[0].obj.can_see=3;NASTY_LIVES(targets[0])=80;
 OBJ_SET_CID(&targets[1],-1);
 for(int i=0;i<PLAYER_SHOT_SLOT_COUNT;++i){OBJ_SET_CID(&shots[i],i+1);OBJ_SET_ZONE(&shots[i],-1);}
 PlayerState *p=&state.plr1;p->gun_selected=gun;p->gun_data[gun].visible=-1;p->gun_data[gun].ammo=32000;p->p_fire=-1;p->p_clicked=-1;
 p->yoff=-12288;p->height=p->s_height=12288;obj_sw(targets[0].raw+4,-96);
 xr_aim=1;gamepad_firing=1;fired=0;
}
static void aim(float x,float z){
 int angle=abxr_engine_shot_yaw(x,z);state.plr1.angpos=angle;state.plr1.sinval=sin_lookup(angle);state.plr1.cosval=cos_lookup(angle);
}
int main(void){
 legacy_checks_main();
 // Vertical and steep shots intersect throughout the segment, and a high
 // endpoint cannot manufacture a hit after passing beneath an enemy.
 double hit=0;
 assert(abxr_segment_cylinder(0,0,0,0,-8192,0,0,-4096,0,20,2,&hit));
 assert(hit>.4 && hit<.6);
 assert(abxr_segment_cylinder(0,0,0,100,-12800,0,50,-6400,0,10,5,&hit));
 assert(!abxr_segment_cylinder(0,0,0,100,-12800,0,20,-12800,0,10,5,&hit));
 // Preserve the exact tracked barrel direction for steep projectiles.
 setup(1);aim(0,-1);OBJ_SET_ZONE(&targets[0],-1);use_ray=1;
 direction[0]=0;direction[1]=.8660254f;direction[2]=-.5f;
 player1_shoot(&state);
 assert(abs(SHOT_XVEL(shots[0]))<2 && abs(SHOT_ZVEL(shots[0])+8*65536)<2);
 assert(abs(SHOT_YVEL(shots[0])+3547)<2);
 use_ray=0;
 // Hitscan uses the complete tracked ray at close range, including an enemy
 // directly below the muzzle (zero horizontal forward distance).
 for(int vertical=0;vertical<2;++vertical){
  setup(0);use_ray=1;aim(0,1);
  obj_sw(points+4,vertical?0:20);obj_sw(targets[0].raw+4,-32);
  direction[0]=0;direction[1]=vertical?-1.f:-.848f;direction[2]=vertical?0:.53f;
  player1_shoot(&state);
  assert(fired==1 && targets[0].raw[19]==4 && state.plr1.gun_data[0].ammo==31992);
  assert(OBJ_ZONE(&shots[0])==0 && obj_w(shots[0].raw+4)>=-96 && obj_w(shots[0].raw+4)<=0);
 }
 use_ray=0;
 // Exercise the real projectile handler: crossing the target vertically
 // during a tick must count even when its endpoint is above the hit volume.
 setup(1);aim(0,-1);OBJ_SET_ZONE(&targets[0],-1);player1_shoot(&state);
 OBJ_SET_ZONE(&targets[0],0);obj_sw(points+4,-150);obj_sw(targets[0].raw+4,-31);
 SHOT_SET_ACCYPOS(shots[0],0);obj_sw(shots[0].raw+4,0);
 SHOT_SET_YVEL(shots[0],-20000);SHOT_SET_ZVEL(shots[0],-300*65536);
 object_handle_bullet(&shots[0],&state);assert(targets[0].raw[19]==16);
 assert(SHOT_ACCYPOS(shots[0])<0 && SHOT_ACCYPOS(shots[0])>-20000);
 // A target whose generous hit radius protrudes through a solid wall must
 // remain protected by that wall in the production handler.
 setup(1);aim(0,-1);OBJ_SET_ZONE(&targets[0],-1);player1_shoot(&state);
 static uint8_t room_data[64],room_adds[4],wall[16];
 memset(room_data,0,sizeof(room_data));memset(wall,0,sizeof(wall));
 obj_sl(room_data+6,-32768);obj_sw(room_data+32,48);
 obj_sw(room_data+48,0);obj_sw(room_data+50,-1);obj_sw(room_data+52,-2);
 obj_sw(wall,64);obj_sw(wall+2,-100);obj_sw(wall+4,-128);obj_sw(wall+8,-1);obj_sw(wall+10,128);
 state.level.data=room_data;state.level.data_byte_count=sizeof(room_data);state.level.zone_adds=room_adds;
 state.level.floor_lines=wall;state.level.num_floor_lines=1;state.level.num_zones=state.level.num_zone_slots=1;
 OBJ_SET_ZONE(&targets[0],0);obj_sw(points+4,-200);
 for(int tick=0;tick<20 && !SHOT_STATUS(shots[0]);++tick)object_handle_bullet(&shots[0],&state);
 assert(targets[0].raw[19]==0 && SHOT_STATUS(shots[0]));
 // Generous hitscan volumes protruding through masonry must not bypass it.
 setup(0);aim(0,-1);use_ray=1;direction[0]=0;direction[1]=0;direction[2]=-1;
 state.level.data=room_data;state.level.data_byte_count=sizeof(room_data);state.level.zone_adds=room_adds;
 state.level.floor_lines=wall;state.level.num_floor_lines=1;state.level.num_zones=state.level.num_zone_slots=1;
 obj_sw(points+4,-150);player1_shoot(&state);
 assert(fired==1 && targets[0].raw[19]==0);use_ray=0;
 // A full cosmetic pool must not eat successful damage.
 setup(0);aim(0,-1);obj_sw(points,0);obj_sw(points+4,-200);
 for(int i=0;i<PLAYER_SHOT_SLOT_COUNT;++i)OBJ_SET_ZONE(&shots[i],0);
 player1_shoot(&state);assert(targets[0].raw[19]==4);
 for(int cardinal=0;cardinal<4;++cardinal){
  const float x[]={0,-1,0,1},z[]={-1,0,1,0};
  setup(0);aim(x[cardinal],z[cardinal]);obj_sw(points,(int)(x[cardinal]*200));obj_sw(points+4,(int)(z[cardinal]*200));
  player1_shoot(&state);assert(targets[0].raw[19]==4 && fired==1 && OBJ_ZONE(&shots[0])>=0);
  // Apply actual damage via the real enemy handler, rather than stopping at
  // an in-line target/cache assertion. Worms take quarter damage: 80 -> 79.
  object_handle_worm(&targets[0],&state);assert(NASTY_LIVES(targets[0])==79);
  setup(0);aim(-x[cardinal],-z[cardinal]);obj_sw(points,(int)(x[cardinal]*200));obj_sw(points+4,(int)(z[cardinal]*200));
  player1_shoot(&state);assert(targets[0].raw[19]==0 && fired==1);
  setup(1);aim(x[cardinal],z[cardinal]);OBJ_SET_ZONE(&targets[0],-1);player1_shoot(&state);
  assert(SHOT_XVEL(shots[0])*x[cardinal]+SHOT_ZVEL(shots[0])*z[cardinal]>0);
  // Advance the real projectile collision/damage path to the enemy.
  OBJ_SET_ZONE(&targets[0],0);obj_sw(points,(int)(x[cardinal]*200));obj_sw(points+4,(int)(z[cardinal]*200));
  for(int tick=0;tick<20 && OBJ_ZONE(&shots[0])>=0;++tick)object_handle_bullet(&shots[0],&state);
  assert(targets[0].raw[19]==16);
  object_handle_worm(&targets[0],&state);assert(NASTY_LIVES(targets[0])==76);
 }
 // A click on the intervening 50Hz tick must survive its release and fire
 // once at the next 25Hz weapon check, without turning into held autofire.
 setup(1);aim(0,-1);OBJ_SET_ZONE(&targets[0],-1);int clicks=0;
 abxr_weapon_click(&clicks,1,0);abxr_weapon_click(&clicks,0,0);
 state.plr1.p_fire=0;state.plr1.p_clicked=abxr_weapon_click(&clicks,0,1);
 player1_shoot(&state);assert(fired==1 && clicks==0);
 state.plr1.time_to_shoot=0;state.plr1.p_clicked=abxr_weapon_click(&clicks,0,1);
 player1_shoot(&state);assert(fired==1);
 puts("PASS short plasma clicks latch until the next weapon check and consume exactly once");
 puts("PASS controller rays damage enemies in all four world directions; reverse rays miss; plasma launches forwards");
 for(int fps_index=0;fps_index<6;++fps_index){
  const int rates[]={30,60,72,90,120,144};int fps=rates[fps_index];setup(0);OBJ_SET_ZONE(&targets[0],-1);aim(0,-1);
  int pending=0,previous=0;
  for(int display=1;display<=fps*10;++display){
   int vblank=display*50/fps;pending+=vblank-previous;previous=vblank;int ticks;
   while((ticks=abxr_weapon_clock(&pending,0))>0){
    state.temp_frames=ticks;state.plr1.p_fire=-1;state.plr1.p_clicked=-1;
    for(int i=0;i<PLAYER_SHOT_SLOT_COUNT;++i)OBJ_SET_ZONE(&shots[i],-1);
    player1_shoot(&state);
   }
  }
  assert(fired==63);assert(pending==0);printf("PASS %d Hz display: %d actual pistol shots in 10 seconds (same 500 PAL VBlanks)\n",fps,fired);
 }
}
