# Reverse-engineering / fidelity notes - v0.013

v0.013 continues using the recovered C1 route centerline/elevation from the supplied Unity scene data and adds the first source-environment profiling pass.

## Source mesh measurements

Selected mesh metadata was read from `sharedassets1.assets` (Unity 2018.4). Local AABB measurements and source names are recorded under:

- `tools/source_geometry_extraction/source_mesh_profiles.csv`
- `tools/source_geometry_extraction/source_mesh_profiles.json`

The current proxy renderer intentionally uses very low-cost primitives instead of copying the original PC vertex buffers directly. This is a performance decision for Old 3DS, not a source-access limitation.

## Source-derived proxy families

- Road proportions: `AREA1_BIG_ROAD_2`, `AREA_TATSUMI_R ROAD_LOD0.004`
- Fence language: `_TATSUMI_MESH FENCE0_LOD0`, FENCE1, FENCE2
- Support language: `AREA_2_SUPPORTS2.001`, `.002`
- Tatsumi landmark proportions: `AREA_TATSUMI_BUILDING LOD0`, `AREA_TATSUMI_VENDING_LOD0`

## Current limitation

The recovered v0.012 route passes through `AREA_2,1` and `AREA_TUNNEL_2,1`. Their exact render geometry is stored in different additive Unity scene/shared-asset files than the supplied `sharedassets1.assets`. v0.013 therefore keeps the exact recovered road path while using source-informed proxies for the visible shell.

Likely next source scenes from the build-settings mapping are:

- `level11` -> `C1_AREA_2,1.unity`
- `level19` -> `C1_AREA_TUNNEL_2,1.unity`
- `level31` -> `C1_AREA_2,1_BUILDINGS.unity`
- `level61` -> `C1_AREA_2,1_COLLIDERS.unity`

Their matching `sharedassetsXX.assets` files would allow section-specific mesh conversion/decimation.
