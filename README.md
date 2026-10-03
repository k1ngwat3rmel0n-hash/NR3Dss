# NR3DS v0.015

Long recovered-C1 route, first real source-car pass, and first 3DS audio-output milestone.

## What is new

### ~6.97 km recovered C1 route

The source-derived route has been expanded from about 2.09 km to about 6.97 km using
ordered waypoint chains recovered from `level1 / C1_TATSUMI.unity`.

The current connected chain is:

- `WP_AREA_2,1_HIGH_0`
- partial `WP_AREA_2,1_R_0`
- `WP_AREA_2,1_TUNNEL_0`
- `WP_AREA_2,1_1_MAIN_L_1`
- `WP_RIGHT_MAIN_1.0`
- `WP_RIGHT_MAIN_1.1`
- the connected `WP_LEFT_MAIN_0` branch (reversed)
- the connected `WP_RIGHT_MAIN_0` branch
- `WP_RIGHT_HIGHER_MAIN_0_JOIN`
- `WP_RIGHT_HIGHER_MAIN_0.1`
- `WP_AREA_0,8_HIGH_0 (1)`
- `WP_AREA_0_UPPER_R_0`
- `WP_AREA_0,1_MAIN_R_4`

Short gaps between additive-scene waypoint chains are bridged linearly offline. The road
centerline itself no longer uses hand-authored sine curves.

The sprint race is now 6.4 km with checkpoints at 1.6 / 3.2 / 4.8 km.

### First real source player car

The cuboid body has been replaced by a road-LOD conversion made from the developer-authorized
Sannis Livisa '89 customization bundle. The first pass combines representative stock body
panels into a welded ~6,368-triangle shell (~19,104 expanded draw vertices).

The same source body is used in the showroom and on the expressway. Wheels, glass and light
accents remain intentionally cheap procedural pieces for now so they can later become separate
customization slots.

### Audio pipeline online

The supplied music UnityFS bundle was parsed into 30 streamed `AudioClip` entries. Exact names,
durations, source sample rates and FSB5 offsets/sizes are stored in:

`tools/music_import/source_music_manifest.json`

The source songs are FSB5/Vorbis and are **not embedded in this build yet**. v0.015 instead
runs a tiny original four-second PCM synth loop through the 3DS NDSP API to validate real
hardware/emulator audio output without blocking the map/car work on FSB5 transcoding.

### Kept from v0.014.1

- continuous world movement after a race result
- stable absolute segment IDs (no 10 m scenery re-phasing)
- source road/tunnel LOD test geometry
- high-speed steering precision
- wall sliding and traffic collisions
- traffic/rival race systems
- garage/upgrades/gearing

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

`nr3ds_v015.3dsx`

## Performance note

Azahar processing percentages are useful for spotting regressions but are not a substitute for
performance testing on a physical Old 3DS. The Livisa shell is deliberately a first LOD pass;
further mesh/material work will be budgeted against real-hardware measurements.
