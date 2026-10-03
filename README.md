# NR3DS v0.017 — Source Textures

First real NIGHT-RUNNERS texture pass for the Old 3DS renderer.

## What changed

- Decoded the developer-authorized `_generic_ROAD_2_ALB` Texture2D from
  `sharedassets1.assets` / its `.resS` payload.
- Decoded `generic_TUNNEL_GRUNGE_ALB 1` from `sharedassets2.assets`.
- Selected repeat-friendly source regions and reduced each to 128x128.
- `tex3ds` converts both images to small ETC1/T3X textures at build time.
- Added UV-capable Citro3D rendering while retaining the existing vertex-color path.
- Real source asphalt is now overlaid on the recovered C1 road deck.
- Real source tunnel concrete is rendered on tunnel walls and ceiling.
- Texture brightness is reduced offline for the night scene; the texture pass is
  also multiplied by the existing distance fog.
- v0.016.1 curved-wall and black-slab fixes are preserved.
- The flat-colored HighRoad source stamping is temporarily disabled so it cannot
  cover the new asphalt layer. The mesh remains in the asset catalog for the
  later UV-preserving source-mesh conversion.

## Still intentionally unchanged

- Livisa body still uses the source mesh with flat materials; its material/UV pass comes next.
- Source road-line atlas is still represented by geometry rather than its original texture UVs.
- The real soundtrack is still only catalogued; the NDSP synth test remains until FSB5/Vorbis
  is transcoded offline.
- Some environment meshes remain road-local proxies until exact Unity scene transforms are applied.

## Texture source notes

See `tools/source_texture_extraction/README.md` and the preview images there.

## Build

GitHub Actions should produce:

`nr3ds_v017.3dsx`
