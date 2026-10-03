# Source geometry profiling - v0.013

This folder records the source-mesh measurements used by the first NR3DS source-geometry proxy pass.

The supplied `sharedassets1.assets` file is a Unity 2018.4 serialized asset file. v0.013 reads the local AABB metadata from selected meshes and uses their proportions/naming as references for extremely cheap Old-3DS proxy geometry.

Selected source mesh families include:

- `AREA_TATSUMI_R ROAD_LOD0.004`
- `AREA1_BIG_ROAD_2`
- `_TATSUMI_MESH FENCE0_LOD0` / FENCE1 / FENCE2
- `AREA_2_SUPPORTS2.001` / `.002`
- `AREA_TATSUMI_BUILDING LOD0`
- `AREA_TATSUMI_VENDING_LOD0`
- `AREA_1_MAIN_COLLIDER`

The raw PC meshes are not embedded in the v0.013 executable. The game uses lightweight generated geometry so the scene remains suitable for Old 3DS.

The exact `AREA_2,1` and `AREA_TUNNEL_2,1` render meshes live in additive Unity scenes/assets that have not yet been supplied. Once those scene asset files are available, the same pipeline can replace the generic proxy shell with section-specific converted geometry.
