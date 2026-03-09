# Constructive Solid Geometry Demo

This sample demonstrates real-time Constructive Solid Geometry (CSG) using an A-Buffer
(per-pixel linked list) with analytical ray tracing and triangle mesh rasterization.
Four mesh–sphere pairs showcase each CSG operation — union, subtraction, intersection,
and symmetric difference — while a ground plane with configurable CSG allows interactive
experimentation.

## Features

- **A-Buffer** — per-pixel linked list of fragments stored in a `RWStructuredBuffer`.
  Head pointers use `RWTexture2D<uint>` with native texture atomics (MSL 3.1+ / HLSL).
- **Analytical ray tracing** — ray-sphere and ray-AABB intersections computed in the pixel shader
  produce entry/exit fragment pairs written into the A-Buffer.
- **Triangle mesh CSG** — STL meshes are rasterized with front/back face classification;
  each triangle emits an entry or exit fragment into the same A-Buffer.
- **CSG resolve (compute shader)** — a compute shader reads the linked list into
  groupshared memory (LDS), sorts fragments front-to-back, and walks them while tracking
  per-object inside/outside state to find the first visible CSG boundary.
- **CSG operations** — union, subtraction, intersection, and symmetric difference are
  evaluated per operation via `StructuredBuffer<CSGOperationAttribs>`.
- **Image-based lighting** — prefiltered environment map and BRDF integration LUT for
  specular IBL, plus directional and ambient terms.
- **Tone mapping** — Uncharted 2 tone mapping with adjustable exposure parameters.
- **Pipeline states** — all PSOs loaded from `RenderStates.json` via
  `IRenderStateNotationLoader`; runtime-only parameters set via `ModifyPipeline` callbacks.

## Render Passes

| Pass                     | Type     | Description |
|--------------------------|----------|-------------|
| ClearRadiance            | Clear    | Clear the radiance render target |
| ClearABuffer             | Compute  | Reset head pointers to `0xFFFFFFFF` and fragment counter to zero |
| GenerateCSGGeometry      | Graphics | Rasterize AABB / sphere proxy geometry; pixel shader ray-traces and writes A-Buffer fragments |
| GenerateCSGMesh          | Graphics | Rasterize STL mesh triangles; pixel shader writes entry/exit fragments |
| ResolveCSG               | Compute  | Sort fragments in LDS, evaluate CSG, light the first visible surface |
| ToneMapping              | Graphics | Apply Uncharted 2 tone mapping |
| GammaCorrection          | Graphics | Convert to sRGB when the swap chain format is not already sRGB |


## Controls

- **WASD** — Move camera
- **Mouse** — Look around
