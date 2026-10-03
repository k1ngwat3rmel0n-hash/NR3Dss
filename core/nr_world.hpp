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
    HighLevel,
};

struct RoadSection {
    float startM = 0.0f;
    float lengthM = 100.0f;
    RoadStyle style = RoadStyle::Open;
    float roadWidthM = 10.8f;
    int lanes = 3;
    const char* sourceName = "";
};

struct RouteLocalFrame {
    float lateralM = 0.0f;
    float forwardM = 0.0f;
    float elevationM = 0.0f;
    float yawRad = 0.0f;
};

class ExpresswayRoute {
public:
    static constexpr float kChunkLengthM = 80.0f;
    static constexpr std::size_t kSectionCount = 13;

    ExpresswayRoute();

    float totalLengthM() const;
    const RoadSection& sectionAt(float worldM) const;
    RoadStyle styleAt(float worldM) const;

    // Absolute recovered route values, measured in the converted Unity route frame.
    float centerAt(float worldM) const;
    float elevationAt(float worldM) const;
    float yawAt(float worldM) const;

    // Converts a point farther along the recovered route into the player's local
    // road frame. This preserves real two-dimensional bends instead of reducing
    // the source path to a hand-authored sine wave.
    RouteLocalFrame localFrame(float playerWorldM, float aheadM) const;

    int chunkIndex(float worldM) const;
    int activeChunkFirst(float worldM) const;
    int activeChunkLast(float worldM) const;
    bool chunkActive(int chunk, float worldM) const;

    const std::array<RoadSection, kSectionCount>& sections() const { return sections_; }

    static const char* styleName(RoadStyle style);

private:
    std::array<RoadSection, kSectionCount> sections_{};

    float clampWorld(float worldM) const;
    void pointAt(float worldM, float& x, float& y, float& z) const;
};

} // namespace nr3ds
