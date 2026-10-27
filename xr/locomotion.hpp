#pragma once
#include <algorithm>
#include <cmath>
#include <openxr/openxr.h>

namespace tempest {
// OpenXR forward is -Z. Use its horizontal projection so pitch/roll cannot
// introduce vertical movement; keep the last heading when looking vertically.
inline float horizontalHeadYaw(XrQuaternionf q, float fallback) {
    float x = -2.f * (q.x * q.z + q.w * q.y);
    float z = 2.f * (q.x * q.x + q.y * q.y) - 1.f;
    if (!std::isfinite(x) || !std::isfinite(z) || std::hypot(x, z) < .05f)
        return fallback;
    return std::atan2(-x, -z);
}
// Return normalized world X/Z input. Stick +Y is forward, +X is right.
// Body yaw is only the virtual snap-turn offset, never a replacement for gaze.
inline XrVector2f headOrientedMovement(XrVector2f stick, float headYaw, float anchorYaw, float bodyYaw) {
    if (!std::isfinite(stick.x) || !std::isfinite(stick.y) || !std::isfinite(headYaw) ||
        !std::isfinite(anchorYaw) || !std::isfinite(bodyYaw))
        return {};
    constexpr float deadzone = .2f;
    float radius = std::hypot(stick.x, stick.y);
    if (radius <= deadzone)
        return {};
    float strength = std::min(1.f, (radius - deadzone) / (1.f - deadzone));
    float right = stick.x / radius * strength, forward = stick.y / radius * strength;
    float yaw = bodyYaw - headYaw + anchorYaw, c = std::cos(yaw), s = std::sin(yaw);
    return {c * right + s * forward, -s * right + c * forward};
}
inline float trackedBodyHeight(float headY, float anchorY, float standing = 1.5f, float minimum = 1.f) {
    if (!std::isfinite(headY) || !std::isfinite(anchorY))
        return standing;
    float drop = std::max(0.f, anchorY - headY - .04f);
    return std::clamp(standing - drop, minimum, standing);
}
inline float crouchViewCompensation(float bodyHeight, float physicalTarget, float standing = 1.5f) {
    return std::min(std::max(0.f, standing - bodyHeight), std::max(0.f, standing - physicalTarget));
}
} // namespace tempest
