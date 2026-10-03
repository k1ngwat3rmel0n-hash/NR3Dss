# NR3DS v0.008

First playable race-loop milestone for the Old 3DS NIGHT-RUNNERS-style demake/reimplementation.

## What is new

- 3-second race countdown.
- One visible rival car with lightweight catch-up behavior.
- 900 m sprint race.
- Three checkpoint gantries plus a finish gantry.
- Rival lane changes during the race.
- Win/loss state and race timer.
- Rival gap and speed telemetry on the bottom screen.
- Player/rival collision handling.
- SELECT instantly resets the race for another run.
- Keeps the v0.007 high-speed precision steering and wall-slide behavior unchanged.

## Controls

- Circle Pad: steer
- A: throttle
- B: brake
- X: handbrake
- L/R: shift down/up
- SELECT: retry/reset race
- START: exit

## Build

Use the included GitHub Actions workflow, or run `make` from `3ds/` with devkitPro `3ds-dev` installed.

Expected output:

```text
nr3ds_v008.3dsx
```

## Race HUD

The lower screen displays countdown/result, race time, player distance, checkpoint count, rival gap, rival speed, vehicle telemetry, steering telemetry, collision counters, and Citro3D CPU/GPU timing.

## Fidelity note

The driving core still uses the recovered NIGHT-RUNNERS-inspired engine/turbo/grip behavior from earlier milestones, while the chassis, rival AI, renderer, and race presentation are lightweight native implementations intended for Old 3DS hardware. Exact Unity-serialized handling curves have not yet been imported.
