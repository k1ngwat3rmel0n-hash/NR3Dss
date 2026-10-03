# NR3DS v0.010

Garage/showroom visual-overhaul milestone for the Old 3DS NIGHT-RUNNERS-style demake/reimplementation.

## What is new

- Rebuilt the garage into a bright Japanese tuning-shop/showroom scene.
- Added ceiling panels, fluorescent lights, parts shelves, product boxes, banners, tire stacks, a vending-machine silhouette, tool chest and a branded back-wall color band.
- Replaced the very simple garage car with a more detailed low-poly coupe silhouette made entirely from native cuboids.
- Added headlights, bumpers, skirts, dark glass, wheel hubs, a plate and an upgrade-visible intercooler accent.
- The garage car now slowly rotates on the display mat; the Circle Pad can rotate the display faster in either direction.
- Redesigned the bottom-screen garage HUD so parts, transmission settings and controls are grouped more cleanly.
- Keeps the v0.009 economy, upgrades, gearing, race, rival, traffic, steering and wall-slide systems unchanged.

## Garage controls

- D-pad Up/Down: select item
- A: buy selected Engine/Turbo/Tires upgrade
- D-pad Left/Right: adjust final drive or selected gear ratio
- Circle Pad Left/Right: rotate showroom car
- Y: start expressway race
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

## Build

Use the included GitHub Actions workflow, or run `make` from `3ds/` with devkitPro `3ds-dev` installed.

Expected output:

```text
nr3ds_v010.3dsx
```

## Visual direction

The new garage is an original low-poly interpretation of the Japanese tuning-shop atmosphere shown in the user's references. It does not reuse NIGHT-RUNNERS textures, meshes, logos or other copyrighted assets. The goal is to establish the same kind of mood while remaining practical for Old 3DS hardware.

The next planned milestone is the highway presentation overhaul: tighter Tokyo-expressway proportions, tunnels, overhead signs, denser roadside detail and stronger speed sensation.
