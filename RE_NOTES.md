# Reverse-engineering / fidelity notes - v0.011

The portable physics core still preserves selected behavior reconstructed from the supplied IL2CPP NIGHT-RUNNERS build: engine torque flow, turbo-spool trends, heat/health effects, tire-temperature/grip behavior, handbrake grip transition and speed-sensitive steering behavior.

v0.011 adds a **data-driven route representation** intended to receive reconstructed Unity scene data later. The current `RoadSection` values are not claimed to be exact NIGHT-RUNNERS coordinates. They are an original temporary route using the user's highway reference footage for visual direction.

The world format stores section start/length, style, lateral displacement, local bend amplitude, elevation change, road width and lane count. Rendering only considers a small chunk window around the player, providing the basis for a much larger road network without keeping the entire city active at once.

Current route styles are: Open, SodiumFence, Elevated, DenseCity, Underpass, Tunnel and Junction.

## What is still needed for accurate map reconstruction

The current available source material confirms that the Unity build contains many `level0..level86` scene files, but the raw scene bytes themselves are not available in the current working set. The file tree alone cannot supply Transform values.

The minimum useful next upload is `globalgamemanagers`, followed by whichever `levelXX` files correspond to the highway. Matching resource files may also be required depending on serialization references.

No original NIGHT-RUNNERS meshes, textures or logos are distributed in NR3DS.
