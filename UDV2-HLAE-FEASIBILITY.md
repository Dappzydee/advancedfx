# UDV2 HLAE Live Tactical Analysis Feasibility

## 1. Executive assessment
VERIFIED: HLAE exposes entity eyes, observer handles, demo ticks and a world D3D11
draw hook. A static-geometry approximation is feasible; live performance and
Windows integration remain to be validated.
## 2. Repository versions inspected
HLAE `79886f03747efe78e0038c3cf5cbf7e7eb4ce597` in `repos/advancedfx`;
UDV1 `68509a5484476610fa00911522ba5487d6b2f4fc` in `repos/Ultimate-Demo-Viewer`.
Both working trees clean initially. Workspace base `7281a9ebe097f443f09e86fe9e2cac4eaad0d9f1`.
## 3. Desired feature semantics
Vision = body sample inside player FOV with LOS. Gap = enemy sees player body
AND player does not see that enemy body, per plausible stance.
## 4. UDV1 concepts worth reusing
VERIFIED: `cs2_visibility/geometry.py` reads Awpy TRI and samples interior faces;
`raycasting.py` uses Trimesh CPU intersection or persistent Warp mesh queries.
Retain static geometry, acceleration, early FOV/range filtering, compact results.
## 5. UDV1 concepts to discard
Replace surface/conical vision and outside-FOV surface gaps with reciprocal body
visibility on floor positions. Irrelevant: browser viewer, session archives,
packed timeline masks, cumulative analysis, grenade/flash tooling.
## 6. Verified HLAE integration points
`AfxHookSource2/ClientEntitySystem.{h,cpp}`: IsPlayerPawn, GetOrigin,
GetRenderEyeOrigin/Angles, GetObserverMode/Target, GetHandle.
`MirvTime.cpp`: GetCurrentDemoTick uses engine demo file.
`main.cpp`: setup-view has original origin/angles/FOV before HLAE camera overrides;
UnkMakeMatrix supplies g_WorldToScreenMatrix. `CampathDrawer.cpp` illustrates
deferred-context rendering and engine/render handoff; `RenderSystemDX11Hooks.cpp`
has the world overlay draw site.
## 7. Candidate-position strategy
Choose static TRI floor sampling with slope, support and clearance heuristics.
Nav is more playable but no verified HLAE nav API. Runtime collision needs new
reverse engineering. A future hybrid can replace LOS without replacing semantics.
## 8. Vision classification design
Rectangular perspective FOV, body points, any sample visible. Sample model is an
approximation, not exact hitboxes. Current render eye incorporates stance.
## 9. Exposure-gap classification design
Enemy orientation unconstrained. Evaluate each stance independently; gap priority
when one stance is hidden/exposing even if another stance is visible.
## 10. Geometry / trace options
TRI stores nine float32 coordinates per triangle, no header (Awpy reader).
Native BVH over two-sided segments. No engine collision or dynamic occluders.
## 11. Rendering options
Separate UDV renderer on the existing world draw hook. Batched floor markers;
avoid one draw per cell. Reuse deferred-context state restoration pattern.
## 12. Compute options
CPU correctness implementation completed. User explicitly requires GPU as primary,
CPU as emergency fallback; deployment is RTX 3060 Ti on Windows. CUDA is selected
for worker-owned execution without touching CS2's immediate D3D context. D3D compute
remains a possible broader-hardware backend, not the first implementation.
## 13. Threading model
One worker, one pending pose, immutable map/results, cancellation generation.
No game entity or D3D calls from worker; no analysis on renderer.
## 14. Performance analysis
UDV1 CHANGELOG reports 0.301 s for 32 Dust II poses on RTX 3060 Ti; not reproduced
and not evidence of live single-pose latency. Native initial Release benchmark:
513,783 Dust II triangles, 55,109 candidates at spacing 32, median 188.5 ms,
max 602.4 ms across 20 synthetic poses (2,000-unit range, both stances).
Spacing 64: 13,396 candidates, median 108.4 ms, max 308.0 ms. These initial
median-split BVH results motivated traversal optimization; not CS2 frame measurements.
Binned-SAH on Ryzen 7 PRO 7840U / GCC 13.3 Release: latest 64-unit run median
13.45 ms, max 22.20 ms, map preparation 514.6 ms. Runs vary with laptop power/clock.
These measurements characterize the emergency fallback, not target GPU performance.
## 15. Maintenance / compatibility risk
Inherited HLAE hooks can break after CS2 updates. Fail closed when state invalid.
## 16. Licensing findings
HLAE root LICENSE is MIT. No explicit UDV1 license found: no source copied.
Awpy format inspected in https://awpy.readthedocs.io/en/latest/_modules/awpy/visibility.html .
Warp upstream https://github.com/NVIDIA/warp documents Apache-2.0; neither Warp
nor CUDA is redistributed or linked. Map assets remain user supplied.
## 17. Uncertainties
UNKNOWN: current CS2 build, live draw alignment, frame cost and scoped FOV behavior.
## 18. Potential issues
Floor heuristics include inaccessible surfaces. Static map versions can disagree
with demos. Sparse body samples miss small slivers, arms, feet and weapons.
## 19. Assumptions made autonomously
Standing/crouching approximations; gap color takes precedence across stances;
no new binary signatures; limit analysis cadence and spatial range.
## 20. Recommended architecture
Portable C++17 geometry/BVH/visibility/worker with thin HLAE bridge and D3D renderer.
## 21. Implementation phases
Documentation -> core and synthetic tests -> worker -> bridge/overlay -> profiling
and lifecycle tests -> final documentation. Windows runtime acceptance pending.
## 22. Acceptance tests
Open/front, behind, fully blocked, symmetric, asymmetric corner; FOV and stance;
TRI validation; candidate support; cancellation, disable, seek and target reset.
## 23. Files modified / likely future files
`AfxHookSource2/UDV/`, host CMake and minimal callbacks in main/render hooks;
this report and five docs files.
## 24. Current implementation status
Portable core and worker built with GCC 13.3 Release. Tests pass for required
visibility cases, FOV, finite segments, floor candidates, cancellation, latest-only
publication, disable/unload, map load failure and malformed TRI. Host integration
implemented with commands and batched D3D11 floor overlay. Static SDK/interface
inspection and portable regression tests completed. Windows host compilation,
shader execution, geometry alignment and CS2 lifecycle/frame-time acceptance
remain unverified. No new signatures/offsets introduced. The existing HLAE
`misc/mirv-script/src/snippets/mirv_script_view.ts` also identifies in-eye mode 2.
