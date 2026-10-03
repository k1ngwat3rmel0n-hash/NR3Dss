# Reverse-engineering / conversion notes — v0.016

## Complete sharedassets set

The four user-supplied `gpt1.7z` … `gpt4.7z` archives were unpacked successfully and collectively contain every `sharedassetsN.assets` file from 0 through 86 plus the available `.resS`/`.resource` companions.

A Unity 2018.4.26f1 SerializedFile parser was used to index object tables without requiring UnityPy/AssetRipper. Across all 87 sharedassets files the current catalog contains 53,372 serialized objects, including 5,210 Mesh, 337 Material and 1,892 Texture2D objects.

## Mesh decode

For uncompressed static road meshes, the converter decodes:

- submesh table
- 16/32-bit index buffers
- VertexData channel descriptors
- interleaved Float32 position channel

The source road meshes in this pass generally use stream 0 with position/normal/tangent/color/UV channels. v0.016 expands indexed triangles offline so the current 3DS renderer can continue using `C3D_DrawArrays(GPU_TRIANGLES, ...)`.

## Road-local normalization

The source PC meshes are authored in local XY with Z as vertical, but different scene families are rotated differently before their Unity Transform is applied. The v0.016 converter therefore chooses the longer source XY extent as the forward axis and the shorter one as lateral, then normalizes lateral half-width to 1.0. This lets each mesh family reuse the existing recovered-route frame.

This is not yet exact per-instance Unity scene placement. Exact scene MeshFilter/Transform reconstruction is being developed separately; v0.016 is the intermediate atlas step that provides substantially more authentic geometry while retaining the stable route renderer.

## Source atlas

See `tools/source_geometry_extraction/v016_source_atlas_manifest.json` for exact sharedassets/path IDs and raw bounds.

## Textures

Texture2D names/path IDs are now catalogued across the complete sharedassets set. Texture payload decoding, format conversion, UV retention and PICA200 texture upload are intentionally deferred rather than pretending fixed colors are original textures.
