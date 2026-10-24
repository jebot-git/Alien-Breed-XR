/* Exercise actual shot selection after a turn/strafe with stale target caches.
 * Deterministic hit rolls isolate aim-cache behavior from random accuracy.
 * Audio, controller input and projection dimensions are fixed test inputs. */
#define rand feedback_test_rand
#include "../src/player.c"
#undef rand
#include <assert.h>
int feedback_test_rand(void) { return 32767; }
GameState g_state;
RendererState g_renderer;
int renderer_get_width(void) { return 320; }
int renderer_get_height(void) { return 200; }
void audio_play_sample(int id, int volume) { (void)id; (void)volume; }
static bool gamepad_firing;
bool input_gamepad_fire_held(void) { return gamepad_firing; }
static void check_shot(int player, int gun, int mouse_look, int old_angle, int old_x, int new_angle, int new_x, int expected) {
 static GameState state;
 static GameObject objects[2], shots[PLAYER_SHOT_SLOT_COUNT];
 static uint8_t points[(PLAYER_SHOT_SLOT_COUNT+1)*8];
 memset(&state,0,sizeof(state));memset(objects,0,sizeof(objects));memset(shots,0,sizeof(shots));memset(points,0,sizeof(points));
 state.cfg_mouse_look=mouse_look;
 state.cfg_y_proj_scale=100;
 gamepad_firing=(gun==1);
 state.level.object_data=(uint8_t*)objects;
 state.level.object_points=points;
 state.level.num_object_points=PLAYER_SHOT_SLOT_COUNT+1;
 state.level.player_shot_data=(uint8_t*)shots;
 GameObject *worm=&objects[0];
 OBJ_SET_CID(worm,0);OBJ_SET_ZONE(worm,0);
 worm->obj.number=OBJ_NBR_WORM;worm->obj.can_see=3;
 NASTY_LIVES(*worm)=80; /* Original level_o/twolev.bin worms. */
 if(gun==1)obj_sw(worm->raw+4,12);
 OBJ_SET_CID(&objects[1],-1);
 obj_sw(points,0);obj_sw(points+4,200);
 for(int i=0;i<PLAYER_SHOT_SLOT_COUNT;i++) {OBJ_SET_CID(&shots[i],i+1);OBJ_SET_ZONE(&shots[i],-1);}
 PlayerState *p=player==1?&state.plr1:&state.plr2;
 p->gun_selected=gun;p->gun_data[gun].visible=-1;p->gun_data[gun].ammo=160;p->p_fire=-1;p->p_clicked=-1;
 p->sinval=sin_lookup(old_angle);p->cosval=cos_lookup(old_angle);p->xoff=old_x*65536;
 if(player==1)calc_plr1_in_line(&state);else calc_plr2_in_line(&state);
 p->sinval=sin_lookup(new_angle);p->cosval=cos_lookup(new_angle);p->xoff=new_x*65536;
 if(player==1)player1_shoot(&state);else player2_shoot(&state);
 if(gun==1) {
  assert(SHOT_SIZE(shots[0])==1 && SHOT_POWER(shots[0])==16);
  assert(SHOT_YVEL(shots[0])==(expected?128:0));
 } else {
  assert(worm->raw[19]==(expected?(gun==7?28:4):0));
 }
 assert(p->gun_data[gun].ammo==152);
}
int main(void) {
 math_tables_init();
 for(int player=1;player<=2;player++) {
  const int guns[]={0,7,1};
  for(int mode=0;mode<2;mode++)for(int g=0;g<3;g++) {
   int gun=guns[g];
   if(gun==1 && !mode)continue;
   check_shot(player,gun,mode,ANGLE_90,0,0,0,1);
   check_shot(player,gun,mode,0,0,ANGLE_90,0,0);
   check_shot(player,gun,mode,0,400,0,0,1);
   check_shot(player,gun,mode,0,0,0,400,0);
  }
 }
 puts("PASS: 40 current-aim/position cases, both players, mouse/fixed-view pistol/shotgun and controller plasma aim");
 return 0;
}
