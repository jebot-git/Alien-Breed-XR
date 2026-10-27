#pragma once
#include "game.hpp"
#include <map>
#include <string>
#include <vector>
namespace tempest {
class Renderer {
    std::map<int, unsigned> textures;
    std::map<unsigned long long, unsigned> sprites;
    unsigned sprite(int vect, int frame, int width, int height);
    unsigned fontTexture = 0, digits[2]{}, cards[4]{};
    int fontWidth = 0, fontHeight = 0, digitWidth[2]{}, digitHeight[2]{};
    void loadOverlay();
    void text(const std::string &, float, float, float);
    void number(int, int, float, float, float);
    int cachedLevel = -1;
    std::map<const uint8_t *, unsigned> polygons;
    void floor(const LevelState &, const uint8_t *, int, float);
    unsigned texture(int id, int rows = 64);

  public:
    unsigned long triangles = 0;
    void world(const Game &game, bool xr);
    void finishFrame(const Game &, int, int, int, int, int);
    void desktop(const Game &, int w, int h);
    void clear();
    void invalidateLevel();
};
} // namespace tempest
