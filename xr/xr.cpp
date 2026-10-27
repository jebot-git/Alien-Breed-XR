#include "xr.hpp"
#include "aim.hpp"
#include "locomotion.hpp"
#include "render.hpp"
#include <SDL.h>
#include <SDL_opengl.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
#ifdef TEMPEST_OPENXR
#include <SDL_syswm.h>
#define XR_USE_PLATFORM_XLIB
#define XR_USE_PLATFORM_EGL
#define XR_USE_GRAPHICS_API_OPENGL
#include <EGL/egl.h>
#include <GL/glx.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

namespace tempest {
static void check(XrResult r, const char *name) {
    if (XR_FAILED(r))
        throw std::runtime_error(std::string(name) + " (OpenXR " + std::to_string(r) + ")");
}
#define XR_CHECK(call) check(call, #call)
struct XR::Impl {
    XrInstance instance = XR_NULL_HANDLE;
    XrSession session = XR_NULL_HANDLE;
    XrSpace space = XR_NULL_HANDLE;
    XrSystemId system = XR_NULL_SYSTEM_ID;
    bool running = false, hasAnchor = false, quit = false, isFocused = false, visible = true;
    XrEnvironmentBlendMode blend = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    float wallFade = 0;
    struct Eye {
        XrSwapchain chain = XR_NULL_HANDLE;
        int width = 0, height = 0;
        std::vector<XrSwapchainImageOpenGLKHR> images;
        GLuint depth = 0;
    };
    std::vector<Eye> eyes;
    GLuint fbo = 0;
    XrVector3f anchor{};
    bool heightCalibrated = false;
    float standingY = 0;
    float yaw = 0;
    std::string runtime;
    uint64_t frames = 0, stereoFrames = 0;
    Tracking tracking;
    bool touchPro = false;
    XrActionSet actions = XR_NULL_HANDLE;
    XrAction pose = XR_NULL_HANDLE, trigger = XR_NULL_HANDLE, stick = XR_NULL_HANDLE,
             operate = XR_NULL_HANDLE, menu = XR_NULL_HANDLE, previous = XR_NULL_HANDLE,
             next = XR_NULL_HANDLE, select = XR_NULL_HANDLE, haptic = XR_NULL_HANDLE;
    XrPath handPaths[2]{};
    XrSpace handSpaces[2]{};
    ~Impl() {
        if (instance)
            std::cerr << "OpenXR frames=" << frames << " stereo_frames=" << stereoFrames << '\n';
        if (fbo)
            glDeleteFramebuffers(1, &fbo);
        for (auto &e : eyes) {
            if (e.depth)
                glDeleteRenderbuffers(1, &e.depth);
            if (e.chain)
                xrDestroySwapchain(e.chain);
        }
        for (auto hand : handSpaces)
            if (hand)
                xrDestroySpace(hand);
        if (space)
            xrDestroySpace(space);

        if (session)
            xrDestroySession(session);
        if (actions)
            xrDestroyActionSet(actions);
        if (instance)
            xrDestroyInstance(instance);
    }
};
XR::XR() : p(std::make_unique<Impl>()) {}
XR::~XR() = default;
void XR::initialize(SDL_Window *window) {
    SDL_SysWMinfo wm{};
    SDL_VERSION(&wm.version);
    auto &x = *p;
    bool useEgl = eglGetCurrentContext() != EGL_NO_CONTEXT;
    bool hasWindowInfo = SDL_GetWindowWMInfo(window, &wm);
    if (!useEgl && !hasWindowInfo)
        throw std::runtime_error("SDL could not identify the native window");
    if (!useEgl && wm.subsystem != SDL_SYSWM_X11)
        throw std::runtime_error("XR requires a native EGL or X11/GLX OpenGL context");
    uint32_t extensionCount = 0;
    XR_CHECK(xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionCount, nullptr));
    std::vector<XrExtensionProperties> available(extensionCount, {XR_TYPE_EXTENSION_PROPERTIES});
    XR_CHECK(
        xrEnumerateInstanceExtensionProperties(nullptr, extensionCount, &extensionCount, available.data()));
    std::vector<const char *> extensions{XR_KHR_OPENGL_ENABLE_EXTENSION_NAME};
    if (useEgl)
        extensions.push_back(XR_MNDX_EGL_ENABLE_EXTENSION_NAME);
    x.touchPro = std::any_of(available.begin(), available.end(), [](const auto &item) {
        return std::strcmp(item.extensionName, "XR_FB_touch_controller_pro") == 0;
    });
    if (x.touchPro)
        extensions.push_back("XR_FB_touch_controller_pro");
    XrInstanceCreateInfo ci{XR_TYPE_INSTANCE_CREATE_INFO};
    std::strcpy(ci.applicationInfo.applicationName, "Alien Breed XR");
    ci.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);
    ci.enabledExtensionCount = extensions.size();
    ci.enabledExtensionNames = extensions.data();
    XR_CHECK(xrCreateInstance(&ci, &x.instance));
    XrInstanceProperties properties{XR_TYPE_INSTANCE_PROPERTIES};
    XR_CHECK(xrGetInstanceProperties(x.instance, &properties));
    x.runtime = properties.runtimeName;
    XrSystemGetInfo si{XR_TYPE_SYSTEM_GET_INFO};
    si.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XR_CHECK(xrGetSystem(x.instance, &si, &x.system));
    PFN_xrGetOpenGLGraphicsRequirementsKHR requirementsFn = nullptr;
    XR_CHECK(xrGetInstanceProcAddr(x.instance, "xrGetOpenGLGraphicsRequirementsKHR",
                                   reinterpret_cast<PFN_xrVoidFunction *>(&requirementsFn)));
    XrGraphicsRequirementsOpenGLKHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
    XR_CHECK(requirementsFn(x.instance, x.system, &requirements));
    GLint major = 0, minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    auto version = XR_MAKE_VERSION(major, minor, 0);
    if (version < requirements.minApiVersionSupported || version > requirements.maxApiVersionSupported)
        throw std::runtime_error("OpenGL version is outside the OpenXR runtime's supported range");
    XrGraphicsBindingOpenGLXlibKHR binding{XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR};
    XrGraphicsBindingEGLMNDX eglBinding{XR_TYPE_GRAPHICS_BINDING_EGL_MNDX};
    if (useEgl) {
        eglBinding.display = eglGetCurrentDisplay();
        eglBinding.context = eglGetCurrentContext();
        eglBinding.getProcAddress = reinterpret_cast<PFN_xrEglGetProcAddressMNDX>(eglGetProcAddress);
        EGLint configId = 0, found = 0;
        eglQueryContext(eglBinding.display, eglBinding.context, EGL_CONFIG_ID, &configId);
        EGLint attributes[] = {EGL_CONFIG_ID, configId, EGL_NONE};
        if (!eglChooseConfig(eglBinding.display, attributes, &eglBinding.config, 1, &found) || !found)
            throw std::runtime_error("Could not identify SDL's EGL configuration");
    } else {
        Display *display = wm.info.x11.display;
        GLXContext context = glXGetCurrentContext();
        int id = 0;
        glXQueryContext(display, context, GLX_FBCONFIG_ID, &id);
        int count = 0;
        auto configs = glXGetFBConfigs(display, DefaultScreen(display), &count);
        GLXFBConfig config = nullptr;
        VisualID visual = 0;
        for (int i = 0; i < count; i++) {
            int candidate = 0;
            glXGetFBConfigAttrib(display, configs[i], GLX_FBCONFIG_ID, &candidate);
            if (candidate == id) {
                config = configs[i];
                auto vi = glXGetVisualFromFBConfig(display, config);
                if (vi) {
                    visual = vi->visualid;
                    XFree(vi);
                }
                break;
            }
        }
        XFree(configs);
        if (!config || !context)
            throw std::runtime_error("Could not identify the SDL GLX context");
        binding.xDisplay = display;
        binding.visualid = visual;
        binding.glxFBConfig = config;
        binding.glxDrawable = glXGetCurrentDrawable();
        binding.glxContext = context;
    }
    XrSessionCreateInfo sessionInfo{XR_TYPE_SESSION_CREATE_INFO};
    sessionInfo.next = useEgl ? static_cast<void *>(&eglBinding) : static_cast<void *>(&binding);
    sessionInfo.systemId = x.system;
    XR_CHECK(xrCreateSession(x.instance, &sessionInfo, &x.session));
    XrReferenceSpaceCreateInfo ri{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    ri.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    ri.poseInReferenceSpace.orientation.w = 1;
    XR_CHECK(xrCreateReferenceSpace(x.session, &ri, &x.space));
    uint32_t n = 0;
    XR_CHECK(xrEnumerateEnvironmentBlendModes(x.instance, x.system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                              0, &n, nullptr));
    std::vector<XrEnvironmentBlendMode> blends(n);
    XR_CHECK(xrEnumerateEnvironmentBlendModes(x.instance, x.system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                              n, &n, blends.data()));
    if (blends.empty())
        throw std::runtime_error("No OpenXR blend modes");
    x.blend = blends.front();
    XR_CHECK(xrEnumerateSwapchainFormats(x.session, 0, &n, nullptr));
    std::vector<int64_t> formats(n);
    XR_CHECK(xrEnumerateSwapchainFormats(x.session, n, &n, formats.data()));
    int64_t format = 0;
    for (auto wanted : {GL_RGBA8, GL_SRGB8_ALPHA8})
        if (std::find(formats.begin(), formats.end(), wanted) != formats.end()) {
            format = wanted;
            break;
        }
    if (!format)
        throw std::runtime_error("No supported OpenGL RGBA swapchain format");
    XR_CHECK(xrEnumerateViewConfigurationViews(x.instance, x.system,
                                               XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &n, nullptr));
    std::vector<XrViewConfigurationView> views(n, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
    XR_CHECK(xrEnumerateViewConfigurationViews(
        x.instance, x.system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, n, &n, views.data()));
    if (n != 2)
        throw std::runtime_error("Prototype requires stereo OpenXR views");
    x.eyes.resize(n);
    for (size_t i = 0; i < x.eyes.size(); i++) {
        auto &e = x.eyes[i];
        e.width = int(views[i].recommendedImageRectWidth);
        e.height = int(views[i].recommendedImageRectHeight);
        XrSwapchainCreateInfo sc{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        sc.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        sc.format = format;
        sc.sampleCount = 1;
        sc.width = e.width;
        sc.height = e.height;
        sc.faceCount = 1;
        sc.arraySize = 1;
        sc.mipCount = 1;
        XR_CHECK(xrCreateSwapchain(x.session, &sc, &e.chain));
        XR_CHECK(xrEnumerateSwapchainImages(e.chain, 0, &n, nullptr));
        e.images.resize(n, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR});
        XR_CHECK(xrEnumerateSwapchainImages(e.chain, n, &n,
                                            reinterpret_cast<XrSwapchainImageBaseHeader *>(e.images.data())));
        glGenRenderbuffers(1, &e.depth);
        glBindRenderbuffer(GL_RENDERBUFFER, e.depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, e.width, e.height);
    }
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    initializeActions();
    glGenFramebuffers(1, &x.fbo);
    std::cerr << "OpenXR initialized: " << x.runtime << " / " << x.eyes[0].width << "x" << x.eyes[0].height
              << " per eye\n";
}
void XR::initializeActions() {
    auto &x = *p;
    auto path = [&](const char *name) {
        XrPath value;
        XR_CHECK(xrStringToPath(x.instance, name, &value));
        return value;
    };
    x.handPaths[0] = path("/user/hand/left");
    x.handPaths[1] = path("/user/hand/right");
    XrActionSetCreateInfo set{XR_TYPE_ACTION_SET_CREATE_INFO};
    std::strcpy(set.actionSetName, "gameplay");
    std::strcpy(set.localizedActionSetName, "Gameplay");
    XR_CHECK(xrCreateActionSet(x.instance, &set, &x.actions));
    auto action = [&](XrAction &out, const char *name, XrActionType type) {
        XrActionCreateInfo info{XR_TYPE_ACTION_CREATE_INFO};
        info.actionType = type;
        std::strcpy(info.actionName, name);
        std::strcpy(info.localizedActionName, std::strcmp(name, "use") == 0 ? "Use / Operate" : name);
        info.countSubactionPaths = 2;
        info.subactionPaths = x.handPaths;
        XR_CHECK(xrCreateAction(x.actions, &info, &out));
    };
    action(x.pose, "aim_pose", XR_ACTION_TYPE_POSE_INPUT);
    action(x.trigger, "trigger", XR_ACTION_TYPE_FLOAT_INPUT);
    action(x.stick, "move_turn", XR_ACTION_TYPE_VECTOR2F_INPUT);
    action(x.operate, "use", XR_ACTION_TYPE_BOOLEAN_INPUT);
    action(x.menu, "pause", XR_ACTION_TYPE_BOOLEAN_INPUT);
    action(x.previous, "previous_weapon", XR_ACTION_TYPE_BOOLEAN_INPUT);
    action(x.next, "next_weapon", XR_ACTION_TYPE_BOOLEAN_INPUT);
    action(x.select, "select", XR_ACTION_TYPE_BOOLEAN_INPUT);
    action(x.haptic, "feedback", XR_ACTION_TYPE_VIBRATION_OUTPUT);
    std::vector<const char *> profiles{
        "/interaction_profiles/oculus/touch_controller", "/interaction_profiles/valve/index_controller",
        "/interaction_profiles/htc/vive_controller", "/interaction_profiles/khr/simple_controller"};
    if (x.touchPro)
        profiles.push_back("/interaction_profiles/facebook/touch_controller_pro");
    for (auto profile : profiles) {
        bool simple = std::strstr(profile, "simple"), vive = std::strstr(profile, "vive");
        std::vector<XrActionSuggestedBinding> bindings;
        auto bind = [&](XrAction a, const std::string &b) { bindings.push_back({a, path(b.c_str())}); };
        for (int h = 0; h < 2; ++h) {
            std::string hand = h ? "/user/hand/right" : "/user/hand/left";
            bind(x.pose, hand + "/input/aim/pose");
            bind(x.haptic, hand + "/output/haptic");
            if (simple)
                bind(x.select, hand + "/input/select/click");
            else {
                bind(x.trigger, hand + "/input/trigger/value");
                bind(x.stick, hand + (vive ? "/input/trackpad" : "/input/thumbstick"));
            }
            if (!simple)
                bind(x.operate, hand + (vive ? "/input/squeeze/click"
                                        : h  ? "/input/a/click"
                                             : "/input/x/click"));
        }
        bind(x.menu, "/user/hand/left/input/menu/click");
        if (!simple && !vive) {
            bind(x.previous, "/user/hand/left/input/y/click");
            bind(x.next, "/user/hand/right/input/b/click");
        }
        // Index has no menu path or A/X naming distinction; use its A/B buttons.
        if (std::strstr(profile, "index")) {
            bindings.clear();
            for (int h = 0; h < 2; ++h) {
                std::string hand = h ? "/user/hand/right" : "/user/hand/left";
                bind(x.pose, hand + "/input/aim/pose");
                bind(x.haptic, hand + "/output/haptic");
                bind(x.trigger, hand + "/input/trigger/value");
                bind(x.stick, hand + "/input/thumbstick");
                bind(x.operate, hand + "/input/a/click");
            }
            bind(x.menu, "/user/hand/left/input/thumbstick/click");
            bind(x.previous, "/user/hand/left/input/b/click");
            bind(x.next, "/user/hand/right/input/b/click");
        }
        XrInteractionProfileSuggestedBinding info{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        info.interactionProfile = path(profile);
        info.countSuggestedBindings = bindings.size();
        info.suggestedBindings = bindings.data();
        auto result = xrSuggestInteractionProfileBindings(x.instance, &info);
        if (XR_FAILED(result))
            std::cerr << "Controller profile unavailable: " << profile << " (" << result << ")\n";
    }
    XrSessionActionSetsAttachInfo attach{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    attach.countActionSets = 1;
    attach.actionSets = &x.actions;
    XR_CHECK(xrAttachSessionActionSets(x.session, &attach));
    for (int h = 0; h < 2; ++h) {
        XrActionSpaceCreateInfo info{XR_TYPE_ACTION_SPACE_CREATE_INFO};
        info.action = x.pose;
        info.subactionPath = x.handPaths[h];
        info.poseInActionSpace.orientation.w = 1;
        XR_CHECK(xrCreateActionSpace(x.session, &info, &x.handSpaces[h]));
    }
}
void XR::sampleActions(XrTime time) {
    auto &x = *p;
    auto &t = x.tracking;
    bool wasFire = t.fire;
    t.fire = t.use = t.menu = t.previousWeapon = t.nextWeapon = false;
    t.move = {};
    t.turn = {};
    t.valid[0] = t.valid[1] = false;
    XrActiveActionSet active{x.actions, XR_NULL_PATH};
    XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO};
    sync.countActiveActionSets = 1;
    sync.activeActionSets = &active;
    XR_CHECK(xrSyncActions(x.session, &sync));
    if (!x.isFocused || !x.visible || !t.headValid)
        return;
    for (int h = 0; h < 2; ++h) {
        XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};
        get.subactionPath = x.handPaths[h];
        get.action = x.pose;
        XrActionStatePose pose{XR_TYPE_ACTION_STATE_POSE};
        XR_CHECK(xrGetActionStatePose(x.session, &get, &pose));
        XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};
        XR_CHECK(xrLocateSpace(x.handSpaces[h], x.space, time, &location));
        auto required = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
        t.valid[h] = pose.isActive && (location.locationFlags & required) == required;
        t.hands[h] = location.pose;
        get.action = x.stick;
        XrActionStateVector2f stick{XR_TYPE_ACTION_STATE_VECTOR2F};
        XR_CHECK(xrGetActionStateVector2f(x.session, &get, &stick));
        if (stick.isActive) {
            if (h)
                t.turn = stick.currentState;
            else
                t.move = stick.currentState;
        }
        auto boolean = [&](XrAction action) {
            get.action = action;
            XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
            XR_CHECK(xrGetActionStateBoolean(x.session, &get, &state));
            return state.isActive && state.currentState;
        };
        bool clicked = boolean(x.select);
        if (h)
            t.fire = clicked;
        else
            t.use = clicked;
        t.use = t.use || boolean(x.operate);
        t.menu = t.menu || boolean(x.menu);
        t.previousWeapon = t.previousWeapon || boolean(x.previous);
        t.nextWeapon = t.nextWeapon || boolean(x.next);
        get.action = x.trigger;
        XrActionStateFloat trigger{XR_TYPE_ACTION_STATE_FLOAT};
        XR_CHECK(xrGetActionStateFloat(x.session, &get, &trigger));
        if (h && t.valid[h])
            t.fire = t.fire || (trigger.isActive && trigger.currentState > .55f);
    }
    if (!t.valid[1])
        t.fire = false;
    if (t.fire && !wasFire) {
        XrHapticActionInfo action{XR_TYPE_HAPTIC_ACTION_INFO};
        action.action = x.haptic;
        action.subactionPath = x.handPaths[1];
        XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
        vibration.duration = 30000000;
        vibration.amplitude = .35f;
        vibration.frequency = XR_FREQUENCY_UNSPECIFIED;
        xrApplyHapticFeedback(x.session, &action, reinterpret_cast<const XrHapticBaseHeader *>(&vibration));
    }
}
// Inverse rigid pose, OpenGL column-major.
static void inversePose(const XrPosef &pose) {
    auto q = pose.orientation;
    float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z, xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z,
          wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
    GLfloat m[] = {1 - 2 * (yy + zz),
                   2 * (xy - wz),
                   2 * (xz + wy),
                   0,
                   2 * (xy + wz),
                   1 - 2 * (xx + zz),
                   2 * (yz - wx),
                   0,
                   2 * (xz - wy),
                   2 * (yz + wx),
                   1 - 2 * (xx + yy),
                   0,
                   0,
                   0,
                   0,
                   1};
    glMultMatrixf(m);
    glTranslatef(-pose.position.x, -pose.position.y, -pose.position.z);
}
bool XR::frame(Renderer &renderer, const Game &game) {
    auto &x = *p;
    XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
    XrResult poll;
    while ((poll = xrPollEvent(x.instance, &event)) == XR_SUCCESS) {
        if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            auto state = reinterpret_cast<XrEventDataSessionStateChanged &>(event).state;
            x.isFocused = state == XR_SESSION_STATE_FOCUSED;
            if (state == XR_SESSION_STATE_READY) {
                XrSessionBeginInfo bi{XR_TYPE_SESSION_BEGIN_INFO};
                bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                XR_CHECK(xrBeginSession(x.session, &bi));
                x.running = true;
            }
            if (state == XR_SESSION_STATE_STOPPING) {
                x.running = false;
                XR_CHECK(xrEndSession(x.session));
            }
            if (state == XR_SESSION_STATE_EXITING || state == XR_SESSION_STATE_LOSS_PENDING)
                x.quit = true;
        } else if (event.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING)
            x.quit = true;
        else if (event.type == XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING)
            x.hasAnchor = false;
        event = {XR_TYPE_EVENT_DATA_BUFFER};
    }
    if (poll != XR_EVENT_UNAVAILABLE)
        check(poll, "xrPollEvent");
    if (x.quit)
        return false;
    if (!x.running)
        return true;
    XrFrameWaitInfo wi{XR_TYPE_FRAME_WAIT_INFO};
    XrFrameState fs{XR_TYPE_FRAME_STATE};
    XR_CHECK(xrWaitFrame(x.session, &wi, &fs));
    XrFrameBeginInfo bi{XR_TYPE_FRAME_BEGIN_INFO};
    XR_CHECK(xrBeginFrame(x.session, &bi));
    std::array<XrView, 2> views{{{XR_TYPE_VIEW}, {XR_TYPE_VIEW}}};
    std::array<XrCompositionLayerProjectionView, 2> projectionViews{
        {{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}, {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}}};
    XrViewLocateInfo li{XR_TYPE_VIEW_LOCATE_INFO};
    li.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    li.displayTime = fs.predictedDisplayTime;
    li.space = x.space;
    XrViewState vs{XR_TYPE_VIEW_STATE};
    uint32_t n = 0;
    XR_CHECK(xrLocateViews(x.session, &li, &vs, 2, &n, views.data()));
    bool render = x.visible && fs.shouldRender && n == 2 &&
                  (vs.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) &&
                  (vs.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT);
    x.tracking.headValid = render;
    sampleActions(fs.predictedDisplayTime);
    if (render) {

        if (!x.hasAnchor) {
            x.anchor = {(views[0].pose.position.x + views[1].pose.position.x) * .5f,
                        (views[0].pose.position.y + views[1].pose.position.y) * .5f,
                        (views[0].pose.position.z + views[1].pose.position.z) * .5f};
            if (!x.heightCalibrated) {
                x.standingY = x.anchor.y;
                x.heightCalibrated = true;
            }
            x.anchor.y = x.standingY; // yaw/position recenter must not recalibrate a physical crouch
            x.yaw = horizontalHeadYaw(views[0].pose.orientation, 0) * 180 / 3.14159265f;
            x.hasAnchor = true;
        }
        x.tracking.headPosition = {(views[0].pose.position.x + views[1].pose.position.x) * .5f,
                                   (views[0].pose.position.y + views[1].pose.position.y) * .5f,
                                   (views[0].pose.position.z + views[1].pose.position.z) * .5f};
        x.tracking.headYawRadians = horizontalHeadYaw(views[0].pose.orientation, x.tracking.headYawRadians);
        x.tracking.anchor = x.anchor;
        x.tracking.anchorYaw = x.yaw;
        Game viewGame = game;
        viewGame.wallFade = 0;
        const auto &player = game.state->plr1;
        float bodyYaw=player.angpos*(2.f*3.14159265f/8192),anchorYaw=x.yaw*(3.14159265f/180);
        float bodyX = player.xoff / 65536.f, bodyZ = player.zoff / 65536.f;
        float bodyY = player.yoff - game.crouchCompensation * 8192;
        // Use both predicted eye positions, including IPD. Keep the real poses
        // unchanged; fade the scene before the near plane can cross a wall.
        for (int i=0;i<2;++i) {
            float dx = views[i].pose.position.x - x.anchor.x;
            float dz = views[i].pose.position.z - x.anchor.z;
            auto offset=engineVector({dx,0,dz},bodyYaw,anchorYaw);
            float ex = bodyX + offset.x*32;
            float ez = bodyZ + offset.z*32;
            float ey = bodyY - (views[i].pose.position.y-x.anchor.y)*8192;
            auto blocked = [&](float radius) {
                return view_path_blocked(&game.state->level,player.zone,
                                         bodyX,bodyY,bodyZ,ex,ey,ez,radius);
            };
            // Full blackout at 12.5cm, outside the 5cm near clipping plane.
            if (blocked(4)) viewGame.wallFade=1;
            else if (blocked(6)) viewGame.wallFade=std::max(viewGame.wallFade,.65f);
            else if (blocked(8)) viewGame.wallFade=std::max(viewGame.wallFade,.3f);
        }
        x.wallFade = viewGame.wallFade;
        float laserLength=0;
        auto muzzle=renderer.weapons.muzzle(player.gun_selected);
        if(x.tracking.valid[1] && (!game.overlay || !game.overlay->menu)){
            auto ray=weaponRay(x.tracking.hands[1],muzzle);
            float dx=ray.origin.x-x.anchor.x,dz=ray.origin.z-x.anchor.z;
            auto offset=engineVector({dx,0,dz},bodyYaw,anchorYaw);
            float ox=bodyX+offset.x*32,oz=bodyZ+offset.z*32;
            float oy=bodyY-(ray.origin.y-x.anchor.y)*8192;
            if(!view_path_blocked(&game.state->level,player.zone,bodyX,bodyY,bodyZ,ox,oy,oz,1)){
                int zone=level_find_zone_for_point(&game.state->level,int(ox),int(oz),player.zone);
                auto direction=engineVector(ray.direction,bodyYaw,anchorYaw);
                float vx=direction.x*32,vz=direction.z*32,vy=-direction.y*8192;
                auto blocked=[&](float distance){return view_path_blocked(&game.state->level,zone,ox,oy,oz,
                    ox+vx*distance,oy+vy*distance,oz+vz*distance,.15);};
                float low=0,high=1.25f;
                if(!blocked(high))low=high;
                else for(int step=0;step<12;++step){float middle=(low+high)*.5f;if(blocked(middle))high=middle;else low=middle;}
                laserLength=low;
            }
        }
        for (int i = 0; i < 2; i++) {
            auto &e = x.eyes[i];
            uint32_t index = 0;
            XrSwapchainImageAcquireInfo ai{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            XR_CHECK(xrAcquireSwapchainImage(e.chain, &ai, &index));
            XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            wait.timeout = XR_INFINITE_DURATION;
            XR_CHECK(xrWaitSwapchainImage(e.chain, &wait));
            glBindFramebuffer(GL_FRAMEBUFFER, x.fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, e.images[index].image,
                                   0);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, e.depth);
            auto framebufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (framebufferStatus != GL_FRAMEBUFFER_COMPLETE)
                throw std::runtime_error(
                    "OpenXR eye framebuffer is incomplete: status=" + std::to_string(framebufferStatus) +
                    " glError=" + std::to_string(glGetError()) +
                    " texture=" + std::to_string(e.images[index].image) +
                    " isTexture=" + std::to_string(glIsTexture(e.images[index].image)));
            glViewport(0, 0, e.width, e.height);
            glClearColor(.006f, .009f, .025f, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            auto f = views[i].fov;
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glFrustum(std::tan(f.angleLeft) * .05, std::tan(f.angleRight) * .05, std::tan(f.angleDown) * .05,
                      std::tan(f.angleUp) * .05, .05, 150);
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            inversePose(views[i].pose);
            glTranslatef(x.anchor.x, x.anchor.y, x.anchor.z);
            glRotatef(x.yaw, 0, 1, 0);
            renderer.world(game, true);
            glLoadIdentity();
            inversePose(views[i].pose);
            // Controller geometry uses its full runtime pose in LOCAL space.
            for (int hand = 0; hand < 2; ++hand)
                if (x.tracking.valid[hand]) {
                    glPushMatrix();
                    auto pose = x.tracking.hands[hand];
                    // A rigid inverse is transposed by OpenGL; use a quaternion matrix below.
                    auto q = pose.orientation;
                    GLfloat m[] = {1 - 2 * (q.y * q.y + q.z * q.z),
                                   2 * (q.x * q.y + q.w * q.z),
                                   2 * (q.x * q.z - q.w * q.y),
                                   0,
                                   2 * (q.x * q.y - q.w * q.z),
                                   1 - 2 * (q.x * q.x + q.z * q.z),
                                   2 * (q.y * q.z + q.w * q.x),
                                   0,
                                   2 * (q.x * q.z + q.w * q.y),
                                   2 * (q.y * q.z - q.w * q.x),
                                   1 - 2 * (q.x * q.x + q.y * q.y),
                                   0,
                                   0,
                                   0,
                                   0,
                                   1};
                    glTranslatef(pose.position.x, pose.position.y, pose.position.z);
                    glMultMatrixf(m);
                    if(hand==1) {
                        renderer.weapons.draw(game.state->plr1.gun_selected,game.state->plr1.gun_frame);
                        if(laserLength>.01f){
                            glPushAttrib(GL_ALL_ATTRIB_BITS);
                            glDisable(GL_TEXTURE_2D);glDisable(GL_LIGHTING);glDisable(GL_ALPHA_TEST);
                            glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
                            glDepthMask(GL_FALSE);glColor4f(1,.18f,.035f,.8f);glLineWidth(2);
                            glBegin(GL_LINES);
                            glVertex3f(muzzle[0],muzzle[1],muzzle[2]);
                            glVertex3f(muzzle[0],muzzle[1],muzzle[2]-laserLength);
                            glEnd();glPopAttrib();++renderer.laserDraws;
                        }
                    } else {
                    glDisable(GL_TEXTURE_2D);
                    glColor3f(hand ? .9f : .2f, hand ? .5f : .8f, .3f);
                    glBegin(GL_LINES);
                    glVertex3f(0, 0, 0);
                    glVertex3f(0, 0, -.25f);
                    glEnd();
                    }
                    glPopMatrix();
                }
            if (viewGame.wallFade>0) {
                glPushAttrib(GL_ALL_ATTRIB_BITS);
                glDisable(GL_DEPTH_TEST);
                glDisable(GL_ALPHA_TEST);
                glDisable(GL_TEXTURE_2D);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
                glMatrixMode(GL_PROJECTION);
                glPushMatrix(); glLoadIdentity();
                glMatrixMode(GL_MODELVIEW);
                glLoadIdentity();
                glColor4f(0,0,0,viewGame.wallFade);
                glBegin(GL_QUADS);
                glVertex2f(-1,-1); glVertex2f(1,-1); glVertex2f(1,1); glVertex2f(-1,1);
                glEnd();
                glMatrixMode(GL_PROJECTION); glPopMatrix();
                glMatrixMode(GL_MODELVIEW);
                glPopAttrib();
            }
            // Head-locked HUD/menu: retain the real eye-to-head offset so the
            // panel has stable stereo depth rather than duplicated screen text.
            glLoadIdentity();
            inversePose(views[i].pose);
            auto head = views[0].pose;
            head.position = x.tracking.headPosition;
            glTranslatef(head.position.x, head.position.y, head.position.z);
            auto q = head.orientation;
            GLfloat rotation[] = {1 - 2 * (q.y * q.y + q.z * q.z),
                                  2 * (q.x * q.y + q.w * q.z),
                                  2 * (q.x * q.z - q.w * q.y),
                                  0,
                                  2 * (q.x * q.y - q.w * q.z),
                                  1 - 2 * (q.x * q.x + q.z * q.z),
                                  2 * (q.y * q.z + q.w * q.x),
                                  0,
                                  2 * (q.x * q.z + q.w * q.y),
                                  2 * (q.y * q.z - q.w * q.x),
                                  1 - 2 * (q.x * q.x + q.y * q.y),
                                  0,
                                  0,
                                  0,
                                  0,
                                  1};
            glMultMatrixf(rotation);
            renderer.finishFrame(viewGame, i, 0, 0, e.width, e.height);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glFlush();
            XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            XR_CHECK(xrReleaseSwapchainImage(e.chain, &release));
            auto &pv = projectionViews[i];
            pv.pose = views[i].pose;
            pv.fov = views[i].fov;
            pv.subImage.swapchain = e.chain;
            pv.subImage.imageRect.extent = {e.width, e.height};
        }
    }
    XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    layer.space = x.space;
    layer.viewCount = 2;
    layer.views = projectionViews.data();
    const XrCompositionLayerBaseHeader *layers[] = {
        reinterpret_cast<const XrCompositionLayerBaseHeader *>(&layer)};
    XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};
    end.displayTime = fs.predictedDisplayTime;
    end.environmentBlendMode = x.blend;
    end.layerCount = render ? 1 : 0;
    end.layers = render ? layers : nullptr;
    XR_CHECK(xrEndFrame(x.session, &end));
    x.frames++;
    if (render)
        x.stereoFrames++;
    return true;
}
const Tracking &XR::tracking() const { return p->tracking; }
void XR::setVisible(bool visible) { p->visible = visible; }
void XR::recenter() { p->hasAnchor = false; }
bool XR::resetHeight() {
    if(!p->tracking.headValid || !std::isfinite(p->tracking.headPosition.y))return false;
    // Recalibrate vertical offset only. Keep position/yaw recenter independent.
    p->standingY=p->tracking.headPosition.y;
    p->anchor.y=p->standingY;p->tracking.anchor.y=p->standingY;p->heightCalibrated=true;
    return true;
}
bool XR::focused() const { return p->isFocused; }
float XR::wallFade() const { return p->wallFade; }
std::string XR::description() const { return p->runtime; }
} // namespace tempest
#else
namespace tempest {
struct XR::Impl {};
XR::XR() : p(std::make_unique<Impl>()) {}
XR::~XR() = default;
void XR::initialize(SDL_Window *) {
    throw std::runtime_error("Built without OpenXR. Rebuild with -DTEMPEST_OPENXR=ON.");
}
bool XR::frame(Renderer &, const Game &) { return false; }
void XR::setVisible(bool) {}
void XR::recenter() {}
bool XR::resetHeight() { return false; }
bool XR::focused() const { return false; }
float XR::wallFade() const { return 0; }
std::string XR::description() const { return "unavailable"; }
} // namespace tempest
#endif
