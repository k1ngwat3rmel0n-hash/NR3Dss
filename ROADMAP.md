# NR3DS roadmap

## Stable through v0.016.1

- native devkitARM/libctru/Citro3D project
- recovered C1 route expanded to ~6.97 km
- continuous post-race world travel
- stable curved barriers / 8 m visual segmentation
- Livisa '89 source body
- complete `sharedassets0–86` catalog
- multi-family source mesh catalog
- NDSP audio test path

## v0.017

- first decoded NIGHT-RUNNERS Texture2D payloads from `.resS`
- 128x128 source asphalt and tunnel concrete
- ETC1/T3X build-time conversion
- UV-capable Citro3D shader/pipeline
- source asphalt road overlay
- source tunnel wall/ceiling texture overlay

## Next

1. Preserve UV channels on converted source road/tunnel meshes and reconstruct exact MeshFilter transforms.
2. Add scaled source textures for barriers, road markings/signage and selected buildings.
3. Give the Livisa proper source material/texture treatment and split bumper/hood/spoiler/wheels into customization slots.
4. Extend the C1 graph with junction choices/free-roam.
5. Offline-transcode selected authorized FSB5 music to a 3DS-friendly streaming format.
6. Profile texture/triangle budgets on physical Old 3DS.
