# NR3DS v0.017.2 — Curved Walls + Audio Retry

Hotfix based on the v0.017.1 screenshots.

## Wall fix

The tunnel/highway wall pieces were being rotated around their own centers. On a bend,
that leaves the square end cap of one long cuboid visible before the next piece catches
up, which looked like a beige/black wall pointing straight across the road.

v0.017.2 constructs the main concrete barrier spans from the **actual route endpoints**
at both ends of each 8 m section. The left/right wall endpoints are individually offset
using the route tangent, then the visible span is placed between those endpoints. This
makes neighboring walls meet along the bend instead of only sharing a center yaw.

When the real tunnel texture is available, the old long closed tunnel-wall cuboids are
also hidden so their end caps cannot appear through the textured wall strips.

## Audio

The screenshot reported `DSP unavailable`; that means `ndspInit()` failed before any
audio could play. libctru normally expects `sdmc:/3ds/dspfirm.cdc`.

For Azahar/Citra HLE, v0.017.2 now tries a temporary zero-byte `dspfirm.cdc` only when no
DSP file exists, retries NDSP, and deletes the temporary file on exit. It never overwrites
an existing DSP firmware file. Real 3DS hardware still requires a real dumped DSP
firmware file.

This remains the small synthesized playback test. The recovered NIGHT-RUNNERS FSB5
tracks are catalogued but are not yet decoded/streamed by the 3DS build.

Expected artifact: `nr3ds_v0172.3dsx`.
