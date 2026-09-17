# UDV2 live tactical analysis

Experimental native CS2 demo analysis hosted by AfxHookSource2. Vision marks
plausible enemy floor positions visible from the selected player's current POV.
Exposure gaps require reciprocal body visibility: the enemy can see the player
while that player cannot see the enemy. Static geometry only; not Valve visibility.

Portable geometry, reciprocal classification and asynchronous worker are implemented
and tested. Native commands, pose bridge and batched D3D11 overlay are implemented
but have not been Windows-built or tested in CS2. Treat this as experimental source.

```sh
cmake -S AfxHookSource2/UDV -B build/udv -DCMAKE_BUILD_TYPE=Release
cmake --build build/udv
ctest --test-dir build/udv --output-on-failure
build/udv/udv_benchmark /path/to/de_dust2.tri 64
```

The host uses upstream Windows x64 presets (see ../BUILDING.md).
Windows/CS2 runtime validation is not available in this Linux development session.

In a demo, after loading the map, use:

```text
mirv_udv load de_dust2 "C:/maps/de_dust2.tri" 64
mirv_udv vision 1
mirv_udv status
mirv_udv vision 0
```

Supply matching Awpy geometry yourself; assets are not included. First-person
spectating is the default. See FEATURES for explicit targets and colors.
Code: `AfxHookSource2/UDV/{Core,Worker,HlaeBridge,Overlay}.*`; portable tests and
benchmark are alongside them. The project `AGENTS.md` is the session handoff.

See [architecture](ARCHITECTURE.md), [features](FEATURES.md),
[compatibility](COMPATIBILITY.md), [changelog](CHANGELOG.md), and the
[engineering report](../UDV2-HLAE-FEASIBILITY.md).
