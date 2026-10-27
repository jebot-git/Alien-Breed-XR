#pragma once
#include "game.hpp"
#include <nlohmann/json.hpp>
namespace tempest {
/* Explicit diagnostics only: compare loaded placements to the original file. */
nlohmann::json auditOriginalContent(const GameState &);
}
