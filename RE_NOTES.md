# Reverse-engineering notes

The portable physics core is based on behavior inferred from the supplied IL2CPP build. Recovered systems already reflected in the prototype include:

- engine torque shaping
- engine inertia behavior
- turbo spool / decay structure
- heat soak and engine health power reduction
- tire temperature state
- gear/power-dependent grip trends
- progressive handbrake grip behavior

The original Unity serialized curve keyframes are still not present in this project. Values in `Vehicle::makeDefaultConfig()` are placeholders chosen to keep the prototype stable until those curves are extracted.

v0.005 adds original lightweight 3DS-side systems for traffic lane changing, collisions, road-edge response and visual highway curvature. These are not claimed to be recovered NIGHT-RUNNERS implementations.
