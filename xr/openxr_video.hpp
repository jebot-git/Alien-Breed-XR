#pragma once
#include <SDL.h>
#include <openxr/openxr.h>
#include <cstring>
#include <vector>
namespace xr_platform {
inline bool supports(const char *extension) {
    uint32_t count = 0;
    if (XR_FAILED(xrEnumerateInstanceExtensionProperties(nullptr, 0, &count, nullptr)))
        return false;
    std::vector<XrExtensionProperties> properties(count, {XR_TYPE_EXTENSION_PROPERTIES});
    if (XR_FAILED(xrEnumerateInstanceExtensionProperties(nullptr, count, &count, properties.data())))
        return false;
    for (const auto &p : properties)
        if (!std::strcmp(p.extensionName, extension)) return true;
    return false;
}
inline void selectVideo(bool headset) {
#ifdef DE_XR_STANDALONE
    // Pick a context that can also serve a later desktop-to-XR toggle.
    headset = true;
#endif
#ifndef _WIN32
    // A generic GLX runtime need not implement the optional Monado EGL binding.
    // Preserve explicitly selected drivers, including DomeD's native Wayland path.
    if (headset && !SDL_getenv("SDL_VIDEODRIVER") && SDL_getenv("DISPLAY") &&
        supports("XR_KHR_opengl_enable") &&
        !supports("XR_MNDX_egl_enable")) {
        SDL_setenv("SDL_VIDEODRIVER", "x11", 0);
        SDL_setenv("SDL_VIDEO_X11_FORCE_EGL", "0", 0);
    }
#else
    (void)headset;
#endif
}
}
