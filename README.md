# NR3DS v0.005

Old 3DS-oriented NIGHT-RUNNERS-style driving prototype.

This version moves the project from a straight traffic test toward an actual driving loop:

- Citro3D perspective highway renderer
- soft chase camera
- speed-sensitive FOV
- lightweight engine / drivetrain / tire model
- six moving traffic cars
- deterministic traffic lane-changing AI
- player/traffic collision response
- road-edge collision response
- simple curving highway presentation
- street lights, skyline blocks, gantries, headlights and brake lights
- SELECT resets the run

## Controls

- Circle Pad: steer
- A: throttle
- B: brake
- X: handbrake
- L/R: shift down/up
- SELECT: reset
- START: exit

## Build

The GitHub Actions workflow builds with the devkitPro devkitARM container. Locally, with devkitPro `3ds-dev` installed:

```sh
cd 3ds
rm -rf build
make
```

Expected output:

```text
nr3ds_v005.3dsx
```

## Fidelity note

The physics core contains behavior reconstructed from the supplied NIGHT-RUNNERS IL2CPP build, but the original Unity serialized AnimationCurve keyframes have not yet been extracted. Current torque/turbo/tire curves remain temporary approximations. The chassis and collision model are lightweight 3DS-specific substitutes rather than PhysX.

No original NIGHT-RUNNERS art/audio assets are included.
