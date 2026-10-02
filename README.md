# NR3DS v0.006

Old 3DS-oriented NIGHT-RUNNERS-style driving prototype.

This build keeps the v0.005 curved highway, traffic AI and collision systems and adds the requested **high-speed steering precision fix**.

## What changed

- speed-dependent steering sensitivity
- larger steering deadzone only at high speed
- progressive input curve around the Circle Pad center
- steering-rate smoothing so tiny thumb movements do not snap the car across a lane
- maximum steering angle reduced further at highway speed
- drift/handbrake mode relaxes the filter so countersteering still works
- bottom-screen filtered steering telemetry for tuning

The intended behavior is responsive below ~50 km/h, progressively calmer above ~80 km/h, and significantly easier to place within a lane above 160 km/h.

## Controls

- Circle Pad: steer
- A: throttle
- B: brake
- X: handbrake
- L/R: shift down/up
- SELECT: reset
- START: exit

## Build

```sh
cd 3ds
rm -rf build
make
```

Expected output:

```text
nr3ds_v006.3dsx
```

## Fidelity note

The physics core contains behavior reconstructed from the supplied NIGHT-RUNNERS IL2CPP build, while chassis, road, traffic, collisions and this steering filter are lightweight 3DS-specific implementations. Original Unity serialized AnimationCurve keyframes have not yet been extracted.

No original NIGHT-RUNNERS art/audio assets are included.
