# Unreleased

## Added

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

- Binned-SAH BVH and conservative bounds improve CPU fallback traversal; exhaustive
  ray checks cover hierarchy pruning. Map reading/building supports cancellation.
- Primary compute target is now CUDA on RTX 3060 Ti, per explicit user requirement;
  source implemented, device validation pending. CPU is reference/emergency fallback.
