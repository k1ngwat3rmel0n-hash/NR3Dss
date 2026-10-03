# NR3DS v0.007

Old 3DS driving prototype focused on high-speed steering precision and cleaner barrier contact behavior.

## Changes from v0.006

- Keeps the high-speed steering curve, deadzone, and rate smoothing from v0.006.
- Barrier contacts now slide along the wall instead of repeatedly bouncing/spinning the car.
- Wall speed loss is much less aggressive, especially during shallow contact.
- Traffic impacts are slightly softer so collisions remain recoverable.
- Bottom-screen steering telemetry now shows raw Circle Pad input and filtered steering side-by-side.
- Curved highway, traffic AI, collisions, turbo/heat/tire systems, and chase camera remain enabled.

## Controls

- Circle Pad: steer
- A: throttle
- B: brake
- X: handbrake
- L/R: shift
- SELECT: reset
- START: exit

## 3DS build

Use the existing GitHub Actions workflow or run `make` inside `3ds/` with devkitPro `3ds-dev`. Expected output: `nr3ds_v007.3dsx`.
