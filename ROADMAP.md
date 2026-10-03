# NR3DS roadmap

## Stable baseline through v0.012

- Native Citro3D Old-3DS renderer
- High-speed precision steering and wall sliding
- Traffic / rival / race loop
- Garage, economy and tuning
- Chunked expressway renderer
- Recovered Unity scene/route parsing
- First ~2.09 km source-derived C1 route branch running in-game

## Next: v0.013

- Convert selected original road/tunnel/barrier meshes from `sharedassets1.assets` into a lightweight NR3DS mesh format.
- Add triangle decimation and vertex-color/baked-light conversion.
- Attach converted meshes to recovered world transforms.
- Expand route graph beyond the first AREA_2,1 branch.
- Start junction-aware route selection rather than one fixed branch.

## Following milestones

- Tatsumi/parking-area reconstruction and rival encounter flow.
- Full C1 route graph streaming.
- Original signs/props/textures selectively downsampled/atlased for 3DS memory.
- Free-roam rival challenges and SP/gap-style highway battles.
- Persistent save/progression.
- Audio and atmosphere pass.
- Physical Old 3DS profiling and LOD tuning.
