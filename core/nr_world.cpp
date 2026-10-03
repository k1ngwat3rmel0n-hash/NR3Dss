#include "nr_world.hpp"

#include <algorithm>
#include <cmath>

namespace nr3ds {
namespace {
constexpr float kPi = 3.14159265358979323846f;

inline float clamp01(float v) {
    return std::max(0.0f, std::min(1.0f, v));
}

inline float smoothstep(float t) {
    t = clamp01(t);
    return t * t * (3.0f - 2.0f * t);
}
}

ExpresswayRoute::ExpresswayRoute() {
    // This table is the v0.011 renderer/streaming test route. The structure is
    // intentionally data-driven so extracted Unity road transforms can replace
    // these hand-authored values later without rewriting the renderer.
    sections_ = {{
        {   0.0f, 120.0f, RoadStyle::Open,        0.8f,  1.00f,  0.0f, 11.0f, 3},
        { 120.0f, 150.0f, RoadStyle::SodiumFence, 4.7f,  0.95f,  0.8f, 10.9f, 3},
        { 270.0f, 145.0f, RoadStyle::Elevated,    -5.6f, -1.10f,  2.2f, 10.8f, 3},
        { 415.0f, 140.0f, RoadStyle::DenseCity,    2.9f,  0.80f, -1.2f, 10.8f, 3},
        { 555.0f, 105.0f, RoadStyle::Underpass,   -1.2f, -0.45f, -0.9f, 10.7f, 3},
        { 660.0f, 165.0f, RoadStyle::Tunnel,       5.0f,  1.10f,  0.4f, 10.6f, 3},
        { 825.0f, 155.0f, RoadStyle::Junction,    -5.4f, -0.90f,  1.2f, 10.8f, 3},
        { 980.0f, 210.0f, RoadStyle::Open,         1.8f,  1.60f, -1.0f, 11.0f, 3},
        {1190.0f, 250.0f, RoadStyle::SodiumFence,  0.0f, -1.25f,  0.0f, 10.9f, 3},
    }};
}

float ExpresswayRoute::totalLengthM() const {
    const auto& last = sections_.back();
    return last.startM + last.lengthM;
}

float ExpresswayRoute::clampWorld(float worldM) const {
    return std::max(0.0f, std::min(worldM, totalLengthM() - 0.001f));
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

float ExpresswayRoute::accumulatedLateralBefore(std::size_t sectionIndex) const {
    float out = 0.0f;
    for (std::size_t i = 0; i < sectionIndex; ++i) out += sections_[i].lateralDeltaM;
    return out;
}

float ExpresswayRoute::accumulatedElevationBefore(std::size_t sectionIndex) const {
    float out = 0.0f;
    for (std::size_t i = 0; i < sectionIndex; ++i) out += sections_[i].elevationDeltaM;
    return out;
}

float ExpresswayRoute::centerAt(float worldM) const {
    const float w = clampWorld(worldM);
    for (std::size_t i = 0; i < sections_.size(); ++i) {
        const auto& s = sections_[i];
        if (w < s.startM + s.lengthM || i + 1 == sections_.size()) {
            const float t = clamp01((w - s.startM) / std::max(s.lengthM, 0.001f));
            const float base = accumulatedLateralBefore(i);
            const float transition = s.lateralDeltaM * smoothstep(t);
            const float localWave = s.waveAmplitudeM * std::sin(kPi * t);
            return base + transition + localWave;
        }
    }
    return 0.0f;
}

float ExpresswayRoute::elevationAt(float worldM) const {
    const float w = clampWorld(worldM);
    for (std::size_t i = 0; i < sections_.size(); ++i) {
        const auto& s = sections_[i];
        if (w < s.startM + s.lengthM || i + 1 == sections_.size()) {
            const float t = clamp01((w - s.startM) / std::max(s.lengthM, 0.001f));
            return accumulatedElevationBefore(i) + s.elevationDeltaM * smoothstep(t);
        }
    }
    return 0.0f;
}

float ExpresswayRoute::yawAt(float worldM) const {
    constexpr float d = 2.0f;
    const float a = centerAt(worldM - d);
    const float b = centerAt(worldM + d);
    return std::atan2(b - a, 2.0f * d);
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
        case RoadStyle::Elevated: return "ELEVATED";
        case RoadStyle::DenseCity: return "CITY";
        case RoadStyle::Underpass: return "UNDERPASS";
        case RoadStyle::Tunnel: return "TUNNEL";
        case RoadStyle::Junction: return "JUNCTION";
    }
    return "UNKNOWN";
}

} // namespace nr3ds
