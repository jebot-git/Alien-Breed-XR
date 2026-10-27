#include "domed.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <curl/curl.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <nlohmann/json.hpp>
#include <random>
#include <sstream>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>
namespace tempest {
using Json = nlohmann::json;
namespace fs = std::filesystem;
namespace {
int peer(const fs::path &path) {
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
        throw std::runtime_error("Cannot open DomeD socket");
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    auto name = path.string();
    if (name.size() >= sizeof(address.sun_path)) {
        close(fd);
        throw std::runtime_error("DomeD socket path too long");
    }
    std::copy(name.begin(), name.end(), address.sun_path);
    ucred cred{};
    socklen_t length = sizeof(cred);
    bool ok = connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0 &&
              getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &length) == 0 && cred.uid == getuid();
    close(fd);
    if (!ok)
        throw std::runtime_error("DomeD socket ownership check failed");
    auto exe = fs::read_symlink("/proc/" + std::to_string(cred.pid) + "/exe").filename().string();
    if (exe != "dome-compositor" && exe != "dome-compositor (deleted)")
        throw std::runtime_error("Wayland peer is not DomeD");
    return cred.pid;
}
Json record(const fs::path &file) {
    struct stat st{};
    if (lstat(file.c_str(), &st) || !S_ISREG(st.st_mode) || st.st_uid != getuid() || (st.st_mode & 0022) ||
        st.st_size > 4096)
        throw std::runtime_error("Invalid DomeD launch record");
    std::ifstream input(file);
    Json value;
    input >> value;
    return value;
}
size_t received(char *bytes, size_t size, size_t count, void *data) {
    auto &text = *static_cast<std::string *>(data);
    size_t n = size * count;
    if (text.size() + n > 8 * 1024 * 1024)
        return 0;
    text.append(bytes, n);
    return n;
}
Json http(int port, const std::string &path, const Json *body = nullptr) {
    CURL *curl = curl_easy_init();
    if (!curl)
        throw std::runtime_error("Cannot initialize DomeD control connection");
    std::string response, url = "http://127.0.0.1:" + std::to_string(port) + path,
                          payload = body ? body->dump() : "";
    curl_slist *headers = curl_slist_append(nullptr, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_PROXY, "");
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 250L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 150L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, received);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    if (body) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    }
    auto result = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if (result != CURLE_OK)
        throw std::runtime_error("DomeD control connection unavailable");
    if (status == 404)
        throw std::runtime_error("Restart DomeD to enable native XR handoff");
    Json value = Json::parse(response);
    if (status != 200)
        throw std::runtime_error(value.value("error", std::string("DomeD rejected native XR handoff")));
    return value;
}
} // namespace
DomeLaunch domeLaunch(const std::string &mode) {
    DomeLaunch launch;
    auto display = getenv("WAYLAND_DISPLAY"), runtime = getenv("XDG_RUNTIME_DIR");
    if (!display || !runtime)
        return launch;
    fs::path socket = display;
    if (!socket.is_absolute())
        socket = fs::path(runtime) / socket;
    if (!fs::exists(socket.string() + ".dome-session.json"))
        return launch;
    int pid = peer(socket);
    auto info = record(socket.string() + ".dome-session.json");
    if (info.value("version", 0) != 1 || info.value("compositor_pid", 0) != pid)
        throw std::runtime_error("Invalid DomeD launch context");
    std::string realm = info.value("realm", "");
    if (realm != "desktop" && realm != "xr")
        throw std::runtime_error("Unknown DomeD launch realm");
    std::string desired = mode == "auto" ? realm : mode;
    if (desired != realm) {
        bool found = false;
        unsigned count = 0;
        for (auto &entry : fs::directory_iterator(socket.parent_path())) {
            if (++count > 256)
                break;
            std::string name = entry.path().string(), suffix = ".dome-session.json";
            if (name.size() < suffix.size() ||
                name.compare(name.size() - suffix.size(), suffix.size(), suffix))
                continue;
            try {
                auto other = record(entry.path());
                fs::path candidate = name.substr(0, name.size() - suffix.size());
                if (other.value("compositor_pid", 0) == pid && other.value("realm", "") == desired &&
                    peer(candidate) == pid) {
                    socket = candidate;
                    info = other;
                    realm = desired;
                    found = true;
                    break;
                }
            } catch (const std::exception &) {
            }
        }
        if (!found)
            throw std::runtime_error("Requested DomeD launch realm is unavailable");
        setenv("WAYLAND_DISPLAY", socket.c_str(), 1);
        unsetenv("WAYLAND_SOCKET");
    }
    // A validated DomeD socket is authoritative over inherited X11 preferences.
    setenv("SDL_VIDEODRIVER", "wayland", 1);
    launch.managed = true;
    launch.xr = realm == "xr";
    launch.pid = pid;
    launch.port = info.value("control_port", 0);
    if (launch.port < 1 || launch.port > 65535)
        throw std::runtime_error("Invalid DomeD control port");
    return launch;
}
struct DomeLink::State {
    DomeLaunch launch;
    std::atomic_bool stop{false}, claimed{false}, active{false}, wantsSuspend{false}, capable{false};
    std::atomic<uint64_t> window{0};
    std::string token, error;
    std::mutex mutex, api;
    std::condition_variable wake;
    std::thread worker;
    explicit State(DomeLaunch l) : launch(l) {
        std::random_device random;
        std::ostringstream text;
        for (int i = 0; i < 4; i++)
            text << std::hex << std::setw(8) << std::setfill('0') << random();
        token = text.str();
    }
    Json request(const char *op, bool desktop = false) {
        return Json{
            {"op", op}, {"id", window.load()}, {"pid", getpid()}, {"token", token}, {"desktop", desktop}};
    }
    void update(const Json &value) {
        if (value.value("version", 0) != 1 || value.value("compositor_pid", 0) != launch.pid)
            throw std::runtime_error("Unsupported DomeD handoff version");
        active = value.value("active", false) && value.value("owner", uint64_t(0)) == window.load();
    }
};
DomeLink::DomeLink(DomeLaunch launch) : p(std::make_unique<State>(launch)) {
    if (!launch.managed)
        return;
    curl_global_init(CURL_GLOBAL_DEFAULT);
    p->worker = std::thread([this] {
        bool viewReady = false;
        while (!p->stop) {
            try {
                if (p->claimed) {
                    std::lock_guard<std::mutex> api(p->api);
                    if (p->claimed) {
                        auto body = p->request(p->wantsSuspend ? "suspend" : "heartbeat");
                        p->update(http(p->launch.port, "/api/native-xr", &body));
                    }
                } else {
                    {
                        auto state = http(p->launch.port, "/api/state");
                        uint64_t newest = 0;
                        for (auto &w : state.at("scene").at("windows"))
                            if (w.value("client_pid", 0) == getpid())
                                newest = std::max(newest, w.at("id").get<uint64_t>());
                        if (newest != p->window.load()) {
                            p->window = newest;
                            viewReady = false;
                        }
                    }
                    if (p->window && !viewReady) {
                        if (!p->launch.xr) {
                            Json view{
                                {"op", "send_to_realm"}, {"id", p->window.load()}, {"realm", "desktop"}};
                            http(p->launch.port, "/api/action", &view);
                        }
                        viewReady = true;
                    }
                    auto capability = http(p->launch.port, "/api/native-xr");
                    if (capability.value("version", 0) != 1 ||
                        capability.value("compositor_pid", 0) != p->launch.pid)
                        throw std::runtime_error("DomeD native XR handoff unavailable");
                    p->capable = true;
                }
                {
                    std::lock_guard<std::mutex> lock(p->mutex);
                    p->error.clear();
                }
            } catch (const std::exception &e) {
                p->active = false;
                if (!p->claimed)
                    p->capable = false;
                std::lock_guard<std::mutex> lock(p->mutex);
                p->error = e.what();
            }
            p->wake.notify_all();
            std::unique_lock<std::mutex> lock(p->mutex);
            p->wake.wait_for(lock, std::chrono::milliseconds(200), [this] { return p->stop.load(); });
        }
    });
}
DomeLink::~DomeLink() {
    end();
    p->stop = true;
    p->wake.notify_all();
    if (p->worker.joinable())
        p->worker.join();
}
bool DomeLink::managed() const { return p->launch.managed; }
bool DomeLink::active() const { return !managed() || (p->claimed && p->active && !p->wantsSuspend); }
bool DomeLink::suspended() const { return managed() && p->claimed && (!p->active || p->wantsSuspend); }
bool DomeLink::focused() {
    if (!managed())
        return true;
    std::lock_guard<std::mutex> api(p->api);
    try {
        auto state = http(p->launch.port, "/api/state").at("scene");
        if (state.value("locked", false) || state.at("focused") != p->window.load())
            return false;
        for (auto &w : state.at("windows"))
            if (w.at("id") == p->window.load())
                return !w.value("minimized", true);
    } catch (const std::exception &) {
    }
    return false;
}
void DomeLink::ready() {
    if (!managed())
        return;
    std::unique_lock<std::mutex> lock(p->mutex);
    p->wake.wait_for(lock, std::chrono::seconds(2), [this] {
        return (p->window.load() != 0 && p->capable.load()) ||
               p->error.find("Restart DomeD") != std::string::npos;
    });
    if (!p->window || !p->capable)
        throw std::runtime_error(p->error.empty() ? "DomeD did not register the game window" : p->error);
}
void DomeLink::begin() {
    if (!managed())
        return;
    ready();
    std::lock_guard<std::mutex> api(p->api);
    p->wantsSuspend = false;
    // SDL can remap its Wayland surface while creating the GL renderer.
    // Resolve the current surface at the ownership boundary, not the first map.
    auto scene = http(p->launch.port, "/api/state").at("scene");
    uint64_t newest = 0;
    for (auto &window : scene.at("windows"))
        if (window.value("client_pid", 0) == getpid())
            newest = std::max(newest, window.at("id").get<uint64_t>());
    if (!newest)
        throw std::runtime_error("DomeD game surface is unavailable");
    p->window = newest;
    auto body = p->request("begin");
    p->update(http(p->launch.port, "/api/native-xr", &body));
    p->claimed = true;
    if (!p->active)
        throw std::runtime_error("DomeD did not capture the game");
}
void DomeLink::suspend() {
    if (!managed() || !p->claimed)
        return;
    std::lock_guard<std::mutex> api(p->api);
    p->wantsSuspend = true;
    p->active = false;
    auto body = p->request("suspend");
    try {
        p->update(http(p->launch.port, "/api/native-xr", &body));
    } catch (const std::exception &) {
    }
}
void DomeLink::end(bool desktop) {
    if (!managed())
        return;
    std::lock_guard<std::mutex> api(p->api);
    if (!p->claimed.exchange(false))
        return;
    p->active = false;
    auto body = p->request("end", desktop);
    try {
        http(p->launch.port, "/api/native-xr", &body);
    } catch (const std::exception &) {
    }
}
} // namespace tempest
