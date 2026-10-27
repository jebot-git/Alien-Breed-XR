#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int abxr_startup(int argc, char **argv);
int abxr_level(void);
int abxr_quitting(void);
int abxr_status(void);
int abxr_poll(void *state);
int abxr_accelerate(void *state, void *player, int32_t *x, int32_t *z, int ticks, int height,
                    int run_percent);
int abxr_active_controls(const void *state,const void *player);
int abxr_crouch_height(const void *state, const void *player);
int abxr_weapon_steps(const void *state, const void *player);
void abxr_level_loaded(void *state);
void abxr_present(void *state);
int abxr_aim_active(const void *state,const void *player);
int abxr_shot_direction(const void *state,const void *player,float *x,float *y,float *z);
void abxr_logic_tick(void *state);
void abxr_shot_fired(void *state,const void *player,int gun);
void abxr_begin_shot(void *state);
void abxr_end_shot(void *state);
void abxr_shutdown(void);
#ifdef __cplusplus
}
#endif
