#pragma once

#include <cstdint>

namespace nr3ds {

// Low-poly proxy dimensions derived from the supplied NIGHT-RUNNERS Unity
// source meshes in sharedassets1.assets. These are intentionally reduced to
// primitive-friendly proportions for Old 3DS rendering; they are not the raw
// PC meshes.
struct SourceGeometryProfile {
    float spanM;
    float heightM;
    float depthM;
    const char* sourceMesh;
};

enum class SourceProxyKind : std::uint8_t {
    Road = 0,
    Fence,
    Support,
    TatsumiBuilding,
    Vending,
};

const SourceGeometryProfile& sourceGeometryProfile(SourceProxyKind kind);

} // namespace nr3ds
