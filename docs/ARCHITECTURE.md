# Architecture

Planned first slice:

```text
HLAE demo/player state -> immutable pose -> latest-only CPU worker
                                              ^          |
                                      static TRI + BVH   v
HLAE world render hook <- immutable classified floor markers
```

Separate portable geometry/analysis/control from HLAE entity access and D3D11.
No new injection framework or Present hook. No timeline or offline demo scan.

## Key decisions

### 2026-09-17 — CPU correctness first

Reason: portable tests and no additional runtime dependencies.
Consequences: profile Dust II before deciding whether GPU acceleration is needed.

### 2026-09-17 — Independent implementation

Reason: UDV1 has no explicit license in the inspected checkout.
Consequences: reuse concepts and geometry format, not UDV1 source code.
