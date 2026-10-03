# Reverse-engineering / fidelity notes - v0.014

v0.014 uses the recovered C1 route centerline/elevation and begins drawing actual source LOD vertex geometry supplied with permission by the original developer.

## Route-specific asset findings

The supplied additive scenes and asset files showed that the giant `sharedassets2.assets` file is not required for the first Old-3DS pass. The route-specific additive scenes reference lower-detail meshes in their own shared asset files:

- `level11` (`C1_AREA_2,1`) references many LOD1/road-line meshes in `sharedassets11.assets`.
- `level19` (`C1_AREA_TUNNEL_2,1`) references many tunnel/road LOD meshes in `sharedassets19.assets`.

This is advantageous for the Old 3DS because those LOD assets are already substantially lighter than the PC LOD0 meshes.

## Actual mesh data embedded in v0.014

Two source meshes are converted into position-only expanded triangle streams in `3ds/source/source_meshes.hpp`:

- `AREA_2 double single template.013`
  - source: `sharedassets11.assets`, Mesh path ID 2
  - source vertices: 640
  - source indices: 960 / 320 triangles
  - converted draw stream: 960 vertices
- `_TUNNEL_OG_2x2_LANE_HIGH_ROOF.520`
  - source: `sharedassets11.assets`, Mesh path ID 31
  - source vertices: 180
  - source indices: 270 / 90 triangles
  - converted draw stream: 270 vertices

The converter maps Unity mesh coordinates into a compact road-local frame and discards normals/UV/material/shader state for this initial pass. This lets Citro3D draw the real silhouette while keeping the runtime format tiny.

## Stable-stream fix

The v0.013 renderer computed `baseM = floor(progress/10)*10` and then used the local loop index for all modulo/cadence decisions. Crossing each 10 m boundary reset the loop index, causing visible simultaneous relocation of procedural detail.

v0.014 instead computes a stable absolute `segId` for every module and uses that ID for all cadence decisions. New far modules are additionally faded in near the 300 m horizon.

## Remaining fidelity work

The source LOD meshes in v0.014 are road-local modules, not yet exact scene-transform placements. Next work is to combine:

1. `level11` / `level19` MeshFilter and Transform hierarchy,
2. `sharedassets11.assets` / `sharedassets19.assets` vertex/index buffers,
3. recovered master-world transforms,

so individual original road/tunnel pieces can be placed at their true source transforms, then culled/LOD-switched in the NR3DS streamer.
