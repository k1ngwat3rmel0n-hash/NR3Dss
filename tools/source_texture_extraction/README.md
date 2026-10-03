# v0.017 source texture extraction

Developer-authorized NIGHT-RUNNERS Unity textures used in this pass.

- Road source: `sharedassets1.assets`, Texture2D path 190, `_generic_ROAD_2_ALB`, 2048x2048 DXT1.
- Tunnel source: `sharedassets2.assets`, Texture2D path 187, `generic_TUNNEL_GRUNGE_ALB 1`, 2048x2048 DXT5.

The source DXT payloads were decoded from their `.resS` streams, cropped to repeat-friendly
surface regions, resized to 128x128, and brightness-scaled for the night scene. `tex3ds`
converts the PNGs to ETC1/T3X at build time.
