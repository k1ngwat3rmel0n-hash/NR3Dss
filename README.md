# NR3DS v0.016 — Source Atlas C1

v0.016 turns the four supplied 7-Zip archives into a complete NIGHT-RUNNERS static-asset catalog and expands the 3DS renderer from one repeated source-road test mesh to a multi-scene source mesh atlas.

## Full sharedassets catalog

All `sharedassets0.assets` through `sharedassets86.assets` were successfully recovered from the four archives. The catalog currently indexes:

- 87 sharedassets files
- 53,372 serialized Unity objects
- 5,210 Mesh objects
- 337 Material objects
- 1,892 Texture2D objects

The machine-readable catalog is under `tools/full_asset_catalog/`. This gives later builds a direct lookup table for source mesh names/path IDs instead of rediscovering assets one file at a time.

## Seven source mesh families in the 3DS renderer

v0.014/v0.015 used one AREA_2 road test mesh and one tunnel roof. v0.016 adds a normalized source atlas built directly from developer-authorized Unity mesh data:

- high/elevated road — `sharedassets13 / _HIGH_LANE_LOD.001`
- low/underpass road — `sharedassets16 / _LOW_MESH_LOD_1.003`
- junction road — `sharedassets11 / .DUAL LANE JUCTION LOD1.013`
- tunnel roof — `sharedassets19 / _TUNNEL_OG_2x2_LANE_HIGH_ROOF.132`
- actual road-line geometry — `sharedassets16 / _LOW_ROADLINES.003`
- compact steel support — `sharedassets2 / SUPPORTS STEEL WHITE NEW.002`
- AREA_0,8/open road — `sharedassets25 / ._2_LANE_OG.028`

The meshes are converted offline into road-local coordinates and uploaded to dedicated Citro3D VBOs. The renderer selects different source geometry according to the recovered road style instead of stamping the same mesh across the entire route.

The procedural road deck remains underneath as a safety/fallback surface while source alignment and Old-3DS performance are validated. Source textures/materials are catalogued but are **not yet rendered as textures in v0.016**; the source meshes still use the fixed-color Citro3D material path.

## Kept from v0.015

- ~6.97 km recovered C1 centerline
- 6.4 km sprint race
- continuous world movement after a race result
- stable scenery segment IDs/no 10 m re-phasing
- source Sannis Livisa '89 body
- traffic, rival, collisions and tuning
- 3DS NDSP audio test loop
- 30-track source music manifest

## Controls

Garage:
- D-pad Up/Down: select
- A: buy upgrade
- D-pad Left/Right: gear/final-drive tune
- Circle Pad Left/Right: rotate car display
- Y: enter expressway
- START: exit

Expressway:
- Circle Pad: steer
- A: throttle
- B: brake
- X: handbrake
- L/R: shift down/up
- SELECT: retry race
- Y after finish: garage
- START: exit

## Expected build

`nr3ds_v016.3dsx`

## Test focus

Watch for source-road pieces that are obviously rotated, too tall, too wide, or expensive to render. The atlas intentionally keeps the procedural deck underneath, so a bad source module should affect appearance rather than make the route undrivable.

Desktop core/race/garage/world tests pass, and `3ds/source/main.cpp` passes a local syntax-only C++17 compile with libctru/Citro3D API stubs. GitHub Actions remains the authoritative 3DS toolchain build.
