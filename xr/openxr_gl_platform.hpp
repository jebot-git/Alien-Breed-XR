#pragma once
// Standard OpenXR OpenGL bindings. No compositor or streaming-client API.
#include <SDL.h>
#include <SDL_syswm.h>
#include <stdexcept>
#include <cstring>
#include <vector>
#ifdef _WIN32
#define XR_USE_PLATFORM_WIN32
#include <unknwn.h>
#undef near
#undef far
#else
#define XR_USE_PLATFORM_XLIB
#define XR_USE_PLATFORM_EGL
#include <EGL/egl.h>
#include <GL/glx.h>
#endif
#define XR_USE_GRAPHICS_API_OPENGL
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

namespace xr_platform {
struct Binding {
    bool egl = false;
#ifdef _WIN32
    XrGraphicsBindingOpenGLWin32KHR win{XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR};
#else
    XrGraphicsBindingOpenGLXlibKHR glx{XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR};
    XrGraphicsBindingEGLMNDX nativeEgl{XR_TYPE_GRAPHICS_BINDING_EGL_MNDX};
#endif
    explicit Binding(SDL_Window *window) {
        SDL_SysWMinfo wm{};
        SDL_VERSION(&wm.version);
#ifdef _WIN32
        if (!SDL_GetWindowWMInfo(window, &wm) || wm.subsystem != SDL_SYSWM_WINDOWS)
            throw std::runtime_error("OpenXR requires an SDL Win32/WGL window");
        win.hDC = wglGetCurrentDC();
        win.hGLRC = wglGetCurrentContext();
        if (!win.hDC || !win.hGLRC) throw std::runtime_error("No current WGL context");
#else
        egl = eglGetCurrentContext() != EGL_NO_CONTEXT;
        if (egl) {
            nativeEgl.display = eglGetCurrentDisplay();
            nativeEgl.context = eglGetCurrentContext();
            nativeEgl.getProcAddress = reinterpret_cast<decltype(nativeEgl.getProcAddress)>(eglGetProcAddress);
            EGLint id = 0, found = 0;
            eglQueryContext(nativeEgl.display, nativeEgl.context, EGL_CONFIG_ID, &id);
            EGLint attributes[] = {EGL_CONFIG_ID, id, EGL_NONE};
            if (!eglChooseConfig(nativeEgl.display, attributes, &nativeEgl.config, 1, &found) || !found)
                throw std::runtime_error("Could not identify SDL's EGL configuration");
        } else {
            if (!SDL_GetWindowWMInfo(window, &wm) || wm.subsystem != SDL_SYSWM_X11)
                throw std::runtime_error("OpenXR requires native EGL or X11/GLX");
            auto display = wm.info.x11.display;
            auto context = glXGetCurrentContext();
            int id = 0, count = 0;
            glXQueryContext(display, context, GLX_FBCONFIG_ID, &id);
            // SDL_Renderer may use a legacy GLX context whose FBConfig ID is -1.
            // Match its actual window visual instead of rejecting a valid context.
            XWindowAttributes attributes{};
            XGetWindowAttributes(display, wm.info.x11.window, &attributes);
            auto windowVisual = XVisualIDFromVisual(attributes.visual);
            auto configs = glXGetFBConfigs(display, DefaultScreen(display), &count);
            for (int i = 0; i < count; ++i) {
                int candidate = 0;
                glXGetFBConfigAttrib(display, configs[i], GLX_FBCONFIG_ID, &candidate);
                int visualId = 0;
                glXGetFBConfigAttrib(display, configs[i], GLX_VISUAL_ID, &visualId);
                if (candidate == id || (id <= 0 && VisualID(visualId) == windowVisual)) {
                    glx.glxFBConfig = configs[i];
                    if (auto visual = glXGetVisualFromFBConfig(display, configs[i])) {
                        glx.visualid = visual->visualid;
                        XFree(visual);
                    }
                    break;
                }
            }
            XFree(configs);
            if (!glx.glxFBConfig || !context) throw std::runtime_error("Could not identify SDL's GLX context");
            glx.xDisplay = display;
            glx.glxDrawable = glXGetCurrentDrawable();
            glx.glxContext = context;
        }
#endif
    }
    const void *get() const {
#ifdef _WIN32
        return &win;
#else
        return egl ? static_cast<const void *>(&nativeEgl) : static_cast<const void *>(&glx);
#endif
    }
};
} // namespace xr_platform
