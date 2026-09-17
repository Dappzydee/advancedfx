# Architecture

Core, worker and host bridge implemented; renderer integration in progress:

```text
HLAE demo/player state -> immutable pose -> latest-only CPU worker
                                              ^          |
                                      static TRI + BVH   v
HLAE world render hook <- immutable classified floor markers
```

Separate portable geometry/analysis/control from HLAE entity access and D3D11.
No new injection framework or Present hook. No timeline or offline demo scan.

`Core.*` owns TRI parsing, median-split BVH, floor support/headroom probes and
per-stance body visibility. `Worker.*` owns one thread and one replaceable pending
pose. Epoch/serial checks cancel obsolete analysis and reject stale publication.
Map loading runs on the worker too. Disable clears results; unload discards map.
Readers copy immutable shared pointers under short locks, never wait for analysis.

`HlaeBridge.*` samples the original setup-view before HLAE camera overrides. It
resolves a live pawn through existing entity helpers, validates map and demo state,
and submits at most 10 Hz, skipping while work is running. The next submission
uses the current pose. Pause reuses an identical result; target changes, backward
ticks, jumps over 16 ticks and teleports invalidate it. Completed overlays are
rejected beyond 32 ticks of age. Large forward discontinuities are conservative
seek detection; exact same-tick seeks cannot be identified through tick alone.
Worker shutdown is in engine shutdown, never DllMain.

## Key decisions

### 2026-09-17 — CPU correctness first

Reason: portable tests and no additional runtime dependencies.
Consequences: profile Dust II before deciding whether GPU acceleration is needed.

### 2026-09-17 — Independent implementation

Reason: UDV1 has no explicit license in the inspected checkout.
Consequences: reuse concepts and geometry format, not UDV1 source code.
