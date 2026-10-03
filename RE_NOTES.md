# Reverse-engineering / conversion notes — v0.017

## Texture decode milestone

The complete sharedassets set made it possible to resolve Texture2D objects to streamed
`.resS` payloads directly. For this pass:

- `sharedassets1.assets`, Texture2D path 190: `_generic_ROAD_2_ALB`,
  2048x2048, Unity DXT1.
- `sharedassets2.assets`, Texture2D path 187: `generic_TUNNEL_GRUNGE_ALB 1`,
  2048x2048, Unity DXT5.

The Unity 2018 Texture2D serialized layout was parsed manually. Streamed payload offsets and
sizes were used to extract the BC-compressed mip data from `.resS`; Pillow's BCn decoder was
used for offline verification.

## 3DS conversion

The first 3DS pass intentionally does not carry the full PC atlases. Repeat-friendly source
surface regions are cropped and resized to 128x128. Brightness is reduced for the night scene,
then `tex3ds` converts the PNGs to ETC1/T3X at build time.

The PICA shader now passes a second UV varying. Color-only geometry uses fixed UV attribute 2;
textured geometry streams position + UV while keeping vertex color as a fixed fog/tint factor.
The texture combiner uses `GPU_TEXTURE0 * GPU_PRIMARY_COLOR`.

This keeps all existing cuboid/source-mesh rendering intact and makes texture support an
incremental layer rather than a renderer rewrite.

## Why source-mesh road UVs are not enabled yet

The source road meshes already contain UV channels, but the v0.016 atlas converter currently
normalizes only positions and expands triangles. Reconstructing the original MeshFilter instance
transform plus UV stream is the next step. Until then the source asphalt is mapped on stable
road-local quads over the procedural collision deck.

## Preserved v0.016.1 fixes

Roadside lateral offsets continue to rotate with recovered-route yaw, visual segment spacing stays
at 8 m, and the bad free-standing LowRoad/Junction/Open stamping remains disabled until exact
Unity scene transforms are reconstructed.
