# Reverse-engineering / implementation notes — v0.008

The portable vehicle core continues to preserve the recovered structure used in prior builds: engine torque shaping, turbo spool, engine heat/health effects, tire temperature, handbrake grip redistribution, drift response, and high-speed steering filtering.

v0.008 adds an original lightweight race layer rather than attempting to reproduce Unity race objects directly. The new `RaceSession` is intentionally platform-independent and cheap enough for Old 3DS:

- fixed 3-second countdown
- 900 m sprint distance
- checkpoint progression at 225/450/675 m
- one scalar opponent speed state
- modest gap-based catch-up adjustment
- smoothed deterministic rival lane changes
- first-finisher win/loss state

This is a gameplay scaffold. It can later be tuned against the recovered `raceSpot::race_AICatchup`, `setup_Race`, countdown and finish behavior once more original constants/serialized values are mapped.
