/* Focused regression check; run with tools/check_feedback.py. */
#include "../src/player.c"
#include <assert.h>
/* Fixed test input: no mouse delta, no level sounds, 320x160 projection.
 * Movement, run selection, friction and level helpers use production code. */
void input_read_mouse(MouseState *m) { memset(m, 0, sizeof(*m)); }
void audio_play_sample(int sample, int volume) { (void)sample; (void)volume; }
int main(void) {
    math_tables_init();
    KeyBindings keys = {0};
    keys.forward=1; keys.backward=2; keys.sidestep_left=3; keys.sidestep_right=4;
    keys.run=5; keys.fire=6; keys.turn_left=7; keys.turn_right=8; keys.force_sidestep=9;
    for (int percent=50; percent<=200; percent+=10)
    for (int mouse=0; mouse<2; mouse++)
    for (int defaults=0; defaults<2; defaults++)
    for (int run=0; run<2; run++)
    for (int crouch=0; crouch<2; crouch++)
    for (int ticks=1; ticks<=15; ticks++)
    for (int direction=1; direction<=4; direction++) {
        static GameState state;
        memset(&state,0,sizeof(state));
        state.cfg_run_default=defaults;
        state.cfg_run_speed=percent;
        state.key_map[direction]=1;
        state.key_map[keys.run]=defaults ? !run : run;
        PlayerState *p=&state.plr1;
        p->ducked=crouch;
        if(mouse) player_mouse_kbd_control(p,state.key_map,&keys,&state,ticks);
        else player_keyboard_control(p,state.key_map,&keys,&state,ticks);
        int speed=(run?3:2);
        if(crouch) speed>>=1;
        speed*=ticks;
        int expected=16384*speed;
        if(run) expected=expected*percent/100;
        assert(p->s_angpos==0);
        if(direction==1) assert(p->s_zspdval==expected && p->s_xspdval==0);
        if(direction==2) assert(p->s_zspdval==-expected && p->s_xspdval==0);
        if(direction==3) assert(p->s_xspdval==-expected && p->s_zspdval==0);
        if(direction==4) assert(p->s_xspdval==expected && p->s_zspdval==0);
    }
    puts("PASS: 50..200% running, both movement modes, run modifier/default, walking, crouching, directions, tick batching");
}
RendererState g_renderer;
int renderer_get_width(void) { return 320; }
int renderer_get_height(void) { return 160; }
