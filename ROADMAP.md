# NR3DS roadmap

## Stable through v0.016

- native devkitARM/libctru/Citro3D project
- NIGHT-RUNNERS-derived vehicle handling baseline
- stable high-speed steering and wall slide
- traffic and rival sprint race
- tuning garage
- recovered C1 waypoint route expanded to ~6.97 km
- continuous post-race world travel
- developer-authorized Livisa '89 source body
- complete `sharedassets0–86` asset catalog
- seven-family source road/tunnel/support/roadline atlas
- 3DS NDSP audio test path and 30-track music manifest

## Next

1. Retain UVs in converted source meshes and decode selected road/tunnel Texture2D payloads from `.resS`.
2. Build a small PICA200-friendly texture atlas and replace fixed-color source meshes section by section.
3. Reconstruct MeshFilter + Transform placement from additive scenes so road furniture/buildings can use exact source positions instead of road-local repetition.
4. Extend the C1 graph with junction choices/free-roam rather than one linear sprint chain.
5. Split Livisa body into real bumper/hood/spoiler/wheel customization slots.
6. Offline-transcode selected authorized FSB5 music streams into a 3DS-friendly streaming format.
7. Profile on physical Old 3DS and tune triangle/texture budgets.
