# Reverse-engineering / fidelity notes - v0.012

The original `level1` Unity scene yielded 287 ordered route containers with 9,447 waypoint transforms. v0.012 is the first NR3DS build to consume those transforms directly.

The converted test branch is composed from:

1. `WP_AREA_2,1_HIGH_0` - 97 source waypoints, about 1.446 km.
2. `WP_AREA_2,1_R_0` - first connector points leading toward the tunnel branch.
3. `WP_AREA_2,1_TUNNEL_0` - 31 source waypoints, about 0.542 km.

The stitched chain is about 2.094 km and is resampled every 8 m into 263 points. Positions are translated to a local origin but otherwise preserve the recovered x/y/z path. At runtime the renderer builds a player-relative Frenet-like frame from the source centerline so bends can turn in both horizontal axes rather than being represented only as lateral displacement over a straight road.

The surrounding visual dressing is not yet a literal conversion of the source meshes. v0.012 uses NR3DS geometry for barriers, lamps, buildings and tunnel walls while preserving the recovered route path/elevation. `sharedassets1.assets` contains hundreds of named source meshes and is the next input for selective mesh conversion/decimation.

The portable handling core remains the same approximation/reimplementation used by earlier builds. Exact Unity AnimationCurve keyframes are still not imported.
