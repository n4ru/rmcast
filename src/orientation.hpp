#ifndef ORIENTATION_HPP
#define ORIENTATION_HPP

#include <string>

namespace vnsee
{

/**
 * Display orientation of the rendered VNC framebuffer relative to fb0.
 *
 * Portrait     : 1:1 copy. Source dims must match fb0 dims.
 * LandscapeCW  : source rotated 90° clockwise so a landscape source fits
 *                a portrait fb0. Source W×H lands in fb0 H×W.
 * InvertedPortrait : 180° rotation.
 * LandscapeCCW : source rotated 90° counter-clockwise.
 * Auto         : pick at create_framebuf time by comparing source dims
 *                to fb0 dims. Currently resolves to Portrait or LandscapeCW.
 */
enum class Orientation
{
    Portrait,
    LandscapeCW,
    InvertedPortrait,
    LandscapeCCW,
    Auto,
};

inline auto orientation_from_string(const std::string& s) -> Orientation
{
    if (s == "auto")                return Orientation::Auto;
    if (s == "portrait")            return Orientation::Portrait;
    if (s == "landscape")           return Orientation::LandscapeCW;
    if (s == "landscape-cw")        return Orientation::LandscapeCW;
    if (s == "landscape-ccw")       return Orientation::LandscapeCCW;
    if (s == "inverted-landscape")  return Orientation::LandscapeCCW;
    if (s == "inverted-portrait")   return Orientation::InvertedPortrait;
    return Orientation::Auto; // unknown → auto
}

inline auto orientation_to_string(Orientation o) -> const char*
{
    switch (o)
    {
        case Orientation::Portrait:         return "portrait";
        case Orientation::LandscapeCW:      return "landscape-cw";
        case Orientation::LandscapeCCW:     return "landscape-ccw";
        case Orientation::InvertedPortrait: return "inverted-portrait";
        case Orientation::Auto:             return "auto";
    }
    return "unknown";
}

} // namespace vnsee

#endif // ORIENTATION_HPP
