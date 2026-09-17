# UDV2 live tactical analysis

Experimental native CS2 demo analysis hosted by AfxHookSource2. Vision marks
plausible enemy floor positions visible from the selected player's current POV.
Exposure gaps require reciprocal body visibility: the enemy can see the player
while that player cannot see the enemy. Static geometry only; not Valve visibility.

Portable geometry, reciprocal classification and asynchronous worker are implemented
and tested. HLAE integration is in progress; no live CS2 validation yet.

```sh
cmake -S AfxHookSource2/UDV -B build/udv -DCMAKE_BUILD_TYPE=Release
cmake --build build/udv
ctest --test-dir build/udv --output-on-failure
build/udv/udv_benchmark /path/to/de_dust2.tri 64
```

The host uses upstream Windows x64 presets (see ../BUILDING.md).
Windows/CS2 runtime validation is not available in this Linux development session.

See [architecture](ARCHITECTURE.md), [features](FEATURES.md),
[compatibility](COMPATIBILITY.md), [changelog](CHANGELOG.md), and the
[engineering report](../UDV2-HLAE-FEASIBILITY.md).
