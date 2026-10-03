# Reverse-engineering / fidelity notes - v0.010

The portable vehicle core is intentionally unchanged from the stable v0.009 driving baseline. It continues to preserve selected behavior reconstructed from the supplied IL2CPP NIGHT-RUNNERS build: engine torque flow, turbo-spool trends, engine heat/health effects, tire-temperature/grip behavior, handbrake grip transition and speed-sensitive steering behavior.

v0.010 is primarily a presentation milestone. The tuning-shop scene is original NR3DS geometry designed to evoke the same broad Japanese aftermarket/showroom atmosphere as the user's visual references while staying suitable for Old 3DS hardware. No NIGHT-RUNNERS meshes, textures or logos are included.

Garage rendering remains geometry-only for now. The scene uses simple cuboids, fixed vertex colors and intentionally limited draw complexity. Future milestones can introduce small texture atlases after the basic scene composition and hardware performance are proven.

Current garage upgrade effects remain:

- Engine: +30 Nm base torque per level, plus strength/cooling headroom.
- Turbo: +25 hp maximum forced-induction contribution per level.
- Tires: +0.045 front/rear grip scalar per level.
- Final drive: user adjustable 3.20-5.00.
- Gear ratios: individually adjustable while maintaining descending ratio order.

The exact serialized Unity AnimationCurve keyframes are still not imported, so the portable torque/turbo/tire curves remain approximations.
