#pragma once
#include <string>
#define _Static_assert static_assert
extern "C" {
#include "game_data.h"
#include "game_state.h"
#include "input.h"
#include "io.h"
#include "level.h"
#include "math_tables.h"
#include "movement.h"
#include "player.h"
#include "renderer.h"
}
namespace tempest {
enum PauseItem { PauseResume, PauseTurning, PauseHeight, PauseSave, PauseLoad, PauseQuit, PauseCount };
struct Overlay {
    bool menu = false, smoothTurn = true;
    int selection = 0;
    std::string message;
};
struct Game {
    GameState *state;
    float crouchCompensation = 0;
    const Overlay *overlay = nullptr;
    float wallFade = 0;
};
} // namespace tempest
