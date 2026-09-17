# Compatibility

Session constraint: offline development only. No Steam/CS2 launch, process attach,
injection, game-library execution or game-installation edits are authorized.
Future runtime validation needs separate explicit user authorization.

- HLAE base: `79886f03747efe78e0038c3cf5cbf7e7eb4ce597`.
- UDV1 inspected: `68509a5484476610fa00911522ba5487d6b2f4fc`.
- Last verified CS2 build: none (runtime validation outstanding).
- UDV2 revision: use `git log -- AfxHookSource2/UDV docs`.

HLAE's existing entity virtual calls, schema offsets, setup-view and D3D11 hooks
remain version-sensitive. No new signature or offset added. Bridge also uses
the pinned SDK's Source2EngineToClient001 GetLevelNameShort and IsPlayingDemo.
Unresolved schema fields, invalid target, wrong map, or non-demo state suppress
analysis. Incompatible upstream vtables can still crash: guards cannot validate
an arbitrary changed ABI. OBS_MODE_IN_EYE=2 and FOV scaling require runtime checks.

Pinned SDK submodule fetched: `2e1366353c50d40150276c5d580b0eecb183881b`.
Remaining upstream submodules are not initialized in this checkout. Windows
MSVC/SDK toolchain is absent here, so host compile acceptance is outstanding.

Overlay uses the Windows SDK D3DCompiler library (`d3dcompiler.lib`) to compile
small embedded SM5 shaders lazily. D3D11 initialization failure suppresses drawing
and warns once per device. Depth convention follows CampathDrawer (LESS_EQUAL);
projection/depth alignment must be validated in live CS2 and recording passes.

CUDA toolkit detection preserves upstream builds without CUDA; use
`UDV_REQUIRE_CUDA=ON` for deployment validation. Static cudart linkage, C++17,
`UDV_CUDA_ARCHITECTURES=80-virtual;86-real` defaults (Ampere PTX and 3060 Ti native).
Runtime selects the first NVIDIA device with capability >=8.0. GPU execution
is unverified. Match toolkit/driver/MSVC versions using NVIDIA's
[Windows installation guide](https://docs.nvidia.com/cuda/archive/13.0.0/cuda-installation-guide-microsoft-windows/index.html).

Validation record (2026-09-17): portable Release tests pass with GCC 13.3; ASan/UBSan
tests pass with leak detection disabled because LeakSanitizer cannot run under this
sandbox's ptrace. CUDA 13.0.48 compilation/linking passes, producing compute_80 PTX
and sm_86 code. Compiler components were checksum-verified and unpacked in `/tmp`,
not system-installed. The CUDA-linked portable test passes; actual-device tests
return 77: driver unavailable/insufficient. That is a skip, not GPU validation.
Full Windows host compilation, D3D shaders and CS2 execution remain unverified.

Native collision research: HLAE's inspected Source2 code/SDK contains no usable
trace or world-geometry extraction wrapper. Local Linux `libvphysics2.so` exports
CreateInterface and RnMeshCreate/Clone; it contains `VPhysics2_Interface_001`.
These observations do not establish Windows ABI, scene enumeration or ownership.
No guessed physics offsets/vtables were added. Native producer integration must
be validated against the target Windows CS2 build before calling engine methods.
