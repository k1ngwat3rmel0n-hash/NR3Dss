# NR3DS v0.012 - recovered C1 route

This milestone replaces the hand-authored v0.011 road curve with the first highway branch reconstructed from the supplied NIGHT-RUNNERS Unity data.

## What is new

- First source-derived road centerline/elevation data in-game.
- Converted branch is built from the recovered waypoint chain:
  - `WP_AREA_2,1_HIGH_0`
  - `WP_AREA_2,1_R_0` (connector portion)
  - `WP_AREA_2,1_TUNNEL_0`
- About 2.09 km of recovered route geometry represented by 263 uniformly spaced samples.
- Full 2D bends are rendered in the player's moving road frame instead of reducing the map to a sine-wave road.
- Source elevation is preserved and scaled for the handheld presentation.
- 80 m chunk streaming remains active.
- Race extended to 2050 m so the test run traverses the recovered upper route, connector and tunnel branch.
- Bottom screen shows the current recovered source section.
- v0.010 garage, v0.011 visual treatment, high-speed steering, traffic, collisions and tuning remain intact.

## Important fidelity distinction

The **route path and elevation** in this build come from recovered Unity waypoint data. The surrounding buildings, barriers, lamps, tunnel shell and lighting are still lightweight NR3DS geometry. Original source meshes can be converted later now that asset use has been authorized, but this build deliberately validates the recovered map coordinate pipeline first.

## Controls

### Garage
- D-pad Up/Down: select item
- A: buy upgrade
- D-pad Left/Right: tune gearing
- Circle Pad Left/Right: rotate display car
- Y: start expressway run
- START: exit

### Highway
- Circle Pad: steer
- A: throttle
- B: brake
- X: handbrake
- L/R: shift down/up
- SELECT: retry
- Y after result: garage
- START: exit

## Build

Use the included GitHub Actions workflow or `make` inside `3ds/` with devkitPro `3ds-dev`.

Expected output:

```text
nr3ds_v012.3dsx
```
