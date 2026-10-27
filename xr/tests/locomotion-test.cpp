#include "locomotion.hpp"
#include <cassert>
#include <limits>
using namespace tempest;
namespace {
constexpr float pi = 3.141592653589793f;
void direction(XrVector2f input, float head, float anchor, float body, float x, float z) {
    auto result = headOrientedMovement(input, head, anchor, body);
    assert(std::abs(result.x - x) < .0001f && std::abs(result.y - z) < .0001f);
}
} // namespace
int main() {
    // Cardinal movements in the visible OpenGL world, independent of the
    // original engine's +Z keyboard-forward convention.
    direction({0, 1}, 0, 0, 0, 0, -1);
    direction({0, -1}, 0, 0, 0, 0, 1);
    direction({1, 0}, 0, 0, 0, 1, 0);
    direction({-1, 0}, 0, 0, 0, -1, 0);
    direction({0, 1}, pi / 2, 0, 0, -1, 0); // head turns left, forward follows gaze
    direction({1, 0}, pi / 2, 0, 0, 0, -1); // strafe right relative to that gaze
    direction({0, 1}, -pi / 2, 0, 0, 1, 0);
    direction({0, 1}, pi, 0, 0, 0, 1);
    direction({0, 1}, pi / 2, pi / 2, 0, 0, -1);      // recentered tracking origin
    direction({0, 1}, pi / 2, pi / 2, -pi / 2, 1, 0); // virtual right turn composes with tracking
    direction({0, 1}, pi / 2, 0, -pi / 2, 0, -1);     // physical and virtual turns cancel
    direction({.1f, .1f}, 0, 0, 0, 0, 0);
    direction({0, .6f}, 0, 0, 0, 0, -.5f); // analog speed after radial deadzone
    assert(std::abs(trackedBodyHeight(1.6f, 1.6f) - 1.5f) < .0001f);
    assert(std::abs(trackedBodyHeight(1.3f, 1.6f) - 1.24f) < .0001f);
    assert(std::abs(trackedBodyHeight(.7f, 1.6f) - 1.f) < .0001f);
    // Physical camera lowering must not be applied again by the body collider.
    assert(std::abs(1.5f - .26f + crouchViewCompensation(1.24f, 1.24f) - .3f - 1.2f) < .0001f);
    assert(crouchViewCompensation(1.f, 1.5f) == 0); // authored low ceiling still lowers camera
    auto diagonal = headOrientedMovement({1, 1}, 0, 0, 0);
    assert(std::abs(std::hypot(diagonal.x, diagonal.y) - 1) < .0001f);
    direction({std::numeric_limits<float>::quiet_NaN(), 1}, 0, 0, 0, 0, 0);
    float pitch = .7f;
    assert(std::abs(horizontalHeadYaw({std::sin(pitch / 2), 0, 0, std::cos(pitch / 2)}, 1)) < .0001f);
    assert(std::abs(horizontalHeadYaw({0, 0, std::sin(pitch / 2), std::cos(pitch / 2)}, 1)) < .0001f);
    assert(std::abs(horizontalHeadYaw({.70710678f, 0, 0, .70710678f}, .4f) - .4f) < .0001f);
}
