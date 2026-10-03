# NR3DS v0.017.1 — Texture Pipeline Hotfix

Hotfix for v0.017 freezing on the last garage frame after pressing Y.

## Cause

The new source-texture renderer switches Citro3D from the normal fixed-color vertex
pipeline to a textured UV pipeline for the asphalt/tunnel pass. The restore function
contained an accidental recursive call to itself. Entering the expressway reached that
function on the first road frame, recursively exhausted the stack, and therefore left
the previously rendered garage frame on screen.

## Fix

- Removed the recursive `setupColorPipeline()` call.
- Explicitly restores the primary-color TexEnv combiner after the textured pass.
- Keeps all v0.017 source textures, v0.016.1 curve fixes, source car, long C1 route,
  traffic, race, garage and audio test.

Expected artifact: `nr3ds_v0171.3dsx`.
