#pragma once
#include "game.hpp"
#include <map>
namespace tempest {
/* Original Amiga indexed materials and brightness LUTs stay in VRAM. */
class PaletteLighting {
    unsigned program=0, points=0;
    int pointCount=1;
    const uint8_t *pointSource=nullptr;
    std::array<int16_t,3> animations{};
    std::map<const uint8_t *,unsigned> palettes;
    std::map<std::string,int> uniforms;
    std::map<int,float> values;
    bool active=false;
    unsigned boundPalette=0;
    int location(const char *);
    void scalar(const char *,float);
    void initialize();
public:
    enum Material { Wall, Floor, Sprite, Polygon };
    unsigned long paletteUploads=0,pointUpdates=0;
    void updatePoints(const LevelState &);
    void invalidatePoints() { pointSource=nullptr; }
    void material(const uint8_t *palette,size_t bytes,int colors,Material mode,
                  float brightness,bool perPoint=false,int upper=0,int maximum=64);
    void stop();
    void clear();
};
}
