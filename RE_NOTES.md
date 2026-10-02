# Reverse-engineering notes used by NR3DS v0.001

These notes describe behavior inferred from the supplied IL2CPP build. They are not original source code.

## Confirmed architecture

The build exposes systems including:

- `CarData`
- `CarEngine`
- `RCC_CarControllerV3`
- `raceSpot`
- `sceneManager_traffic`
- `Traffic_AI_Local`

The native method map connects reconstructed managed methods to `GameAssembly.dll` RVAs.

## Implemented/recovered behavior in v0.001

### `CarData::enginePowerRealTime`

The recovered method contains:

- water/oil heat damage above roughly 130 C under full throttle;
- low engine-health power scaling below 20 health;
- heat-soak power reduction from combined water/oil temperature;
- additional forced-induction power;
- torque-curve evaluation using normalized RPM;
- a final `0.60` multiplier in the engine-torque calculation.

The portable core keeps these structural behaviors. Exact serialized curve shapes are not yet known.

### `CarData::TurboSuper`

Recovered behavior includes:

- spool state related to normalized RPM while on throttle;
- rapid spool decay off-throttle at approximately 5 units/second;
- a serialized turbo-power curve;
- gear/upgrade-dependent UI spool limits.

v0.001 implements the spool-state behavior and substitutes a 16-sample temporary curve.

### `CarData::ApplyTireGrip`

Recovered behavior applies front/rear forward/side grip scalars to four wheel colliders. One handbrake-related state uses a rear-side multiplier that progresses from roughly `0.20` toward `0.75`, while front grip can be raised to approximately `1.50` in that state.

v0.001 retains this front/rear grip redistribution but applies it to a custom chassis model instead of Unity WheelCollider.

### Handbrake timer

Recovered behavior:

```text
if handbrake is fully held:
    timer += dt * lerp(1.0, 0.1, clamp01(speed_kph / 150))
else:
    timer = 0
```

This behavior is implemented directly.

### Tire temperature

The original method keeps a normalized tire-temperature state, heats it under sliding, cools it slowly otherwise, then evaluates a serialized grip curve.

v0.001 implements the state machine and uses a temporary 16-sample curve.

## Not yet source-equivalent

The following are intentionally still approximations:

- exact serialized `AnimationCurve` keyframes;
- full `TireModel()` state table;
- full `driftForces()` behavior;
- clutch-kick and burnout state transitions;
- exact RCC WheelCollider contact-force behavior;
- PhysX rigid-body response;
- per-car serialized mass/gear/final-drive/tire configuration.

These will be replaced incrementally while keeping the 3DS implementation cheap.

## v0.004 implementation note
Traffic and speed-reactive camera behavior are original lightweight systems for the 3DS prototype; they are not claimed to be recovered NIGHT-RUNNERS code. The underlying engine/tire/turbo behavior remains the previously documented clean reimplementation based on observed IL2CPP behavior.
