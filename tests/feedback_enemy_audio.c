/* Run real enemy sound paths with the mixer output captured for assertions. */
#define audio_play_sample feedback_backend_play_sample
#include "../src/audio.c"
#undef audio_play_sample
static int played_sample, played_volume, played_count;
void audio_play_sample(int sample, int volume)
{
    played_sample=sample; played_volume=volume; played_count++;
}
#include "../src/objects.c"
#include <assert.h>
#undef printf
GameState g_state;

int main(void)
{
    math_tables_init();
    static GameState state;
    static GameObject objects[2];
    static uint8_t points[16];
    state.level.object_data=(uint8_t*)objects;
    state.level.object_points=points;
    state.level.num_object_points=2;
    state.level.plr1_obj=(uint8_t*)&objects[1];
    state.level.plr2_obj=(uint8_t*)&objects[1];
    state.temp_frames=1;
    GameObject *obj=&objects[0];
    OBJ_SET_CID(obj,1); OBJ_SET_CID(&objects[1],-1);
    OBJ_SET_ZONE(obj,0);
    /* Emitter CID differs from its object slot; use ObjectPoints, not slot 0. */
    obj_sw(points+8,1600); obj_sw(points+12,-800);
    state.plr1.p_xoff=0; state.plr1.p_zoff=-800;
    state.plr2.p_xoff=1600; state.plr2.p_zoff=-800;
    const int types[]={OBJ_NBR_ALIEN,OBJ_NBR_ROBOT,OBJ_NBR_HUGE_RED_THING,
        OBJ_NBR_WORM,OBJ_NBR_FLAME_MARINE,OBJ_NBR_TOUGH_MARINE,OBJ_NBR_MARINE,
        OBJ_NBR_BIG_NASTY,OBJ_NBR_SMALL_RED_THING,OBJ_NBR_FLYING_NASTY,
        OBJ_NBR_EYEBALL,OBJ_NBR_TREE};
    for(int mode=0;mode<2;mode++) {
        state.mode=mode?MODE_SLAVE:MODE_SINGLE;
        int expected=mode?255:123; /* Noisevol=200 at distance 0 / 1600. */
        for(unsigned t=0;t<sizeof(types)/sizeof(types[0]);t++) {
            obj->obj.number=types[t];
            const EnemyParams *params=&enemy_params[obj_type_to_enemy_index(types[t])];
            NASTY_LIVES(*obj)=100;
            OBJ_SET_TD_W(obj,ENEMY_SEC_TIMER_OFF,32767);
            obj->raw[19]=(uint8_t)(1<<params->damage_shift);
            int before=played_count;
            assert(!enemy_check_damage(obj,params,&state));
            if(params->scream_sound>=0) {
                assert(played_count==before+1);
                assert(played_sample==params->scream_sound && played_volume==expected);
            }
            if(params->periodic_vocal1>=0 &&
                (params->hiss_timer_min>0 || params->hiss_timer_range>0)) {
                for(int attacking=0;attacking<2;attacking++) {
                    OBJ_SET_TD_W(obj,ENEMY_SEC_TIMER_OFF,0);
                    enemy_tick_sec_timer_vocal(obj,&state,params,attacking);
                    int noise=attacking?params->periodic_vol_attack:params->periodic_vol_idle;
                    assert(played_volume==audio_distance_volume(noise,mode?0:1600,0));
                    assert(played_sample>=params->periodic_vocal1 &&
                           played_sample<=(params->periodic_vocal2>=params->periodic_vocal1
                               ?params->periodic_vocal2:params->periodic_vocal1));
                }
            }
        }
        /* Marine death scream, without spawning debris. */
        obj->obj.number=OBJ_NBR_MARINE;
        enemy_death_marine_like(obj,&enemy_params[6],&state,1,false,0,false);
        assert(played_sample==0 && played_volume==expected);
        const int shooters[]={OBJ_NBR_MARINE,OBJ_NBR_FLAME_MARINE,
                              OBJ_NBR_TOUGH_MARINE,OBJ_NBR_ROBOT};
        state.nasty=1;
        for(unsigned i=0;i<sizeof(shooters)/sizeof(shooters[0]);i++) {
            memset(obj,0,sizeof(*obj));
            OBJ_SET_CID(obj,1); OBJ_SET_ZONE(obj,0);
            obj->obj.number=shooters[i]; obj->obj.can_see=1;
            NASTY_LIVES(*obj)=100;
            OBJ_SET_TD_W(obj,ENEMY_SEC_TIMER_OFF,32767);
            obj_sw(points+8,1600); obj_sw(points+12,-800);
            int before=played_count;
            if(shooters[i]==OBJ_NBR_ROBOT) object_handle_robot(obj,&state);
            else object_handle_marine(obj,&state);
            assert(played_count==before+1);
            int index=obj_type_to_enemy_index(shooters[i]);
            int sample=shooters[i]==OBJ_NBR_ROBOT?9:enemy_params[index].attack_sound;
            int noise=shooters[i]==OBJ_NBR_MARINE?200:100;
            const PlayerState *listener=mode?&state.plr2:&state.plr1;
            assert(played_sample==sample);
            assert(played_volume==audio_distance_volume(noise,
                obj_w(points+8)-listener->p_xoff,obj_w(points+12)-listener->p_zoff));
        }
        obj_sw(points+8,1600); obj_sw(points+12,-800);
        /* Gas emitter uses exactly the same listener and falloff path. */
        obj->obj.number=OBJ_NBR_GAS_PIPE;
        OBJ_SET_TD_W(obj,ENEMY_THIRD_TIMER_OFF,0);
        OBJ_SET_TD_W(obj,22,5); OBJ_SET_TD_W(obj,ENEMY_FOURTH_TIMER_OFF,0);
        object_handle_gas_pipe(obj,&state);
        assert(played_sample==22 && played_volume==expected);
    }
    int before=played_count;
    OBJ_SET_CID(obj,-1);
    object_play_noise(obj,&state,9,100);
    assert(played_count==before);
    puts("PASS: all 12 enemy types' pain/vocals, marine/robot attacks, marine death and fire spouts; both listeners and emitter CID lookup");
    return 0;
}
