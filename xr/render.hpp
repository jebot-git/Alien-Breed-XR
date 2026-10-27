#pragma once
#include "game.hpp"
#include "weapon.hpp"
#include <map>
#include <array>
#include <string>
#include <vector>
namespace tempest {
class Renderer {
    std::map<int, unsigned> textures;
    std::map<unsigned long long, unsigned> sprites;
    unsigned sprite(int vect, int frame, int width, int height);
    bool billboard(const Game &,const XrBillboardInfo &,float x,float z);
    unsigned fontTexture = 0, digits[2]{}, cards[4]{};
    int fontWidth = 0, fontHeight = 0, digitWidth[2]{}, digitHeight[2]{};
    void loadOverlay();
    void text(const std::string &, float, float, float);
    void number(int, int, float, float, float);
    int cachedLevel = -1;
    std::map<const uint8_t *, unsigned> polygons;
    std::map<int,unsigned> vectorMeshes,vectorTextures;
    std::map<std::array<int,6>,unsigned> wallTextures;
    std::map<const uint8_t *,std::array<float,2>> switchOffsets;
    unsigned wallTexture(int id,int rows,int shift,int mask,int offset,bool holes);
    unsigned vectorTexture(int map,bool holes);
    void floor(const LevelState &, const uint8_t *, int, float);
    unsigned texture(int id, int rows = 64);

  public:
    void vectorObject(const GameObject *,float x,float z);
    WeaponRenderer weapons;
    unsigned long triangles = 0;
    unsigned long shotBillboardsRendered=0,explosionBillboardsRendered=0;
    unsigned long vectorObjectsRendered=0,vectorMeshesUploaded=0;
    unsigned long laserDraws=0,damageFlashDraws=0;
    unsigned long switchPanelsRendered=0;
    unsigned long switchPanelsMounted=0,switchMountErrors=0;
    void world(const Game &game, bool xr);
    void finishFrame(const Game &, int, int, int, int, int);
    void desktop(const Game &, int w, int h);
    void clear();
    void invalidateLevel();
};
} // namespace tempest
