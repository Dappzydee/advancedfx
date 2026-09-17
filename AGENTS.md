# UDV2 Agent Guide

## Start here

Before changing anything:

1. Read `docs/README.md`.
2. Read `docs/ARCHITECTURE.md`.
3. Read `docs/FEATURES.md`.
4. Read `docs/COMPATIBILITY.md`.
5. Read `docs/CHANGELOG.md`.
6. Read `UDV2-HLAE-FEASIBILITY.md` if present.
7. Run `git status`.
8. Inspect recent history with `git log --oneline`.
9. Inspect any uncommitted changes before modifying them.
10. Build/test the current state where practical before beginning new work.

## Project purpose

UDV2 analyzes the current native CS2 demo pose: floor positions where a player
can see a hypothetical enemy, or that enemy can see the player while unseen.

## Current architecture

Portable C++ geometry/BVH and reciprocal body sampling feed a latest-only worker;
a thin HLAE bridge supplies poses and a D3D11 renderer displays floor markers.
See `docs/ARCHITECTURE.md`; Windows/CS2 validation is still outstanding.

## Development rules

- Current session is offline development only: do not launch Steam/CS2, attach to
  processes, inject/load game libraries, or change the game installation. Read-only
  file/source research and standalone UDV builds/tests are permitted. Runtime work
  requires new explicit user authorization; earlier testing plans are not consent.

- Current product semantics override UDV1 prototype behavior. The workspace
  specification is `../../UDV2-HLAE-LIVE-VISION-GAP-AUTONOMOUS-AGENT-PROMPT.md`.
- Reuse HLAE infrastructure; no duplicate hooks or unnecessary CS2 offsets/signatures.
- Keep UDV2 modular under `AfxHookSource2/UDV`; never block rendering with analysis.
- GPU is the primary compute backend; CPU is only the correctness reference and
  emergency fallback. Target PC is Windows with RTX 3060 Ti; laptop timings are
  not grounds for dropping GPU implementation.
- Production collision must come directly from CS2, without external TRI assets.
  Snapshot ingestion is implemented; the native producer is not. TRI is diagnostic.
- Preserve unrelated user changes and work in coherent vertical slices.
- Build/test before declaring a milestone complete; distinguish portable tests,
  static inspection, Windows builds and live CS2 verification.
- Before each substantial new phase, update relevant docs and commit the last
  validated milestone. Never leave substantial completed work only uncommitted.
- Create meaningful commits; do not squash useful history.
- Update this guide when constraints, continuation procedure or next step change.

## Session continuation

If a previous session was interrupted: inspect `git status`, recent commits,
docs/report, and the last completed milestone. Inspect uncommitted work, validate
the existing state, and continue from there rather than repeating research.
Use the portable build in `docs/README.md`; full host builds require Windows.
Do not claim native runtime success from portable tests alone.

## Current next step

Investigate native Windows CS2 collision access/ownership offline; owned mesh/face
conversion and worker ingestion are tested. Start with
`docs/NATIVE-COLLISION-RESEARCH.md` for inspected routes and unresolved contracts.
Do not invent ABI offsets or claim
these tests validate extraction. Windows host build and strict CUDA parity remain;
live CS2 acceptance requires separate explicit authorization.
Transfer this nested repository's Git history; the wrapper ignores `repos/`.
