# Compatibility

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
