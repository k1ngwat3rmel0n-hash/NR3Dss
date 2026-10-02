# NR3DS v0.001

A first **workable Old 3DS-oriented driving prototype** built from a portable C++ physics core plus a small Citro2D front end.

This is **not** a port of Unity or the original Windows executable. It is a clean reimplementation of selected driving behavior inferred from the supplied NIGHT-RUNNERS IL2CPP build, with lightweight substitutes designed for Old 3DS-class hardware.

## What works now

- Portable C++17 vehicle core compiles on desktop.
- Automated smoke tests pass.
- 30-second deterministic simulation produces telemetry CSV.
- Engine RPM / gearbox / torque flow.
- Simple turbo spool behavior based on recovered IL2CPP logic.
- Engine heat, heat-soak and low-health power reduction.
- Tire temperature state.
- Rear-slip estimate and grip redistribution.
- Recovered progressive handbrake timer behavior.
- Lightweight drift/yaw model.
- Old 3DS homebrew front-end source using Citro2D.
- Circle Pad steering, A throttle, B brake, X handbrake, L/R shifting.
- Top-screen road/car prototype and bottom-screen telemetry/performance counters.

## Important fidelity note

Several **algorithms and constants** are directly based on behavior reconstructed from the supplied IL2CPP files, but the original Unity-serialized `AnimationCurve` keyframes have not yet been extracted. The current torque/turbo/tire curves are therefore temporary approximations. Those are explicitly marked in source.

The current chassis integration is also an original lightweight substitute for Unity WheelCollider/PhysX. It is intentionally cheap enough to target Old 3DS rather than attempting to emulate PhysX.

## Desktop build

From the project root:

```sh
cmake -S desktop -B build-desktop
cmake --build build-desktop --config Release
ctest --test-dir build-desktop --output-on-failure
```

Run the smoke simulation:

```sh
./build-desktop/nr3ds_sim nr3ds_sim.csv
```

On Windows with Visual Studio generators, the executable may be under `build-desktop/Release/`.

## Nintendo 3DS build

Install devkitPro with the `3ds-dev` package group. Open the devkitPro/MSYS2 shell, then:

```sh
cd 3ds
make
```

Expected output:

```text
nr3ds_v001.3dsx
nr3ds_v001.smdh
```

Copy the `.3dsx` to the SD card for Homebrew Launcher use, or load it in a 3DS emulator that supports homebrew.

### Controls

```text
Circle Pad  steering
A           throttle
B           brake
X           handbrake
L           shift down
R           shift up
START       exit
```

## Directory layout

```text
core/
  nr_physics.hpp
  nr_physics.cpp

desktop/
  CMakeLists.txt
  sim_main.cpp
  tests.cpp

3ds/
  Makefile
  source/main.cpp

RE_NOTES.md
ROADMAP.md
```

## Current target

The immediate target is an interactive `.3dsx` that can sustain 30 FPS on an **Old 3DS**, then replace the temporary curves with values extracted from the game's serialized Unity assets and move from the 2D prototype renderer to a small Citro3D highway renderer.

No original NIGHT-RUNNERS assets are included in this package.
