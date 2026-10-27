#pragma once
#include <array>
#include <memory>
namespace tempest {
// GPU-resident glTF meshes. Only small node transforms change during playback.
class WeaponRenderer {
    struct Impl;
    std::unique_ptr<Impl> p;
  public:
    WeaponRenderer();
    ~WeaponRenderer();
    void loadAll();
    void clear();
    void draw(int slot, int remainingTicks);
    std::array<float,3> muzzle(int slot) const;
    unsigned loaded() const;
    unsigned long draws = 0, triangles = 0;
    unsigned slotsDrawn = 0;
    int spriteFrame = 0;
};
}
