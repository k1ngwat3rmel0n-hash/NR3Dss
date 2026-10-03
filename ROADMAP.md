# NR3DS roadmap

## Stable baseline through v0.011

- Citro3D 3D renderer
- Old 3DS-oriented vehicle physics
- High-speed precision steering
- Wall sliding / traffic collision handling
- Traffic AI and rival race
- Garage, tuning, cash and upgrades
- Improved low-poly showroom
- Data-driven expressway route
- Active world-chunk window
- Expressway visual zones, tunnel, underpass, elevated road and junction language
- Lower chase camera and stronger speed presentation

## Next: map reconstruction

1. Obtain raw Unity `globalgamemanagers` and highway `levelXX` scene files from the supplied PC build.
2. Identify the actual highway scene(s) and important GameObjects.
3. Recover road/waypoint/transform data: position, rotation, scale, junctions, ramps, tunnels and PA/garage entrances.
4. Convert those transforms into the `nr_world` route/chunk format.
5. Replace the temporary v0.011 test-route table section by section.

## v0.012 target

- First reconstructed section of the original road topology, if scene transforms are recoverable.
- Otherwise: first free-roam highway loop using the v0.011 chunk system while extraction continues.
- Rival encounter/challenge groundwork.

## Later

- Full streamed road network
- PA/rest-area rival encounters
- Free-roam -> challenge -> battle -> payout loop
- More faithful garage/customization stations
- Better low-poly cars and textures
- Audio / turbo / tire / tunnel atmosphere
- Persistent saves
- Physical Old 3DS profiling and optimization
