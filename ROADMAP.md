# NR3DS roadmap

## Stable baseline through v0.014

- Native Citro3D renderer targeting Old 3DS
- Reconstructed NIGHT-RUNNERS-style vehicle physics core
- High-speed steering precision and wall-slide behavior
- Traffic and rival AI
- Garage, cash, upgrades and gearing
- Race loop
- Recovered Unity C1 waypoint network
- ~2.09 km source-derived route branch
- 80 m streaming/chunk window
- Stable absolute scenery segment IDs (no 10 m environment re-phase)
- Horizon fade for newly active geometry
- Source mesh metadata extraction
- First actual source LOD road/tunnel triangle streams rendered in Citro3D

## Next conversion milestone

- Parse and convert a larger subset of `sharedassets11.assets` and `sharedassets19.assets`.
- Reconstruct exact `level11` / `level19` scene hierarchy transforms for those MeshFilters.
- Place source road-line, road-edge, junction and tunnel modules at their true scene transforms.
- Add near/mid/far source LOD selection and stronger distance culling.
- Replace more cuboid proxy geometry with converted source meshes where Old-3DS performance allows.

## After that

- Expand the recovered C1 graph beyond the first branch.
- Restore junction-aware route choices and free-roam navigation.
- Convert PA/rest-area geometry and connect it to rival encounters.
- Improve player/rival car meshes and source-inspired UI/garage presentation.
- Add persistent saves and rival progression.
- Add engine/turbo/tire audio and tunnel reverb-style effects.
- Validate performance on physical Old 3DS hardware.
