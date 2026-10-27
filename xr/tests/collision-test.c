/* Exercise production collision, sprite metadata and footstep bookkeeping. */
#include "../../src/player.c"
#include <assert.h>
static int sounds;
void audio_play_sample(int sample, int volume) { (void)sample; (void)volume; ++sounds; }
static void word(uint8_t *p,int n) { p[0]=(uint16_t)n>>8;p[1]=n; }
static void longword(uint8_t *p,int n) { word(p,(uint32_t)n>>16);word(p+2,n); }
static uint8_t data[256],adds[8],lines[48];
static void room(int id,int offset,int line) {
    longword(adds+id*4,offset);uint8_t *p=data+offset;
    word(p,id);longword(p+2,0);longword(p+6,-32768);
    word(p+32,48);word(p+48,line);word(p+50,-1);word(p+52,-2);
}
static void line(int id,int x,int target) {
    uint8_t *p=lines+id*16;
    word(p,x);word(p+2,-64);word(p+4,0);word(p+6,128);
    word(p+8,target);word(p+10,128);
}
int main(void) {
    static GameState state;
    LevelState *level=&state.level;
    level->data=data;level->data_byte_count=sizeof(data);level->zone_adds=adds;
    level->floor_lines=lines;level->num_floor_lines=3;
    level->num_zones=level->num_zone_slots=2;
    room(0,0,0);room(1,64,1);
    line(0,32,-1);line(1,64,-1);line(2,32,0);
    assert(!view_path_blocked(level,0,0,-12288,0,20,-12288,0,4));
    assert(view_path_blocked(level,0,0,-12288,0,30,-12288,0,4));
    assert(view_path_blocked(level,0,0,-12288,0,80,-12288,0,4));
    /* A stationary eye sphere must also detect a nearby wall. */
    assert(view_path_blocked(level,0,30,-12288,0,30,-12288,0,4));
    /* Portal traversal, then collision with the wall in the next room. */
    word(lines+8,1);word(data+64+50,2);word(data+64+52,-1);word(data+64+54,-2);
    assert(!view_path_blocked(level,0,0,-12288,0,40,-12288,0,4));
    assert(view_path_blocked(level,0,0,-12288,0,80,-12288,0,4));
    /* Live closed door and ceiling/floor intrusion. */
    longword(data+64+6,0);
    assert(view_path_blocked(level,0,0,-12288,0,40,-12288,0,4));
    longword(data+64+6,-32768);
    assert(view_path_blocked(level,0,0,-12288,0,0,-32000,0,4));
    assert(view_path_blocked(level,0,0,-12288,0,0,-500,0,4));
    assert(!view_path_blocked(level,0,0,-12288,0,0,-16000,0,4));
    /* Query must not set the engine's wall-use/touch bits. */
    assert(obj_w(lines+14)==0 && obj_w(lines+30)==0);
    const int types[]={OBJ_NBR_ALIEN,OBJ_NBR_ROBOT,OBJ_NBR_MARINE,OBJ_NBR_WORM};
    const int heights[]={40,120,64,100};
    for(int i=0;i<4;++i) {
        GameObject obj={0};XrBillboardInfo info={0};
        obj.raw[16]=types[i];obj.raw[6]=obj.raw[7]=heights[i];
        word(obj.raw+4,-heights[i]);word(obj.raw+8,-1);
        assert(renderer_xr_billboard_info(&obj,&info));
        /* Centre elevation equals half full quad height at authored floor 0. */
        assert(fabs(-info.center_y/8192.0-info.height/64.0)<.0001);
        assert(info.width>0 && info.columns>0 && info.rows>0);
    }
    GameObject flyer={0};XrBillboardInfo info={0};
    flyer.raw[16]=OBJ_NBR_FLYING_NASTY;flyer.raw[6]=flyer.raw[7]=32;
    word(flyer.raw+4,-128);word(flyer.raw+8,-1);
    assert(renderer_xr_billboard_info(&flyer,&info));
    assert(-info.center_y/8192.0-info.height/64.0>1); // flying stays above floor
    // Shot-pool sprites and independent explosion records retain authored
    // centre heights, frame selection, dimensions and delayed-start behavior.
    GameObject impact={0};XrBillboardInfo effect={0};impact.obj.number=OBJ_NBR_BULLET;
    impact.raw[6]=20;impact.raw[7]=30;word(impact.raw+4,-60);word(impact.raw+8,2);
    impact.raw[14]=12;impact.raw[15]=15;
    assert(renderer_xr_shot_info(&impact,&effect));assert(effect.center_y==-60*128 && effect.width==20 && effect.height==30);
    assert(effect.columns==24 && effect.rows==30);
    state.num_explosions=1;state.explosions[0].zone=0;state.explosions[0].frame=0;
    state.explosions[0].y_floor=-4096;state.explosions[0].size_scale=100;
    assert(renderer_xr_explosion_info(&state,0,&effect));assert(effect.vector==8 && effect.center_y==-4096 && effect.width>0);
    state.explosions[0].start_delay=1;assert(!renderer_xr_explosion_info(&state,0,&effect));
    /* Hundreds of blocked updates and fractional jitter cannot play a step. */
    PlayerState *p=&state.plr1;p->roompt=0;p->zone=0;
    state.plr1_clumptime=4000;
    for(int i=0;i<500;++i) {
        p->xoff=i%65536;p->oldxoff=0;
        player_clump_after_collision(p,&state);
    }
    assert(sounds==0 && state.plr1_clumptime==4000);
    p->xoff=2<<16;player_clump_after_collision(p,&state);
    assert(sounds==1); // accepted movement crosses the step threshold
    state.plr1_clumptime=0;sounds=0;p->xoff=0;p->zoff=64<<16;
    player_clump_after_collision(p,&state);assert(sounds==1); // strafing counts
    // Cadence is one trigger per 64 accepted units, half the former rate.
    state.plr1_clumptime=0;sounds=0;p->zoff=p->oldzoff=0;
    for(int i=0;i<128;++i){p->oldxoff=i*4<<16;p->xoff=(i+1)*4<<16;player_clump_after_collision(p,&state);}
    assert(sounds==8);
    // Calibration restores standing body height while preserving the feet.
    longword(data+6,-32768);p->height=p->s_height=p->p_height=PLAYER_CROUCHED;
    p->s_yoff=p->yoff=-PLAYER_CROUCHED;p->s_tyoff=-PLAYER_CROUCHED;
    player_xr_reset_height(&state);
    assert(p->s_height==PLAYER_HEIGHT && p->s_yoff==-PLAYER_HEIGHT && !p->ducked);
    assert(p->s_yoff+p->s_height==0 && p->p_height==p->height);
    // A low ceiling still forces the original crouched body and camera.
    longword(data+6,-10000);player_xr_reset_height(&state);
    assert(p->s_height==PLAYER_CROUCHED && p->s_yoff==-PLAYER_CROUCHED && p->ducked);
    assert(p->s_yoff>=-10000 && p->s_yoff+p->s_height==0);
    // Old saves/INIs and all-weapons flags must not expose extra XR weapons.
    state.cfg_flamethrower_weapon=true;
    for(int g=0;g<MAX_GUNS;++g){p->gun_data[g].visible=-1;p->gun_data[g].ammo=100;}
    p->gun_selected=AB3D_GUN_FLAMETHROWER;player_apply_weapon_config(&state);
    unsigned mask=0;for(int g=0;g<MAX_GUNS;++g)if(player_weapon_is_selectable(p,g))mask|=1u<<g;
    assert(mask==0x97 && GUN_KEY_COUNT==5 && !state.cfg_flamethrower_weapon && p->gun_selected==0);
    assert(strcmp(gun_names[0],"Mark IV Pulse Rifle")==0 && strcmp(gun_names[1],"Machine Gun")==0);
    puts("PASS grounded/flying sprites, blocked footsteps, eye sweep, portals, live doors and ceilings");
}
