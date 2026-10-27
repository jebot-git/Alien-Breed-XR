#pragma once
#include <memory>
#include <openxr/openxr.h>
#include <string>
struct SDL_Window;
namespace tempest {
class Renderer;
class Game;
struct Tracking {
    XrPosef hands[2]{};
    bool headValid = false, valid[2]{}, fire = false, use = false, menu = false;
    XrVector2f move{}, turn{};
    XrVector3f anchor{};
    float anchorYaw = 0;
};
class XR {
    struct Impl;
    std::unique_ptr<Impl> p;
    void initializeActions();
    void sampleActions(XrTime time);

  public:
    XR();
    ~XR();
    XR(const XR &) = delete;
    XR &operator=(const XR &) = delete;
    void initialize(SDL_Window *window);
    bool frame(Renderer &renderer, const Game &game);
    const Tracking &tracking() const;
    void recenter();
    void setVisible(bool visible);
    bool focused() const;
    std::string description() const;
};
} // namespace tempest
