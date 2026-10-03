# Reverse-engineering / conversion notes - v0.015

v0.015 uses developer-authorized source material supplied by the user.

## Route

The route header is generated from ordered waypoint chains extracted from `level1 / C1_TATSUMI.unity`.
The connected route is 6972.709 m and resampled at 8 m intervals (873 samples). Several short gaps
between separately serialized route chains are linearly bridged; these bridges are documented in
`tools/map_reconstruction/v015_route_sections.json`.

## Livisa '89

The source customization UnityFS bundle was unpacked and its Unity 2018 Mesh objects parsed. The
v0.015 road/showroom shell combines representative stock meshes (doors, stock bumpers/fenders/hood,
headlights, hatch/rear pieces, side skirts and exhaust). Mesh-local coordinates are already authored
in a common vehicle space, so the conversion preserves source proportions before welding to an
Old-3DS-oriented LOD.

Road LOD in v0.015:
- 3176 welded vertices
- 6368 triangles
- 19104 expanded Citro3D draw vertices
- approximately 1.72 m wide, 4.05 m long, 0.99 m body-panel height

The current fixed-color shader does not yet preserve source materials/UV textures. Glass, wheels and
lights are separate procedural overlays pending the material/customization pass.

## Music

The supplied music UnityFS bundle contains 30 streamed Unity AudioClip objects. Resource payloads
were located exactly and identified as FSB5, codec/compression value 1 at the Unity AudioClip level.
The 3DS build does not ship those source tracks yet. v0.015 validates NDSP playback using an original
runtime-generated PCM loop; offline FSB5 decoding/transcoding is the next audio step.
