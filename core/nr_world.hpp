#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace nr3ds {

enum class RoadStyle : std::uint8_t {
    Open = 0,
    SodiumFence,
    Elevated,
    DenseCity,
    Underpass,
    Tunnel,
    Junction,
};

struct RoadSection {
    float startM = 0.0f;
    float lengthM = 100.0f;
    RoadStyle style = RoadStyle::Open;
    float lateralDeltaM = 0.0f;
    float waveAmplitudeM = 0.0f;
    float elevationDeltaM = 0.0f;
    float roadWidthM = 11.0f;
    int lanes = 3;
};

class ExpresswayRoute {
public:
    static constexpr float kChunkLengthM = 80.0f;
    static constexpr std::size_t kSectionCount = 9;

    ExpresswayRoute();

    float totalLengthM() const;
    const RoadSection& sectionAt(float worldM) const;
    RoadStyle styleAt(float worldM) const;

    float centerAt(float worldM) const;
    float elevationAt(float worldM) const;
    float yawAt(float worldM) const;

    int chunkIndex(float worldM) const;
    int activeChunkFirst(float worldM) const;
    int activeChunkLast(float worldM) const;
    bool chunkActive(int chunk, float worldM) const;

    const std::array<RoadSection, kSectionCount>& sections() const { return sections_; }

    static const char* styleName(RoadStyle style);

private:
    std::array<RoadSection, kSectionCount> sections_{};

    float clampWorld(float worldM) const;
    float accumulatedLateralBefore(std::size_t sectionIndex) const;
    float accumulatedElevationBefore(std::size_t sectionIndex) const;
};

} // namespace nr3ds
