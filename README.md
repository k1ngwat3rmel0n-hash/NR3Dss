# NR3DS v0.011

Midnight-expressway renderer and world-streaming foundation for the Old 3DS NIGHT-RUNNERS-style demake/reimplementation.

## What is new

- Data-driven expressway route format in `core/nr_world.*`.
- 80 m world chunks with a small active window around the player.
- Distinct visual highway zones instead of one endlessly repeating road:
  - open expressway
  - orange/sodium fence corridor
  - elevated roadway
  - dense city section
  - underpass
  - bright yellow/cream tunnel
  - junction/exit section
- Lower, closer chase-camera framing.
- Stronger speed sensation from close roadside geometry and cheap high-speed streaks.
- Narrower roadway proportions and brighter lane markings.
- Section-specific fake lighting and sky/clear colors.
- Basic elevation changes in the road renderer.
- Traffic, rival, checkpoints and race gates now follow the route center/elevation.
- Bottom-screen debug line shows the current zone and active chunk range.
- Keeps the v0.010 garage, tuning, economy, handling, precision steering and wall-slide behavior.

## Important map note

v0.011 is the **streaming/rendering foundation**, not the promised 1:1 reconstructed NIGHT-RUNNERS map yet. The route table in `nr_world.cpp` is a temporary original test route built from the visual references the user supplied.

To reconstruct the original road topology accurately, the next reverse-engineering step needs the raw Unity scene data from the PC build, starting with:

- `NIGHT-RUNNERS PROLOGUE PATREON_Data/globalgamemanagers`
- the highway scene `levelXX` file(s)
- matching `.resS` / `sharedassetsXX.assets` files if referenced by those scenes

The current conversation/library contains the file-tree listing and code/metadata artifacts, but not the raw `levelXX` scene bytes needed to recover GameObject transforms.

## Controls

### Garage
- D-pad Up/Down: select item
- A: buy upgrade
- D-pad Left/Right: tune final drive/gears
- Circle Pad Left/Right: rotate display car
- Y: start expressway race
- START: exit

### Expressway
- Circle Pad: steer
- A: throttle
- B: brake
- X: handbrake
- L/R: shift down/up
- SELECT: retry
- Y after finish: return to garage
- START: exit

## Build

Use the included GitHub Actions workflow or run `make` from `3ds/` with devkitPro `3ds-dev` installed.

Expected output:

```text
nr3ds_v011.3dsx
```

## Verification

The desktop physics/race/garage/world test suite passes. The 3DS `main.cpp` also passes a local C++ syntax check against Citro3D/libctru-compatible stubs; the real 3DS build should still be validated through the existing devkitPro GitHub Actions workflow.
