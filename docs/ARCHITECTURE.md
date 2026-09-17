# Architecture

Implemented pipeline (host runtime validation outstanding):

```text
HLAE demo/player state -> immutable pose -> latest-only analysis worker
                                              ^          |
                                    owned scene + BVH   v
HLAE world render hook <- immutable classified floor markers
```

Separate portable geometry/analysis/control from HLAE entity access and D3D11.
No new injection framework or Present hook. No timeline or offline demo scan.

`Core.*` owns TRI parsing, binned-SAH BVH, floor support/headroom probes and
per-stance body visibility. `Worker.*` owns one thread and one replaceable pending
pose. Epoch/serial checks cancel obsolete analysis and reject stale publication.
Map loading runs on the worker too. Disable clears results; unload discards map.
Readers copy immutable shared pointers under short locks, never wait for analysis.

Geometry input is independent of TRI: `Worker::loadSnapshot` accepts owned
world-space triangles and a collision revision, builds the BVH/candidates on the
worker, and invalidates old results/device geometry on replacement. Tests exercise
this path without reading files. The native CS2 producer remains outstanding;
do not mistake the input API for working game extraction. Raw engine pointers
must never be retained by CUDA or the worker. A future producer must resolve
scene ownership, material/content filtering, transforms, and update lifecycle.

`CollisionScene.*` defines UDV-owned meshes and ordered convex face loops, not an
engine ABI. `Worker::loadScene` converts local vertices using affine transforms on
the worker, then builds the same BVH/candidates and CUDA input. Conversion rejects
invalid indices, nonfinite/bounded coordinates, singular transforms, degenerate
triangles, nonplanar/nonconvex faces, unresolved sight policies and unsupported
occluders. Only explicitly non-occluding shapes may be skipped. Failure publishes
no partial scene; replacement clears stale results. Budgets and cancellation bound
conversion. Face checks do not establish closed hulls or whole-hull convexity;
spheres/capsules and native material/content policy are not implemented.
Shape-level conversion errors include the zero-based snapshot index and producer
shape ID in worker status, preserving the underlying reason for adapter debugging.

`Compute.*` selects CUDA first in auto mode. `CudaCompute.cu` keeps triangles,
BVH, ordering and candidates resident on the GPU in a low-priority nonblocking
stream owned by the worker. Batches of 2,048 candidates bound submitted work;
cancellation is checked between batches. Compact stance masks return through
pinned memory. No CUDA/D3D interop or immediate-context access from this backend.
Auto fallback records the initialization/execution failure. Strict CUDA mode never
falls back. Backend changes/map unload release resources on the worker; no
cudaDeviceReset. Priority is only a scheduling hint, not a D3D frame budget.
`GpuKernel.h` is host/device code: portable tests check its algorithms on CPU;
`udv_gpu_validate` separately checks actual device output. GPU event intervals
include gaps between bounded launches; total time includes upload/readback.

`HlaeBridge.*` samples the original setup-view before HLAE camera overrides. It
resolves a live pawn through existing entity helpers, validates map and demo state,
and submits at most 10 Hz, skipping while work is running. The next submission
uses the current pose. Pause reuses an identical result; target changes, backward
ticks, jumps over 16 ticks and teleports invalidate it. Completed overlays are
rejected beyond 32 ticks of age. Large forward discontinuities are conservative
seek detection; exact same-tick seeks cannot be identified through tick alone.
Worker shutdown is in engine shutdown, never DllMain.

`Overlay.*` uses HLAE's existing world draw site and actual world-to-screen matrix.
An engine/render packet queue mirrors CampathDrawer's frame boundaries, capped at
16 entries. Generation checks prevent pre-invalidation packets from drawing.
The renderer uses a separate deferred context, one DrawInstanced call, read-only
depth, alpha blending and ExecuteCommandList with context restoration. Instance
data changes only for a new result/color; matrices update each draw. Floor quads
follow candidate slope and sit 0.75 units above it. This is a sampled area overlay,
not a continuous navmesh fill. Device reset drops cached resources.

## Key decisions

### 2026-09-17 — Direct game collision is required

Reason: explicit user correction removes external TRI as a product dependency.
Consequences: retain TRI only as a diagnostic fixture importer. Reuse CUDA/BVH,
semantics, worker and renderer with snapshots from an isolated native adapter.
Native traces may be a correctness oracle; moving all classification to engine
CPU traces would violate the GPU-primary requirement. Dynamic scene updates and
collision-vs-visibility filtering must be verified rather than inferred.

### 2026-09-17 — CPU correctness first

Reason: portable tests and no additional runtime dependencies.
Consequences: provides the correctness reference and emergency fallback. Superseded
as the primary compute strategy by the GPU requirement below.

### 2026-09-17 — GPU primary, CPU emergency fallback

Reason: explicit user requirement; deployment is Windows on an RTX 3060 Ti.
Consequences: implement CUDA as primary compute, persistent device geometry and
compact result readback. Laptop CPU measurements do not justify deferring GPU work.

### 2026-09-17 — Independent implementation

Reason: UDV1 has no explicit license in the inspected checkout.
Consequences: reuse concepts and geometry format, not UDV1 source code.
