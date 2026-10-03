# NR3DS v0.014

Stable-source-stream milestone for the Old 3DS NIGHT-RUNNERS-style demake/reimplementation.

## What is new

- Fixes the visible scenery "reset" / apparent car-lag artifact reported in v0.013.
- Roadside objects now use stable absolute segment IDs instead of re-phasing every time the player crosses a 10 m boundary.
- Lane dashes, lights, fences, supports, tunnel ribs, buildings and junction props therefore stay attached to the same world positions while the player moves through them.
- Adds a far-horizon fade so newly active geometry appears inside the fog instead of popping in as a block.
- Keeps a stable deep-night clear/background color while road-section lighting changes locally, avoiding full-screen flashes/pops at zone boundaries.
- Integrates the first actual developer-authorized NIGHT-RUNNERS LOD mesh data into the native Citro3D renderer:
  - `AREA_2 double single template.013` from `sharedassets11.assets` as a lightweight road-detail layer.
  - `_TUNNEL_OG_2x2_LANE_HIGH_ROOF.520` from `sharedassets11.assets` as a tunnel-roof detail layer.
- The source meshes are converted offline into compact position-only triangle arrays; Unity runtime/material data is not required on the 3DS.
- Keeps the recovered ~2.09 km C1 centerline/elevation, traffic, rival, steering, collision, garage and upgrade systems intact.

## Why v0.013 looked like it was lagging

v0.013 rebuilt its procedural scenery pattern from loop index zero whenever the player's 10 m road bucket changed. The route itself moved continuously, but repeated details such as lamps, lane dashes, fence modules and tunnel ribs changed phase on the same frame. At speed this looked like the environment snapped backward/forward around the fixed chase-camera car.

v0.014 gives every 10 m module a permanent absolute segment number, so decorative cadence is deterministic in world space.

## Source-geometry status

This is the first build to draw original LOD vertex geometry in Citro3D, but it is not yet a full direct rendering of each Unity scene mesh at its original world transform. The original LOD meshes are normalized into road-local modules and layered over the recovered route. Exact section-by-section source placement is the next conversion step.

## Controls

### Garage
- D-pad Up/Down: select item
- A: buy selected upgrade
- D-pad Left/Right: adjust gearing
- Circle Pad: rotate display car
- Y: start expressway race
- START: exit

### Race
- Circle Pad: steer
- A: throttle
- B: brake
- X: handbrake
- L/R: shift down/up
- SELECT: retry race
- Y after finish: garage
- START: exit

## Build

Use the included GitHub Actions workflow or run `make` in `3ds/` with devkitPro `3ds-dev` installed.

Expected output:

```text
nr3ds_v014.3dsx
```

## Validation

The portable desktop physics/race/garage/world/source-profile test suite passes. The generated source-mesh header also compiles independently with a standard C++17 compiler. GitHub Actions remains the final devkitARM/Citro3D compile check.
