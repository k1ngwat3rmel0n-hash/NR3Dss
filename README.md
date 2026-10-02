# NR3DS v0.004

Old 3DS-oriented NIGHT-RUNNERS-style driving prototype.

## What changed in v0.004

- Six lightweight traffic cars with independent highway speeds.
- Relative-distance traffic motion so cars can be caught and passed.
- Speed-reactive camera FOV.
- Cheap headlight pool on the road.
- Brake-light brightness response.
- Keeps the v0.003 chase-camera behavior and Citro3D highway scene.
- Preserves the lightweight recovered/reimplemented driving core.

This is still a prototype. Traffic does not collide yet and the road is still straight.

## Build

GitHub Actions is the easiest path. The included workflow runs devkitARM inside the devkitPro container.

Local 3DS build:

```sh
cd 3ds
rm -rf build
make
```

Expected output:

```text
nr3ds_v004.3dsx
nr3ds_v004.smdh
```

## Controls

```text
Circle Pad  steer
A           throttle
B           brake
X           handbrake
L/R         shift down/up
START       exit
```

No original NIGHT-RUNNERS assets are included.
