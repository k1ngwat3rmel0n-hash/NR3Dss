# Reverse-engineering / fidelity notes - v0.009

The portable vehicle core continues to preserve selected behavior reconstructed from the supplied IL2CPP NIGHT-RUNNERS build: engine torque flow, turbo-spool trends, engine heat/health effects, tire-temperature/grip behavior, handbrake grip transition and speed-sensitive steering behavior.

v0.009 adds a clean-room native garage/progression layer. The original build exposes garage/customization and engine/turbo/tire/gear-related systems, which supports using those categories, but the current prices, reward amounts, menu flow and level values are NR3DS-specific balancing rather than claims about exact original game data.

Current upgrade effects:

- Engine: +30 Nm base torque per level, plus strength/cooling headroom.
- Turbo: +25 hp maximum forced-induction contribution per level.
- Tires: +0.045 front/rear grip scalar per level.
- Final drive: user adjustable 3.20-5.00.
- Gear ratios: individually adjustable while maintaining descending ratio order.

The exact serialized Unity AnimationCurve keyframes are still not imported, so the portable torque/turbo/tire curves remain approximations.
