#pragma once
#include <memory>
#include <string>
namespace tempest {
struct DomeLaunch {
    bool managed = false, xr = false;
    int port = 0, pid = 0;
};
// Run before SDL opens its Wayland connection. Explicit modes select the matching socket.
DomeLaunch domeLaunch(const std::string &mode);
class DomeLink {
    struct State;
    std::unique_ptr<State> p;

  public:
    explicit DomeLink(DomeLaunch launch);
    ~DomeLink();
    bool managed() const;
    bool active() const;
    bool suspended() const;
    bool focused();
    void ready();
    void begin();
    void suspend();
    void end(bool desktop = false);
};
} // namespace tempest
