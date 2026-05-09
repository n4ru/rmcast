#include "orientation_sensor.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace vnsee
{

const char* device_orientation_to_string(DeviceOrientation o)
{
    switch (o)
    {
        case DeviceOrientation::PortraitUp:     return "portrait-up";
        case DeviceOrientation::PortraitDown:   return "portrait-down";
        case DeviceOrientation::LandscapeLeft:  return "landscape-left";
        case DeviceOrientation::LandscapeRight: return "landscape-right";
        case DeviceOrientation::Unknown:        return "unknown";
    }
    return "unknown";
}

namespace
{

int read_accel_axis(char axis)
{
    char path[96];
    std::snprintf(path, sizeof(path),
                  "/sys/bus/iio/devices/iio:device0/in_accel_%c_raw", axis);
    FILE* f = std::fopen(path, "r");
    if (!f) return 0;
    int v = 0;
    if (std::fscanf(f, "%d", &v) != 1) v = 0;
    std::fclose(f);
    return v;
}

// Hysteresis threshold: an axis must dominate by at least this much before we
// flip orientation. Prevents jitter when the device is near 45° on its side.
// Raw readings on the rMPP are roughly ±10000 = 1g, so 6000 is a strong tilt.
constexpr int kDominanceThreshold = 6000;

DeviceOrientation classify(int x, int y, int z)
{
    int ax = std::abs(x);
    int ay = std::abs(y);
    int az = std::abs(z);

    // Z dominant means flat on table — keep last orientation.
    if (az > ax && az > ay) return DeviceOrientation::Unknown;

    if (ay > ax)
    {
        if (ay < kDominanceThreshold) return DeviceOrientation::Unknown;
        return y > 0 ? DeviceOrientation::PortraitUp
                     : DeviceOrientation::PortraitDown;
    }
    else
    {
        if (ax < kDominanceThreshold) return DeviceOrientation::Unknown;
        return x > 0 ? DeviceOrientation::LandscapeLeft
                     : DeviceOrientation::LandscapeRight;
    }
}

Orientation parse_env(const char* var, Orientation fallback)
{
    if (const char* s = std::getenv(var))
    {
        return orientation_from_string(s);
    }
    return fallback;
}

} // namespace

Orientation map_device_to_blit(DeviceOrientation d)
{
    // Defaults: rotate 90° in either direction so a landscape source fills
    // a portrait fb0. The two landscape hold directions get opposite rotations
    // so the user's eye sees content "right side up" regardless of which way
    // they tilted into landscape.
    static const Orientation pu = parse_env("ORIENT_PORTRAIT_UP",     Orientation::LandscapeCW);
    static const Orientation pd = parse_env("ORIENT_PORTRAIT_DOWN",   Orientation::LandscapeCCW);
    static const Orientation ll = parse_env("ORIENT_LANDSCAPE_LEFT",  Orientation::LandscapeCW);
    static const Orientation lr = parse_env("ORIENT_LANDSCAPE_RIGHT", Orientation::LandscapeCCW);

    switch (d)
    {
        case DeviceOrientation::PortraitUp:     return pu;
        case DeviceOrientation::PortraitDown:   return pd;
        case DeviceOrientation::LandscapeLeft:  return ll;
        case DeviceOrientation::LandscapeRight: return lr;
        case DeviceOrientation::Unknown:        return Orientation::Auto;
    }
    return Orientation::Auto;
}

OrientationSensor::OrientationSensor(Callback on_change,
                                     std::chrono::milliseconds poll_interval)
    : on_change_(std::move(on_change))
    , poll_interval_(poll_interval)
    , thread_([this] { this->run(); })
{}

OrientationSensor::~OrientationSensor()
{
    stop_.store(true);
    if (thread_.joinable()) thread_.join();
}

void OrientationSensor::run()
{
    while (!stop_.load())
    {
        int x = read_accel_axis('x');
        int y = read_accel_axis('y');
        int z = read_accel_axis('z');
        DeviceOrientation now = classify(x, y, z);

        if (now != DeviceOrientation::Unknown)
        {
            DeviceOrientation prev = last_.exchange(now);
            if (now != prev && on_change_)
            {
                std::cerr << "Orientation: "
                          << device_orientation_to_string(prev) << " → "
                          << device_orientation_to_string(now)
                          << " (accel x=" << x << " y=" << y << " z=" << z
                          << ")\n";
                on_change_(now);
            }
        }

        std::this_thread::sleep_for(poll_interval_);
    }
}

} // namespace vnsee
