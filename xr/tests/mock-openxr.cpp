// SPDX-License-Identifier: MIT
// Test-only API runtime adapted from de-xr Comic Chat's isolated mock.
// LD_PRELOAD applies only to the test child; no system runtime is contacted.
#define XR_USE_PLATFORM_EGL
#define XR_USE_GRAPHICS_API_OPENGL
#define GL_GLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <GL/gl.h>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <thread>
#include <vector>
namespace {
struct Chain {
    GLuint texture = 0;
    int width = 0, height = 0;
};
struct Space {
    const char *action;
    int hand;
};
unsigned frame = 0;
int pollStage = 0, stereoFrames = 0;
XrPosef tracked(int) { return {{0, 0, 0, 1}, {0, 1.6f, 0}}; }
XrResult requirements(XrInstance, XrSystemId, XrGraphicsRequirementsOpenGLKHR *out) {
    out->minApiVersionSupported = XR_MAKE_VERSION(3, 3, 0);
    out->maxApiVersionSupported = XR_MAKE_VERSION(4, 6, 0);
    return XR_SUCCESS;
}
void state(XrEventDataBuffer *out, XrSessionState state) {
    auto e = reinterpret_cast<XrEventDataSessionStateChanged *>(out);
    *e = {XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED};
    e->session = reinterpret_cast<XrSession>(2);
    e->state = state;
}
} // namespace
extern "C" {
XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateInstanceExtensionProperties(const char *, uint32_t capacity,
                                                                      uint32_t *count,
                                                                      XrExtensionProperties *out) {
    const char *names[] = {XR_KHR_OPENGL_ENABLE_EXTENSION_NAME, XR_MNDX_EGL_ENABLE_EXTENSION_NAME,
                           "XR_FB_touch_controller_pro"};
    *count = 3;
    if (!capacity)
        return XR_SUCCESS;
    if (capacity < 3)
        return XR_ERROR_SIZE_INSUFFICIENT;
    for (unsigned i = 0; i < 3; ++i) {
        out[i] = {XR_TYPE_EXTENSION_PROPERTIES};
        std::strcpy(out[i].extensionName, names[i]);
        out[i].extensionVersion = 1;
    }
    return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrCreateInstance(const XrInstanceCreateInfo *, XrInstance *out) {
    frame = 0;
    pollStage = 0;
    *out = reinterpret_cast<XrInstance>(1);
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrDestroyInstance(XrInstance) { return XR_SUCCESS; }

XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProperties(XrInstance, XrInstanceProperties *out) {
    strcpy(out->runtimeName, "Alien Breed isolated XR test");
    out->runtimeVersion = 1;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProcAddr(XrInstance, const char *name, PFN_xrVoidFunction *out) {
    if (!strcmp(name, "xrGetOpenGLGraphicsRequirementsKHR")) {
        *out = reinterpret_cast<PFN_xrVoidFunction>(requirements);
        return XR_SUCCESS;
    }
    *out = nullptr;
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetSystem(XrInstance, const XrSystemGetInfo *, XrSystemId *out) {
    *out = 1;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrCreateSession(XrInstance, const XrSessionCreateInfo *info, XrSession *out) {
    auto binding = static_cast<const XrGraphicsBindingEGLMNDX *>(info->next);
    if (!binding || binding->type != XR_TYPE_GRAPHICS_BINDING_EGL_MNDX)
        return XR_ERROR_GRAPHICS_DEVICE_INVALID;
    *out = reinterpret_cast<XrSession>(2);
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrDestroySession(XrSession) { return XR_SUCCESS; }

XRAPI_ATTR XrResult XRAPI_CALL xrCreateReferenceSpace(XrSession, const XrReferenceSpaceCreateInfo *,
                                                      XrSpace *out) {
    *out = reinterpret_cast<XrSpace>(new Space{"head", -1});
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrDestroySpace(XrSpace space) {
    delete reinterpret_cast<Space *>(space);
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateEnvironmentBlendModes(XrInstance, XrSystemId,
                                                                XrViewConfigurationType, uint32_t capacity,
                                                                uint32_t *count,
                                                                XrEnvironmentBlendMode *out) {
    *count = 1;
    if (capacity)
        out[0] = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateViewConfigurationViews(XrInstance, XrSystemId,
                                                                 XrViewConfigurationType, uint32_t capacity,
                                                                 uint32_t *count,
                                                                 XrViewConfigurationView *out) {
    *count = 2;
    if (capacity < 2)
        return capacity ? XR_ERROR_SIZE_INSUFFICIENT : XR_SUCCESS;
    for (int i = 0; i < 2; ++i) {
        out[i] = {XR_TYPE_VIEW_CONFIGURATION_VIEW};
        out[i].recommendedImageRectWidth = out[i].recommendedImageRectHeight = 128;
        out[i].maxImageRectWidth = out[i].maxImageRectHeight = 128;
        out[i].recommendedSwapchainSampleCount = out[i].maxSwapchainSampleCount = 1;
    }
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateSwapchainFormats(XrSession, uint32_t capacity, uint32_t *count,
                                                           int64_t *out) {
    *count = 1;
    if (capacity)
        out[0] = GL_SRGB8_ALPHA8;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrCreateSwapchain(XrSession, const XrSwapchainCreateInfo *info,
                                                 XrSwapchain *out) {
    auto c = new Chain;
    c->width = info->width;
    c->height = info->height;
    glGenTextures(1, &c->texture);
    glBindTexture(GL_TEXTURE_2D, c->texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, c->width, c->height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
    *out = reinterpret_cast<XrSwapchain>(c);
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrDestroySwapchain(XrSwapchain value) {
    auto c = reinterpret_cast<Chain *>(value);
    glDeleteTextures(1, &c->texture);
    delete c;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateSwapchainImages(XrSwapchain value, uint32_t capacity,
                                                          uint32_t *count, XrSwapchainImageBaseHeader *out) {
    *count = 1;
    if (capacity)
        reinterpret_cast<XrSwapchainImageOpenGLKHR *>(out)->image = reinterpret_cast<Chain *>(value)->texture;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrPollEvent(XrInstance, XrEventDataBuffer *out) {
    if (pollStage == 0) {
        ++pollStage;
        state(out, XR_SESSION_STATE_READY);
        return XR_SUCCESS;
    }
    if (pollStage == 1) {
        ++pollStage;
        state(out, XR_SESSION_STATE_FOCUSED);
        return XR_SUCCESS;
    }
    return XR_EVENT_UNAVAILABLE;
}

XRAPI_ATTR XrResult XRAPI_CALL xrBeginSession(XrSession, const XrSessionBeginInfo *) { return XR_SUCCESS; }

XRAPI_ATTR XrResult XRAPI_CALL xrEndSession(XrSession) { return XR_SUCCESS; }

XRAPI_ATTR XrResult XRAPI_CALL xrWaitFrame(XrSession, const XrFrameWaitInfo *, XrFrameState *out) {
    std::this_thread::sleep_for(std::chrono::milliseconds(8));
    ++frame;
    out->predictedDisplayTime = frame * 11111111LL;
    out->predictedDisplayPeriod = 11111111;
    out->shouldRender = XR_TRUE;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrBeginFrame(XrSession, const XrFrameBeginInfo *) { return XR_SUCCESS; }

XRAPI_ATTR XrResult XRAPI_CALL xrLocateViews(XrSession, const XrViewLocateInfo *, XrViewState *out,
                                             uint32_t capacity, uint32_t *count, XrView *views) {
    *count = 2;
    if (capacity < 2)
        return XR_ERROR_SIZE_INSUFFICIENT;
    out->viewStateFlags = XR_VIEW_STATE_POSITION_VALID_BIT | XR_VIEW_STATE_ORIENTATION_VALID_BIT;
    for (int i = 0; i < 2; ++i) {
        views[i].pose = tracked(-1);
        views[i].pose.position.x = (i ? .032f : -.032f) + .04f * std::sin(frame * .05f);
        if (std::getenv("ABXR_TEST_WALL") && frame>=20 && frame<=30)
            views[i].pose.position.x += 6;
        views[i].pose.position.y += .02f * std::sin(frame * .03f);
        if (!std::getenv("ABXR_TEST_MENU") && frame >= 76 && frame <= 94)
            views[i].pose.position.y -= .45f;
        views[i].pose.orientation = {.02f * std::sin(frame * .04f), 0, .015f * std::cos(frame * .03f), 1};
        auto &q = views[i].pose.orientation;
        float length = std::sqrt(q.x * q.x + q.z * q.z + 1);
        q.x /= length;
        q.z /= length;
        q.w /= length;
        if (frame >= 35)
            views[i].pose.orientation = {0, .70710678f, 0, .70710678f};
        views[i].fov = {-.65f, .65f, .65f, -.65f};
    }
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrAcquireSwapchainImage(XrSwapchain, const XrSwapchainImageAcquireInfo *,
                                                       uint32_t *out) {
    *out = 0;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrWaitSwapchainImage(XrSwapchain, const XrSwapchainImageWaitInfo *) {
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrReleaseSwapchainImage(XrSwapchain, const XrSwapchainImageReleaseInfo *) {
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrEndFrame(XrSession, const XrFrameEndInfo *info) {
    if (const char *path = std::getenv("TEMPEST_TEST_FRAMES")) {
        static std::ofstream log(path, std::ios::app);
        log.precision(16);
        log << std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count()
            << " " << info->layerCount << "\n";
        log.flush();
    }
    if (info->layerCount) {
        auto layer = reinterpret_cast<const XrCompositionLayerProjection *>(info->layers[0]);
        if (layer->viewCount != 2)
            return XR_ERROR_VALIDATION_FAILURE;
        if (const char *file = std::getenv("ABXR_TEST_EYES")) {
            std::ofstream out(file, std::ios::app);
            out << frame;
            for (unsigned eye = 0; eye < 2; ++eye) {
                auto chain = reinterpret_cast<Chain *>(layer->views[eye].subImage.swapchain);
                std::vector<unsigned char> pixels(chain->width * chain->height * 4);
                glBindTexture(GL_TEXTURE_2D, chain->texture);
                glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                unsigned long long hash = 1469598103934665603ULL;
                for (auto byte : pixels) {
                    hash ^= byte;
                    hash *= 1099511628211ULL;
                }
                out << " " << hash;
                if (frame == 6 || frame == 60) {
                    std::ofstream image(std::string(file) + (frame == 6 ? ".frame6" : "") + (eye ? ".right.ppm" : ".left.ppm"),
                                        std::ios::binary);
                    image << "P6\n" << chain->width << " " << chain->height << "\n255\n";
                    for (int y = chain->height - 1; y >= 0; --y)
                        for (int x = 0; x < chain->width; ++x)
                            image.write(
                                reinterpret_cast<const char *>(pixels.data() + (y * chain->width + x) * 4),
                                3);
                }
            }
            out << "\n";
        }
        ++stereoFrames;
    }
    return XR_SUCCESS;
}
}
#include <cmath>
#include <map>
#include <string>
#include <vector>
namespace {
std::map<std::string, XrPath> paths;
struct Action {
    std::string name;
};
std::vector<Action *> actions;
XrPath path(const char *s) {
    auto [it, inserted] = paths.emplace(s, paths.size() + 1);
    return it->second;
}
} // namespace
extern "C" {
XRAPI_ATTR XrResult XRAPI_CALL xrStringToPath(XrInstance, const char *s, XrPath *out) {
    *out = path(s);
    return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrCreateActionSet(XrInstance, const XrActionSetCreateInfo *,
                                                 XrActionSet *out) {
    *out = reinterpret_cast<XrActionSet>(3);
    return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrDestroyActionSet(XrActionSet) {
    for (auto a : actions)
        delete a;
    actions.clear();
    return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrCreateAction(XrActionSet, const XrActionCreateInfo *info, XrAction *out) {
    auto a = new Action{info->actionName};
    actions.push_back(a);
    *out = reinterpret_cast<XrAction>(a);
    return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL
xrSuggestInteractionProfileBindings(XrInstance, const XrInteractionProfileSuggestedBinding *info) {
    if (info->interactionProfile == path("/interaction_profiles/oculus/touch_controller") ||
        info->interactionProfile == path("/interaction_profiles/facebook/touch_controller_pro")) {
        bool right = false, left = false, previous = false, next = false;
        for (unsigned i = 0; i < info->countSuggestedBindings; ++i) {
            auto binding = info->suggestedBindings[i];
            if (reinterpret_cast<Action *>(binding.action)->name == "use") {
                right |= binding.binding == path("/user/hand/right/input/a/click");
                left |= binding.binding == path("/user/hand/left/input/x/click");
            }
            if (reinterpret_cast<Action *>(binding.action)->name == "previous_weapon")
                previous |= binding.binding == path("/user/hand/left/input/y/click");
            if (reinterpret_cast<Action *>(binding.action)->name == "next_weapon")
                next |= binding.binding == path("/user/hand/right/input/b/click");
        }
        if (!right || !left || !previous || !next)
            return XR_ERROR_VALIDATION_FAILURE;
    }
    return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrAttachSessionActionSets(XrSession, const XrSessionActionSetsAttachInfo *) {
    return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrCreateActionSpace(XrSession, const XrActionSpaceCreateInfo *info,
                                                   XrSpace *out) {
    *out =
        reinterpret_cast<XrSpace>(new Space{"aim", info->subactionPath == path("/user/hand/right") ? 1 : 0});
    return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrSyncActions(XrSession, const XrActionsSyncInfo *) { return XR_SUCCESS; }
XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStatePose(XrSession, const XrActionStateGetInfo *,
                                                    XrActionStatePose *out) {
    out->isActive = XR_TRUE;
    return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateFloat(XrSession, const XrActionStateGetInfo *,
                                                     XrActionStateFloat *out) {
    out->isActive = XR_TRUE;
    out->currentState = !std::getenv("ABXR_TEST_MENU") && (frame > 15 && frame < 75) ? 1.f : 0.f;
    return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateVector2f(XrSession, const XrActionStateGetInfo *info,
                                                        XrActionStateVector2f *out) {
    out->isActive = XR_TRUE;
    out->currentState = {0, 0};
    if (std::getenv("ABXR_TEST_MENU")) {
        if (info->subactionPath == path("/user/hand/left") &&
            (frame == 18 || frame == 32 || frame == 46 || frame == 70))
            out->currentState.y = -1;
        if (info->subactionPath == path("/user/hand/right") && frame >= 55 && frame <= 61)
            out->currentState.x = .7f;
    } else if (info->subactionPath == path("/user/hand/left") && frame > 40 && frame < 55)
        out->currentState.y = .5f;
    if (!std::getenv("ABXR_TEST_MENU") && info->subactionPath == path("/user/hand/right") && frame >= 98 &&
        frame <= 105)
        out->currentState.x = 1;
    return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateBoolean(XrSession, const XrActionStateGetInfo *info,
                                                       XrActionStateBoolean *out) {
    out->isActive = XR_TRUE;
    bool right = info->subactionPath == path("/user/hand/right");
    auto name = reinterpret_cast<Action *>(info->action)->name;
    if (std::getenv("ABXR_TEST_MENU")) {
        out->currentState =
            (name == "pause" && !right && (frame == 10 || frame == 64)) ||
            (name == "use" && right && (frame == 24 || frame == 38 || frame == 52 || frame == 76));
    } else {
        out->currentState = (name == "use" && (right ? frame >= 20 && frame <= 28 : frame == 68)) ||
                            (name == "previous_weapon" && !right && frame == 30) ||
                            (name == "next_weapon" && right && frame == 60);
    }
    return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrLocateSpace(XrSpace space, XrSpace, XrTime, XrSpaceLocation *out) {
    auto s = reinterpret_cast<Space *>(space);
    float yaw = .12f * std::sin(frame * .04f);
    out->pose = {{0, std::sin(yaw / 2), 0, std::cos(yaw / 2)}, {s->hand ? .2f : -.2f, 1.35f, -.25f}};
    out->locationFlags = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT |
                         XR_SPACE_LOCATION_POSITION_TRACKED_BIT | XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
    return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrApplyHapticFeedback(XrSession, const XrHapticActionInfo *,
                                                     const XrHapticBaseHeader *) {
    return XR_SUCCESS;
}
}
