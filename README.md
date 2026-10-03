# NR3DS v0.017.3

Audio compatibility hotfix on top of v0.017.2.

- Keeps the endpoint-built curved tunnel/barrier walls.
- Keeps source road/tunnel textures, Livisa source mesh and long C1 route.
- Prefers NDSP on hardware.
- If `ndspInit()` fails (as shown in Azahar), automatically tries the legacy CSND service.
- No fake/zero-byte `dspfirm.cdc` is created.
- Bottom-screen HUD reports `CSND fallback loop playing` when the fallback succeeds.

The audio in this build is still the small original test loop; source NIGHT-RUNNERS soundtrack transcoding is the next audio milestone.

Expected artifact: `nr3ds_v0173.3dsx`.
