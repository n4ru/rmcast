#include "options.hpp"
#include "orientation.hpp"
#include "app/client.hpp"
#include "config.hpp"
#include "rmioc/device.hpp"
#include <algorithm>
#include <cstdarg>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept> // IWYU pragma: keep
// IWYU pragma: no_include <bits/exception.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{

// RAII guard that silences both rotation event sources xochitl listens on:
//   1. lis2dw12 IIO event subscriptions (devices 2..5: orientation/tap/wk),
//      via the per-device events enable files. xochitl subscribes via
//      anon_inode:iio:event.
//   2. Hall effect input device (folio open/landscape) via
//      /sys/class/input/event1/device/inhibited (kernel-level input device
//      inhibit, which blocks event generation at the source).
//
// Each prior value is saved on construction and restored on destruction.
// Signal handlers (SIGINT/TERM/HUP/QUIT) also call restore so a kill never
// leaves the system stuck "rotation locked".
class OrientationLockGuard
{
public:
    OrientationLockGuard()
    {
        // 1. Silence every IIO event-enable file we can find under the lis2dw12
        //    devices. We don't know exactly which one xochitl is subscribed to,
        //    so silence all of them.
        const char* iio_paths[] = {
            "/sys/bus/iio/devices/iio:device2/events/in_accel0_gesture_doubletap_en",
            "/sys/bus/iio/devices/iio:device3/events/in_accel0_gesture_singletap_en",
            "/sys/bus/iio/devices/iio:device4/events/in_accel0_thresh_rising_en",
            "/sys/bus/iio/devices/iio:device5/events/in_rot0_change_either_en",
        };
        for (const char* p : iio_paths)
        {
            int prior = read_int(p);
            // Try to write 0 unconditionally; some entries may not exist.
            if (write_int(p, 0))
            {
                saved_iio_.push_back({p, prior});
                log_to_debug_file("Lock: %s prior=%d → 0 (post=%d)\n",
                                  p, prior, read_int(p));
            }
        }

        // 2. Inhibit the Hall effect input device (folio-orientation switch).
        const char* hall = "/sys/class/input/event1/device/inhibited";
        int prior_hall = read_int(hall);
        if (prior_hall >= 0 && write_int(hall, 1))
        {
            saved_hall_ = prior_hall;
            hall_engaged_ = true;
            log_to_debug_file("Lock: %s prior=%d → 1 (post=%d)\n",
                              hall, prior_hall, read_int(hall));
        }

        if (!saved_iio_.empty() || hall_engaged_)
        {
            sing_ = this;
            install_signal_handlers();
            std::cerr << "Locked rotation: silenced "
                      << saved_iio_.size() << " IIO events + Hall="
                      << (hall_engaged_ ? "inhibited" : "skipped") << "\n";
        }
    }

    ~OrientationLockGuard() { restore(); }

    OrientationLockGuard(const OrientationLockGuard&) = delete;
    OrientationLockGuard& operator=(const OrientationLockGuard&) = delete;

    static int read_int(const char* path)
    {
        FILE* f = std::fopen(path, "r");
        if (!f) return -1;
        int v = -1;
        if (std::fscanf(f, "%d", &v) != 1) v = -1;
        std::fclose(f);
        return v;
    }

    static bool write_int(const char* path, int v)
    {
        FILE* f = std::fopen(path, "w");
        if (!f) return false;
        std::fprintf(f, "%d", v);
        bool ok = (std::fflush(f) == 0);
        std::fclose(f);
        return ok;
    }

    static void log_to_debug_file(const char* fmt, ...)
    {
        FILE* f = std::fopen("/tmp/vnsee-debug.log", "a");
        if (!f) return;
        va_list ap; va_start(ap, fmt);
        std::vfprintf(f, fmt, ap);
        va_end(ap);
        std::fclose(f);
    }

private:
    void restore()
    {
        for (const auto& [path, prior] : saved_iio_)
        {
            if (prior < 0) continue;
            write_int(path.c_str(), prior);
        }
        saved_iio_.clear();

        if (hall_engaged_)
        {
            write_int("/sys/class/input/event1/device/inhibited", saved_hall_);
            hall_engaged_ = false;
            log_to_debug_file("Lock: restored Hall inhibit to %d\n", saved_hall_);
        }
        if (sing_ == this) sing_ = nullptr;
    }

    static void signal_handler(int sig)
    {
        if (sing_) sing_->restore();
        std::signal(sig, SIG_DFL);
        std::raise(sig);
    }

    void install_signal_handlers()
    {
        for (int sig : {SIGINT, SIGTERM, SIGHUP, SIGQUIT})
        {
            std::signal(sig, &OrientationLockGuard::signal_handler);
        }
    }

    std::vector<std::pair<std::string, int>> saved_iio_;
    int saved_hall_ = 0;
    bool hall_engaged_ = false;
    static OrientationLockGuard* sing_;
};

OrientationLockGuard* OrientationLockGuard::sing_ = nullptr;

/**
 * Read /sys/bus/iio/devices/iio:device0/in_accel_*_raw once and decide whether
 * the device is currently in portrait or landscape orientation. Returns true
 * for landscape (|x| dominant), false for portrait (|y| dominant or device
 * lying flat — flat defaults to portrait so the user gets the rMPP's natural
 * setup if they boot vnsee from a desk).
 */
bool initial_device_is_landscape()
{
    auto read = [](char axis) -> int {
        char path[96];
        std::snprintf(path, sizeof(path),
            "/sys/bus/iio/devices/iio:device0/in_accel_%c_raw", axis);
        FILE* f = std::fopen(path, "r");
        if (!f) return 0;
        int v = 0;
        if (std::fscanf(f, "%d", &v) != 1) v = 0;
        std::fclose(f);
        return v;
    };
    int x = read('x'), y = read('y'), z = read('z');
    int ax = std::abs(x), ay = std::abs(y), az = std::abs(z);
    std::cerr << "Initial accel: x=" << x << " y=" << y << " z=" << z << "\n";
    // Z-dominant means flat → default to portrait.
    if (az > ax && az > ay) return false;
    return ax > ay;
}

} // namespace

/**
 * Print a short help message with usage information.
 *
 * @param name Name of the current executable file.
 */
auto help(const char* name) -> void
{
    std::cout << "Usage: " << name << " [IP [PORT] [PASSWORD]] [OPTION...]\n"
"Connect to the VNC server at IP:PORT with PASSWORD.\n\n"
"Only when launching " PROJECT_NAME " from a SSH session is the IP optional,\n"
"in which case the client’s IP address is taken by default.\n"
"By default, PORT is 5900.\n"
"By default, PASSWORD is blank\n\n"
"Available options:\n"
"  -h, --help           Show this help message and exit.\n"
"  -v, --version        Show the current version of " PROJECT_NAME " and exit.\n"
"  --no-buttons         Disable buttons interaction.\n"
"  --no-pen             Disable pen interaction.\n"
"  --no-touch           Disable touchscreen interaction.\n"
"  --orientation MODE   Display rotation. MODE: auto (default), portrait,\n"
"                       landscape (= landscape-cw), landscape-ccw,\n"
"                       inverted-landscape, inverted-portrait.\n"
"  --auto-rotate        Track the rMPP accelerometer and re-rotate fb0 on\n"
"                       device rotation. Overrides --orientation while\n"
"                       active. Per-direction overrides via env vars:\n"
"                         ORIENT_PORTRAIT_UP, ORIENT_PORTRAIT_DOWN,\n"
"                         ORIENT_LANDSCAPE_LEFT, ORIENT_LANDSCAPE_RIGHT.\n"
"  --no-lock-rotation   Don't touch the lis2dw12_orientation IIO event source.\n"
"                       Default: vnsee writes 0 on startup and restores the\n"
"                       prior value on exit, so xochitl keeps AppLoad's\n"
"                       window in a fixed orientation while vnsee runs.\n";
}

/**
 * Print current version.
 */
auto version() -> void
{
    std::cout << PROJECT_NAME << ' ' << PROJECT_VERSION << '\n';
}

constexpr int default_server_port = 5900;
constexpr int min_port = 1;
constexpr int max_port = (1U << 16U) - 1;

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto main(int argc, const char* argv[]) -> int
{
    // Read options from the command line
    std::string server_ip;
    std::string password;
    int server_port = default_server_port;
    rmioc::device_request request(rmioc::device_request::screen);

    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const char* const name = argv[0];
    auto [opts, oper] = options::parse(argv + 1, argv + argc);

    if ((opts.count("help") >= 1) || (opts.count("h") >= 1))
    {
        help(name);
        return EXIT_SUCCESS;
    }

    if ((opts.count("version") >= 1) || (opts.count("v") >= 1))
    {
        version();
        return EXIT_SUCCESS;
    }

    if (oper.size() > 3)
    {
        std::cerr << "Too many operands: at most 3 are needed, you gave "
            << oper.size() << ".\n"
            "Run “" << name << " --help” for more information.\n";
        return EXIT_FAILURE;
    }

    if (!oper.empty())
    {
        server_ip = oper[0];
    }
    else
    {
        // Guess the server IP from the SSH client IP
        const char* ssh_conn = getenv("SSH_CONNECTION");

        if (ssh_conn == nullptr)
        {
            std::cerr << "No server IP given and no active SSH session.\n"
                "Please specify the VNC server IP.\n"
                "Run “" << name << " --help” for more information.\n";
            return EXIT_FAILURE;
        }

        // Extract the remote client IP from the first field
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        const char* ssh_conn_end = ssh_conn + std::strlen(ssh_conn);
        const char* remote_ip_end = std::find(ssh_conn, ssh_conn_end, ' ');

        // Remove IPv4-mapped IPv6 prefix
        const char* remote_ip_start = ssh_conn;
        const auto *ipv4_prefix = "::ffff:";

        if (std::strncmp(ipv4_prefix, ssh_conn, std::strlen(ipv4_prefix)) == 0)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            remote_ip_start += std::strlen(ipv4_prefix);
        }

        server_ip = std::string(remote_ip_start, remote_ip_end);
    }

    if (oper.size() >= 2)
    {
        try
        {
            server_port = std::stoi(oper[1]);
        }
        catch (const std::invalid_argument&)
        {
            std::cerr << "“" << oper[1]
                << "” is not a valid port number.\n";
            return EXIT_FAILURE;
        }

        if (server_port < min_port || server_port > max_port)
        {
            std::cerr << server_port << " is not a valid port "
                "number. Valid values range from " << min_port << " to "
                << max_port << ".\n";
            return EXIT_FAILURE;
        }
    }

    if (oper.size() == 3)
    {
        password = oper[2];
    }

    if (opts.count("no-buttons") >= 1)
    {
        opts.erase("no-buttons");
    }
    else
    {
        request.set_buttons(true);
    }

    if (opts.count("no-pen") >= 1)
    {
        opts.erase("no-pen");
    }
    else
    {
        request.set_pen(true);
    }

    if (opts.count("no-touch") >= 1)
    {
        opts.erase("no-touch");
    }
    else
    {
        request.set_touch(true);
    }

    vnsee::Orientation orientation = vnsee::Orientation::Auto;
    if (opts.count("orientation") >= 1)
    {
        const auto& vals = opts["orientation"];
        if (vals.empty())
        {
            std::cerr << "--orientation requires a value "
                "(auto|portrait|landscape|landscape-ccw|"
                "inverted-landscape|inverted-portrait).\n";
            return EXIT_FAILURE;
        }
        orientation = vnsee::orientation_from_string(vals.front());
        opts.erase("orientation");
    }

    bool auto_rotate = (opts.count("auto-rotate") >= 1);
    if (auto_rotate) opts.erase("auto-rotate");

    bool lock_rotation = !(opts.count("no-lock-rotation") >= 1);
    opts.erase("no-lock-rotation");

    if (!opts.empty())
    {
        std::cerr << "Unknown options: ";

        for (
            auto opt_it = std::cbegin(opts);
            opt_it != std::cend(opts);
            ++opt_it
        )
        {
            std::cerr << opt_it->first;

            if (std::next(opt_it) != std::cend(opts))
            {
                std::cerr << ", ";
            }
        }

        std::cerr << "\n";
        return EXIT_FAILURE;
    }

    // Start the client
    try
    {
        // RAII: lock xochitl's auto-rotation while vnsee runs, then restore.
        std::unique_ptr<OrientationLockGuard> rot_lock;
        if (lock_rotation) rot_lock = std::make_unique<OrientationLockGuard>();

        // Detect the device's current physical orientation and pick a qtfb
        // buffer shape that matches AppLoad's window. With landscape, ask qtfb
        // for a 2160x1620 buffer (matches a folio-keyboard-forced landscape
        // window) and let the source land identity. With portrait, default to
        // panel-native 1620x2160 + a 90° rotation in vnsee.
        // User-supplied --orientation always wins.
        bool start_landscape = initial_device_is_landscape();
        std::cerr << "Initial physical orientation: "
                  << (start_landscape ? "landscape" : "portrait") << "\n";
        // Skip the AppLoad-style "resize qtfb to landscape + identity copy"
        // shortcut when running against the vncast backend — its shm is
        // fixed at panel-native 1620×2160, so an identity copy of a
        // landscape source clips the right 540 columns. Let screen.cpp's
        // Auto resolve to LandscapeCCW which actually rotates into the
        // panel-portrait shm shape.
        const bool using_vncast = std::getenv("VNCAST_QTFB_SOCKET") != nullptr;
        if (orientation == vnsee::Orientation::Auto && !using_vncast)
        {
            if (start_landscape)
            {
                if (!std::getenv("QTFB_RESOLUTION"))
                    setenv("QTFB_RESOLUTION", "2160x1620", 0);
                orientation = vnsee::Orientation::Portrait; // identity copy
            }
        }

        rmioc::device device = rmioc::device::detect(request);

        std::cerr << "Connecting to "
            << server_ip << ":" << server_port << "\n";

        app::client client{server_ip.data(), server_port, password.data(), device, orientation, auto_rotate};

        std::cerr << "Connection established\n";

        if (!client.event_loop())
        {
            std::cerr << "Connection closed by the server.\n";
            return EXIT_FAILURE;
        }
    }
    catch (const std::exception& err)
    {
        std::cerr << "Error: " << err.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
