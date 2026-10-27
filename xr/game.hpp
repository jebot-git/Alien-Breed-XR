#pragma once
#define _Static_assert static_assert
extern "C" {
#include "game_state.h"
#include "input.h"
#include "level.h"
#include "math_tables.h"
#include "renderer.h"
int renderer_xr_decode_sprite(int, int, int, int, uint8_t *);
}
namespace tempest {
struct Game {
    GameState *state;
};
} // namespace tempest
