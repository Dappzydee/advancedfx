# Architecture

Core and worker implemented; host/renderer integration in progress:

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

## Key decisions

### 2026-09-17 — CPU correctness first

Reason: portable tests and no additional runtime dependencies.
Consequences: profile Dust II before deciding whether GPU acceleration is needed.

### 2026-09-17 — Independent implementation

Reason: UDV1 has no explicit license in the inspected checkout.
Consequences: reuse concepts and geometry format, not UDV1 source code.
