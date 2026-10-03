#include "nr_world.hpp"
#include "nr_route_data.hpp"

#include <algorithm>
#include <cmath>

namespace nr3ds {
namespace {
constexpr float kPi = 3.14159265358979323846f;

inline float clampf(float v, float lo, float hi) {
    return std::max(lo, std::min(v, hi));
}

inline float wrapPi(float a) {
    while (a > kPi) a -= 2.0f * kPi;
    while (a < -kPi) a += 2.0f * kPi;
    return a;
}
}

ExpresswayRoute::ExpresswayRoute() {
    // v0.015 extends the recovered C1 route from ~2.09 km to ~6.97 km.
    // Every centerline section below comes from ordered waypoint chains recovered
    // from level1/C1_TATSUMI.unity. Short gaps between additive scene routes are
    // linearly bridged offline; no hand-authored sine-wave road remains.
    sections_ = {{
        {0.000f,    1445.858f, RoadStyle::HighLevel, 10.75f, 3, "AREA_2,1_HIGH"},
        {1445.858f,   99.485f, RoadStyle::Junction,  10.55f, 3, "AREA_2,1_R"},
        {1545.343f,  548.542f, RoadStyle::Tunnel,    10.45f, 3, "AREA_2,1_TUNNEL"},
        {2093.885f,  341.900f, RoadStyle::Underpass, 10.45f, 3, "AREA_2,1_MAIN_L"},
        {2435.785f,  404.206f, RoadStyle::Junction,  10.55f, 3, "RIGHT_MAIN_1A"},
        {2839.991f,   74.977f, RoadStyle::Junction,  10.55f, 3, "RIGHT_MAIN_1B"},
        {2914.968f,  699.204f, RoadStyle::Underpass, 10.45f, 3, "LEFT_MAIN_0"},
        {3614.172f,  410.088f, RoadStyle::Junction,  10.55f, 3, "RIGHT_MAIN_0"},
        {4024.260f,  588.411f, RoadStyle::HighLevel, 10.75f, 3, "RIGHT_HIGH_JOIN"},
        {4612.671f,  164.551f, RoadStyle::HighLevel, 10.75f, 3, "RIGHT_HIGH_MAIN"},
        {4777.222f,  809.337f, RoadStyle::HighLevel, 10.75f, 3, "AREA_0,8_HIGH"},
        {5586.559f, 1059.119f, RoadStyle::HighLevel, 10.75f, 3, "AREA_0_UPPER_R"},
        {6645.678f,  327.031f, RoadStyle::Open,      10.75f, 3, "AREA_0,1_MAIN_R"},
    }};
}

float ExpresswayRoute::totalLengthM() const {
    return kRecoveredRouteLengthM;
}

float ExpresswayRoute::clampWorld(float worldM) const {
    return clampf(worldM, 0.0f, totalLengthM());
}

void ExpresswayRoute::pointAt(float worldM, float& x, float& y, float& z) const {
    const float w = clampWorld(worldM);
    const float regularEndM = float(kRecoveredRouteSampleCount - 2) * kRecoveredRouteStepM;
    if (w >= regularEndM) {
        const auto& a = kRecoveredRouteSamples[kRecoveredRouteSampleCount - 2];
        const auto& b = kRecoveredRouteSamples[kRecoveredRouteSampleCount - 1];
        const float tail = std::max(kRecoveredRouteLengthM - regularEndM, 0.001f);
        const float t = clampf((w - regularEndM) / tail, 0.0f, 1.0f);
        x = a.x + (b.x - a.x) * t;
        y = a.y + (b.y - a.y) * t;
        z = a.z + (b.z - a.z) * t;
        return;
    }

    const float f = w / kRecoveredRouteStepM;
    const std::size_t i = static_cast<std::size_t>(f);
    const float t = f - static_cast<float>(i);
    const auto& a = kRecoveredRouteSamples[i];
    const auto& b = kRecoveredRouteSamples[i + 1];
    x = a.x + (b.x - a.x) * t;
    y = a.y + (b.y - a.y) * t;
    z = a.z + (b.z - a.z) * t;
}

const RoadSection& ExpresswayRoute::sectionAt(float worldM) const {
    const float w = clampWorld(worldM);
    for (const auto& s : sections_) {
        if (w >= s.startM && w < s.startM + s.lengthM) return s;
    }
    return sections_.back();
}

RoadStyle ExpresswayRoute::styleAt(float worldM) const {
    return sectionAt(worldM).style;
}

float ExpresswayRoute::centerAt(float worldM) const {
    float x, y, z;
    pointAt(worldM, x, y, z);
    (void)y; (void)z;
    return x;
}

float ExpresswayRoute::elevationAt(float worldM) const {
    float x, y, z;
    pointAt(worldM, x, y, z);
    (void)x; (void)z;
    return y;
}

float ExpresswayRoute::yawAt(float worldM) const {
    constexpr float d = 2.0f;
    float ax, ay, az, bx, by, bz;
    pointAt(clampWorld(worldM - d), ax, ay, az);
    pointAt(clampWorld(worldM + d), bx, by, bz);
    (void)ay; (void)by;
    return std::atan2(bx - ax, bz - az);
}

RouteLocalFrame ExpresswayRoute::localFrame(float playerWorldM, float aheadM) const {
    const float pM = clampWorld(playerWorldM);
    const float qM = clampWorld(playerWorldM + aheadM);

    float px, py, pz, qx, qy, qz;
    pointAt(pM, px, py, pz);
    pointAt(qM, qx, qy, qz);

    constexpr float d = 2.0f;
    float ax, ay, az, bx, by, bz;
    pointAt(clampWorld(pM - d), ax, ay, az);
    pointAt(clampWorld(pM + d), bx, by, bz);
    (void)ay; (void)by;

    float fx = bx - ax;
    float fz = bz - az;
    const float mag = std::sqrt(fx * fx + fz * fz);
    if (mag > 0.0001f) {
        fx /= mag;
        fz /= mag;
    } else {
        fx = 0.0f;
        fz = 1.0f;
    }

    // right = +90 degrees from forward in the x/z plane.
    const float rx = fz;
    const float rz = -fx;
    const float dx = qx - px;
    const float dz = qz - pz;

    RouteLocalFrame out;
    out.lateralM = dx * rx + dz * rz;
    out.forwardM = dx * fx + dz * fz;
    out.elevationM = qy - py;
    out.yawRad = wrapPi(yawAt(qM) - yawAt(pM));
    return out;
}

int ExpresswayRoute::chunkIndex(float worldM) const {
    const float w = std::max(0.0f, worldM);
    return static_cast<int>(std::floor(w / kChunkLengthM));
}

int ExpresswayRoute::activeChunkFirst(float worldM) const {
    return std::max(0, chunkIndex(worldM) - 1);
}

int ExpresswayRoute::activeChunkLast(float worldM) const {
    const int routeLast = chunkIndex(totalLengthM() - 0.001f);
    return std::min(routeLast, chunkIndex(worldM) + 4);
}

bool ExpresswayRoute::chunkActive(int chunk, float worldM) const {
    return chunk >= activeChunkFirst(worldM) && chunk <= activeChunkLast(worldM);
}

const char* ExpresswayRoute::styleName(RoadStyle style) {
    switch (style) {
        case RoadStyle::Open: return "OPEN";
        case RoadStyle::SodiumFence: return "ORANGE FENCE";
        case RoadStyle::Elevated: return "OVERHEAD";
        case RoadStyle::DenseCity: return "CITY";
        case RoadStyle::Underpass: return "UNDERPASS";
        case RoadStyle::Tunnel: return "TUNNEL";
        case RoadStyle::Junction: return "JUNCTION";
        case RoadStyle::HighLevel: return "RECOVERED HIGH";
    }
    return "UNKNOWN";
}

} // namespace nr3ds
