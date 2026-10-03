#include "nr_source_geometry.hpp"

namespace nr3ds {
namespace {
// The ratios below were measured from local AABBs in the supplied Unity mesh
// objects, then normalized to practical Old-3DS world dimensions.
constexpr SourceGeometryProfile kRoad {
    36.0f, 0.34f, 12.2f,
    "AREA1_BIG_ROAD_2 / AREA_TATSUMI_R ROAD_LOD0"
};
constexpr SourceGeometryProfile kFence {
    42.0f, 2.20f, 0.16f,
    "_TATSUMI_MESH FENCE0_LOD0"
};
constexpr SourceGeometryProfile kSupport {
    30.0f, 4.80f, 1.10f,
    "AREA_2_SUPPORTS2.001"
};
constexpr SourceGeometryProfile kBuilding {
    30.0f, 10.0f, 11.0f,
    "AREA_TATSUMI_BUILDING LOD0"
};
constexpr SourceGeometryProfile kVending {
    1.10f, 2.00f, 0.85f,
    "AREA_TATSUMI_VENDING_LOD0"
};
}

const SourceGeometryProfile& sourceGeometryProfile(SourceProxyKind kind) {
    switch (kind) {
        case SourceProxyKind::Road: return kRoad;
        case SourceProxyKind::Fence: return kFence;
        case SourceProxyKind::Support: return kSupport;
        case SourceProxyKind::TatsumiBuilding: return kBuilding;
        case SourceProxyKind::Vending: return kVending;
    }
    return kRoad;
}

} // namespace nr3ds
