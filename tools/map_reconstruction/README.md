# Map reconstruction input

The NR3DS world renderer is ready for extracted scene transforms, but the raw Unity scenes are required.

Start by supplying:

1. `NIGHT-RUNNERS PROLOGUE PATREON_Data/globalgamemanagers`
2. Candidate highway `levelXX` files
3. Any matching `levelXX.resS`, `sharedassetsXX.assets`, or `.resS` files requested during parsing

Desired output from reverse engineering:

- road centerline / waypoint positions
- transforms of road chunks
- ramp/junction connections
- tunnel starts/ends
- elevation profile
- parking-area / garage entrances
- traffic waypoints
- major landmark anchors

Those values can then be converted into `core/nr_world.cpp` without changing the 3DS renderer architecture.
