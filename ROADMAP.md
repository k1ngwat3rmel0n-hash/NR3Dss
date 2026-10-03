# NR3DS roadmap

## Stable baseline through v0.013

- Native Citro3D renderer targeting Old 3DS
- Reconstructed NIGHT-RUNNERS-style vehicle physics core
- High-speed steering precision and wall-slide behavior
- Traffic and rival AI
- Garage, cash, upgrades and gearing
- Race loop
- Recovered Unity C1 waypoint network
- ~2.09 km source-derived route branch
- 80 m streaming/chunk window
- Source mesh metadata extraction
- Source-inspired road, fence, support and tunnel proxy geometry

## Next conversion milestone

- Obtain the additive scene/assets for `AREA_2,1` and `AREA_TUNNEL_2,1`.
- Parse their MeshFilter -> Mesh associations and exact scene transforms.
- Convert selected road/tunnel meshes to an NR3DS vertex/index format.
- Decimate and quantize geometry for PICA200.
- Generate near/mid/far LOD variants.
- Replace proxy modules with section-specific converted meshes where performance permits.

## After that

- Expand the reconstructed C1 graph beyond the first branch.
- Restore junction-aware route choices.
- Convert PA/rest-area geometry and connect it to free-roam rival encounters.
- Add persistent saves and rival progression.
- Add audio, tunnel reverb-style effects and stronger analog/VHS presentation.
- Validate performance on physical Old 3DS hardware.
