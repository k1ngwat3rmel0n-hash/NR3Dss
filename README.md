# NR3DS v0.002

First real **Citro3D perspective highway prototype**. This replaces the v0.001 2D top-screen test while keeping the same lightweight NIGHT-RUNNERS-inspired vehicle core.

## New in v0.002

- perspective 3D top-screen renderer
- simple 3D player car
- 3D highway segments, barriers and lane markers
- roadside lights
- cheap distance-fog effect
- existing engine / turbo / tire / drift prototype remains connected
- bottom-screen telemetry retained for Old 3DS profiling

The car is intentionally made from simple boxes for this milestone. The goal is to verify that the Citro3D scene, shader and physics all run together before spending time on proper meshes.

## Build

```sh
cd 3ds
rm -rf build
make
```

Expected output: `nr3ds_v002.3dsx`.


## v0.003 additions

- Soft chase camera keeps the car in frame.
- Highway/world shifts laterally with the player.
- Primitive wheels and improved car silhouette.
- Cheap skyline buildings.
- Overhead gantries/signs.
- Retains Old 3DS-oriented simple geometry and fixed-function color shading.
