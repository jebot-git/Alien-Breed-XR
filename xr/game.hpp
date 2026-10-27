#pragma once
#include <array>
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
enum PauseItem { PauseResume, PauseTurning, PauseHeight, PauseSave, PauseLoad, PauseTwoHand, PauseQuit, PauseCount };
enum class Presentation { Gameplay, Title, Level, Ending };
struct Overlay {
    bool menu = false, smoothTurn = true;
    bool twoHand=true;
    int selection = 0;
    std::string message;
    Presentation presentation = Presentation::Gameplay;
};
struct Game {
    GameState *state;
    float crouchCompensation = 0;
    const Overlay *overlay = nullptr;
    std::array<float,3> cameraOffset{}; // engine axes, metres, +Y up
};
} // namespace tempest
