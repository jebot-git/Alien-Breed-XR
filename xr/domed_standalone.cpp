#include "domed.hpp"
// Standalone releases never inspect DomeD sockets or contact its control API.
namespace tempest {
struct DomeLink::State {};
DomeLaunch domeLaunch(const std::string &) { return {}; }
DomeLink::DomeLink(DomeLaunch) : p(std::make_unique<State>()) {}
DomeLink::~DomeLink() = default;
bool DomeLink::managed() const { return false; }
bool DomeLink::active() const { return true; }
bool DomeLink::suspended() const { return false; }
bool DomeLink::focused() { return true; }
void DomeLink::ready() {}
void DomeLink::begin() {}
void DomeLink::suspend() {}
void DomeLink::end(bool) {}
}
