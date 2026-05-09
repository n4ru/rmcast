#ifndef ORIENTATION_SENSOR_HPP
#define ORIENTATION_SENSOR_HPP

#include "orientation.hpp"
#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

namespace vnsee
{

/**
 * Physical hold direction of the rMPP, derived from the lis2dw12 accelerometer
 * via /sys/bus/iio/devices/iio:device0/in_accel_*_raw.
 *
 * - PortraitUp     : panel's natural top is up (USB at bottom).
 * - PortraitDown   : 180° flipped (USB at top).
 * - LandscapeLeft  : panel rotated 90° CW from natural — USB on the left.
 * - LandscapeRight : panel rotated 90° CCW from natural — USB on the right.
 */
enum class DeviceOrientation
{
    Unknown,
    PortraitUp,
    PortraitDown,
    LandscapeLeft,
    LandscapeRight,
};

const char* device_orientation_to_string(DeviceOrientation o);

/**
 * Map a physical device orientation to a vnsee::Orientation (rotation applied
 * to the source framebuffer). Reads optional env-var overrides:
 *   ORIENT_PORTRAIT_UP, ORIENT_PORTRAIT_DOWN,
 *   ORIENT_LANDSCAPE_LEFT, ORIENT_LANDSCAPE_RIGHT
 * each accepting the same strings as the --orientation CLI flag.
 *
 * Defaults are tuned for landscape source (e.g. 2160x1620) on portrait fb0
 * (1620x2160): both "landscape" hold directions resolve to a 90° rotation,
 * portrait hold resolves to whichever 90° fills fb0 (since identity won't fit).
 */
Orientation map_device_to_blit(DeviceOrientation d);

/**
 * Polls the accelerometer in a background thread and notifies on every
 * orientation transition (debounced by hysteresis). Stops on destruction.
 */
class OrientationSensor
{
public:
    using Callback = std::function<void(DeviceOrientation)>;

    explicit OrientationSensor(Callback on_change,
                               std::chrono::milliseconds poll_interval =
                                   std::chrono::milliseconds(250));
    ~OrientationSensor();

    OrientationSensor(const OrientationSensor&) = delete;
    OrientationSensor& operator=(const OrientationSensor&) = delete;

    DeviceOrientation last() const { return last_.load(); }

private:
    void run();

    Callback on_change_;
    std::chrono::milliseconds poll_interval_;
    std::atomic<DeviceOrientation> last_{DeviceOrientation::Unknown};
    std::atomic<bool> stop_{false};
    std::thread thread_;
};

} // namespace vnsee

#endif // ORIENTATION_SENSOR_HPP
