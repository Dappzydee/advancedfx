# UDV2 live tactical analysis

Experimental native CS2 demo analysis hosted by AfxHookSource2. Vision marks
plausible enemy floor positions visible from the selected player's current POV.
Exposure gaps require reciprocal body visibility: the enemy can see the player
while that player cannot see the enemy. Static geometry only; not Valve visibility.

Portable geometry, reciprocal classification and asynchronous worker are implemented
and tested. CUDA primary backend and parity tooling compile/link with CUDA 13.0
on Linux; device execution is unverified. CPU is emergency fallback.
Native commands, pose bridge and batched D3D11 overlay are implemented
but have not been Windows-built or tested in CS2. Treat this as experimental source.

```sh
cmake -S AfxHookSource2/UDV -B build/udv -DCMAKE_BUILD_TYPE=Release
cmake --build build/udv
ctest --test-dir build/udv --output-on-failure
build/udv/udv_benchmark /path/to/de_dust2.tri 64
```

The host uses upstream Windows x64 presets (see ../BUILDING.md).
On the Windows RTX 3060 Ti PC, install a compatible CUDA Toolkit with VS 2022,
initialize upstream submodules, and require CUDA explicitly:

```text
cmake --preset x64-debug -DUDV_REQUIRE_CUDA=ON
cmake --build --preset x64-debug
cmake -S AfxHookSource2/UDV -B build/udv-gpu -A x64 -DUDV_REQUIRE_CUDA=ON
cmake --build build/udv-gpu --config Release
ctest --test-dir build/udv-gpu -C Release --output-on-failure
build/udv-gpu/Release/udv_gpu_validate.exe C:/maps/de_dust2.tri 32
```

A skipped GPU test is not successful validation. `UDV_REQUIRE_CUDA=ON` prevents
accidentally producing a CPU-only deployment build. Status must show `cuda / ...`.

Windows/CS2 runtime validation is not available in this Linux development session.

In a demo, after loading the map, use:

```text
mirv_udv load de_dust2 "C:/maps/de_dust2.tri" 32
mirv_udv vision 1
mirv_udv status
mirv_udv vision 0
```

Supply matching Awpy geometry yourself; assets are not included. First-person
spectating is the default. See FEATURES for explicit targets and colors.
Code: `AfxHookSource2/UDV/{Core,Worker,HlaeBridge,Overlay}.*`; portable tests and
benchmark are alongside them. The project `AGENTS.md` is the session handoff.

To continue on Windows, transfer this HLAE repository **with its local Git commits**.
The enclosing workspace ignores `repos/`; moving/cloning only that wrapper loses
the implementation. A Git bundle or your own fork can carry this history; nothing
has been pushed. GPU parity must return 0 (77 means unavailable), then verify live
POV/scoped FOV, floor/depth alignment, pause/seek/target/map changes, toggle-off,
and frame times with CUDA enabled versus disabled. See COMPATIBILITY for evidence.

See [architecture](ARCHITECTURE.md), [features](FEATURES.md),
[compatibility](COMPATIBILITY.md), [changelog](CHANGELOG.md), and the
[engineering report](../UDV2-HLAE-FEASIBILITY.md).
