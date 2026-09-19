# Unreleased

## Added

- Shared UDV/UDV2 glossary defining directional visibility, exposure gaps,
  stance-aware classifications, floor-marker meaning, and existing UI aliases.
- Offline native-collision research checkpoint: client/server interface distinction,
  existing HLAE lifecycle/schema limits, snapshot ownership requirements and the
  unresolved movement-versus-sight filtering issue. No native calls added.
- Collision conversion diagnostics include shape index/ID and the underlying error;
  a worker regression checks a failing second shape with a 64-bit ID.
- Owned collision-scene conversion for indexed meshes and ordered convex faces,
  affine transforms, fail-closed validation and asynchronous worker ingestion.
  Standalone fixtures cover transforms, malformed/unsupported inputs, cancellation
  and stale-result removal after scene failure. No native engine ABI is assumed.

- In-memory collision snapshot input, provenance/revision tracking and replacement
  tests. Native CS2 collision extraction itself is still outstanding.

- UDV2 architecture, feature definitions and compatibility baseline.
- Portable TRI/BVH, floor support/headroom sampling, rectangular FOV and
  reciprocal standing/crouching visibility.
- Latest-only worker with cancellation, invalidation and asynchronous map loading.
- Synthetic semantic/lifecycle tests and real-map benchmark tool.
- HLAE pose/map/demo bridge and native load/toggle/target/FOV/range/status commands.
- Instanced, configurable floor markers using HLAE's existing D3D11 world draw hook.
- Root agent guide with interruption-safe continuation/checkpoint rules.
- Primary CUDA backend: resident geometry, bounded launches, pinned readback,
  backend controls/status and strict GPU build option.
- Host-executed GPU algorithm parity and actual-device validation executable.

## Changed

- Product requires direct game collision data; TRI becomes a diagnostic importer,
  not the intended production source.

- Binned-SAH BVH and conservative bounds improve CPU fallback traversal; exhaustive
  ray checks cover hierarchy pruning. Map reading/building supports cancellation.
- Primary compute target is now CUDA on RTX 3060 Ti, per explicit user requirement;
  source implemented, device validation pending. CPU is reference/emergency fallback.
- Default floor grid is 32 units for the GPU target; spacing remains configurable.

## Fixed

- Release cached compute/map resources on the worker during map replacement.
- Clear stale error status after a successful analysis.
- Align GPU ray endpoint tolerance with CPU float precision.

## Validation

- Collision conversion regression fixtures now cover all six box faces in both
  ray directions, reversed winding, combined transforms, clear interior/exterior
  segments, concave/repeated vertices, transform bounds and mid-conversion cancel.
- CUDA 13.0 Linux compile/link passes; portable and sanitizer checks pass.
- Actual GPU parity is skipped here; Windows/RTX 3060 Ti and CS2 acceptance remain.
