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
int abxr_accelerate(void *state, void *player, int32_t *x, int32_t *z, int speed, int running,
                    int run_percent);
int abxr_crouch_height(const void *state, const void *player);
int abxr_weapon_steps(const void *state, const void *player);
void abxr_level_loaded(void *state);
void abxr_present(void *state);
void abxr_begin_shot(void *state);
void abxr_end_shot(void *state);
void abxr_shutdown(void);
#ifdef __cplusplus
}
#endif
