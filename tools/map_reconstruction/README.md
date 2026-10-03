# Map reconstruction status

v0.012 now consumes recovered Unity route transforms.

Current converted branch:

- `WP_AREA_2,1_HIGH_0`
- first connector portion of `WP_AREA_2,1_R_0`
- `WP_AREA_2,1_TUNNEL_0`

The source chain is resampled to an 8 m fixed-spacing route table in `core/nr_route_data.hpp`.
`core/nr_world.cpp` interpolates that data and builds a moving local road frame for rendering.

Next conversion work:

1. Parse selected `sharedassets1.assets` road/tunnel meshes.
2. Recover mesh vertex/index data and source transform associations.
3. Decimate/quantize into an Old-3DS-friendly mesh format.
4. Attach converted meshes to the recovered route/world chunks.
5. Expand from one branch into the junction-aware C1 graph.
