#pragma once
#ifdef __cplusplus
extern "C" {
#endif
int abxr_startup(int argc, char **argv);
int abxr_level(void);
int abxr_quitting(void);
int abxr_status(void);
int abxr_poll(void *state);
void abxr_present(void *state);
void abxr_begin_shot(void *state);
void abxr_end_shot(void *state);
void abxr_shutdown(void);
#ifdef __cplusplus
}
#endif
