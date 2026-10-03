# NR3DS v0.009

Garage, tuning and race-reward milestone for the Old 3DS NIGHT-RUNNERS-style demake/reimplementation.

## What is new

- Boots into a simple native 3D garage scene.
- Session cash, wins and losses.
- Race rewards: $1000 for a win, $300 for a loss.
- Engine upgrades (3 levels).
- Turbo upgrades (3 levels).
- Tire/grip upgrades (3 levels).
- Free final-drive tuning.
- Free individual 1st-6th gear ratio tuning.
- Upgrades actually alter the portable vehicle configuration used by the race.
- `Y` starts a race from the garage and returns to the garage after a result.
- Keeps the v0.008 race, rival, traffic, high-speed steering and wall-slide behavior.

## Garage controls

- D-pad Up/Down: select item
- A: buy selected Engine/Turbo/Tires upgrade
- D-pad Left/Right: adjust final drive or selected gear ratio
- Y: start race
- START: exit

## Race controls

- Circle Pad: steer
- A: throttle
- B: brake
- X: handbrake
- L/R: shift down/up
- SELECT: retry race
- Y after finish: return to garage
- START: exit

## Economy

The prototype starts with `$1100`, enough to make an initial build choice. Upgrades get progressively more expensive, while repeated races fund further tuning.

This milestone deliberately keeps progression in memory only. Persistent save data is planned for the next progression milestone.

## Build

Use the included GitHub Actions workflow, or run `make` from `3ds/` with devkitPro `3ds-dev` installed.

Expected output:

```text
nr3ds_v009.3dsx
```

## Fidelity note

The upgrade categories and loop are modeled after the kinds of engine/turbo/tire/gear systems found in the NIGHT-RUNNERS build we inspected, but this menu/economy is a lightweight native implementation for the demake. Exact original prices, progression balance and serialized Unity curves have not yet been imported.
