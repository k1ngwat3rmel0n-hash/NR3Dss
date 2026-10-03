# NR3DS v0.013

Source-geometry proxy milestone for the Old 3DS NIGHT-RUNNERS-style demake/reimplementation.

## What is new

- Keeps the recovered ~2.09 km C1 branch introduced in v0.012.
- Adds a source-geometry profiling layer based on meshes extracted from the supplied `sharedassets1.assets` file.
- Replaces the plain high-level road edges with denser source-inspired fence modules.
- Adds repeated support/piers derived from the `AREA_2_SUPPORTS2` mesh family.
- Reworks the tunnel into a ribbed shell with wall modules, ceiling ribs, fluorescent strips and utility recesses.
- Adds a small distant Tatsumi-source proxy cluster using the proportions/style of the supplied Tatsumi building, vending and fence meshes.
- Keeps the v0.012 recovered centerline, elevation, traffic, rival, physics, steering, garage and upgrade systems intact.
- Adds reproducible source mesh profile data under `tools/source_geometry_extraction/`.

## What "source-derived" means in v0.013

The original PC meshes are **not** copied wholesale into the 3DS renderer yet. v0.013 reads/measures selected Unity mesh metadata and turns those measurements into aggressively simplified cuboid proxies suitable for an Old 3DS.

Selected source families include:

- `AREA_TATSUMI_R ROAD_LOD0.004`
- `AREA1_BIG_ROAD_2`
- `_TATSUMI_MESH FENCE0_LOD0` / FENCE1 / FENCE2
- `AREA_2_SUPPORTS2.001` / `.002`
- `AREA_TATSUMI_BUILDING LOD0`
- `AREA_TATSUMI_VENDING_LOD0`

The actual `AREA_2,1` and `AREA_TUNNEL_2,1` render meshes live in other additive Unity scene asset files. Those can be converted section-by-section once their matching `levelXX` / `sharedassetsXX.assets` files are available.

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
nr3ds_v013.3dsx
```

## Validation

The portable physics, race, garage, recovered-world and source-profile tests pass on the desktop harness. The 3DS frontend also passes the local C++ syntax check used for this project. GitHub Actions remains the final devkitARM build check.
