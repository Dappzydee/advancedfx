# Features

## Live Vision Area

Status: Experimental; core tested, native bridge/renderer not Windows-built or runtime verified.
FOV plus static LOS to representative enemy body points.
Default color: red. Candidate surfaces are approximate walkable space.

Commands: `mirv_udv load de_dust2 "C:/maps/de_dust2.tri" [spacing]`,
`mirv_udv vision 1|0`, `mirv_udv target auto|<entity index>`, `mirv_udv status`.
Load after the map starts; map initialization invalidates geometry.
Default spacing 32, range 2,000 units; `mirv_udv range <128..8000>`.
Automatic targeting follows first-person observer target and uses pre-override
game camera FOV with HLAE's AlienSwarm aspect scaling. Explicit targets require
`mirv_udv fov <horizontal degrees>` because their FOV need not match the camera.
This FOV/observer integration has not been tested in CS2.

Rendering: batched slope-aligned floor squares with depth testing. Use
`mirv_udv color vision 1 0 0 0.45` or `mirv_udv color gap 1 0.65 0 0.65`.
Values are RGBA in 0..1. Gap has deterministic priority when different stances
produce both classes. Status reports analysis latency and overlay CPU submission
time; this is not a measurement of GPU cost or CS2 frame-time impact.

## Exposure Gap

Status: Experimental; core tested, native runtime unverified. Enemy eye to player body LOS AND no player-to-enemy vision,
evaluated per stance. Hypothetical enemy may face the analyzed player.

## GPU analysis

Status: CUDA source compiled/linked on Linux; device and Windows validation pending.
Primary target: RTX 3060 Ti; CPU is emergency fallback.
`mirv_udv backend auto` prefers CUDA and records any fallback reason in status.
`mirv_udv backend cuda` is strict; `mirv_udv backend cpu` forces the reference path.
Backend changes invalidate old work. Status includes backend/device and GPU timing.
10 Hz submission throttling remains. GPU contention must be measured on Windows.

## Accuracy scope

Direct runtime collision extraction: **not implemented**. In-memory collision
snapshots and revision replacement are tested, but no CS2 callback supplies them
yet. The external TRI importer remains available for diagnostics only.
Owned indexed meshes and ordered convex faces can now be transformed/triangulated
on the worker. Unknown sight policy or unsupported occluders reject the scene;
this does not implement native extraction or runtime content/material filtering.

Level 1 approximation. Five samples: head, chest, pelvis and two lateral chest
points; any visible sample suffices. Standing/crouching enemy eye heights are
64/46 units; player body height derives from current eye-to-origin height.
Lateral samples currently lie on the world X axis, not animated model shoulders.
FOV is rectangular perspective; hypothetical enemies may orient toward the player.
Positions within 32 units of the player are excluded as overlapping hulls.

| State | First version |
| --- | --- |
| Static world | Owned scene/snapshot API, or diagnostic TRI; two-sided segment occlusion; native producer missing |
| Candidate floors | Slope <=45 degrees, grid, approximate support/headroom probes |
| Standing/crouching | Separate stance masks; gap priority across stances |
| Doors, breakables, dynamic props | Only their baked TRI representation, if present; no runtime changes |
| Smokes, molotovs, other utility | Ignored |
| Other players as occluders | Ignored |
| Boosts/jumps | No hypothetical airborne candidates |
| Exact models/bones/hitboxes | Not implemented |

Arms-only, feet-only and tiny slivers can be missed. Head-glitches are approximate;
weapon protrusion does not count. Model differences and animations are ignored.
Candidate probes do not establish reachability or perform a swept player hull:
roofs/inaccessible surfaces can pass. Floor squares may extend beyond support
edges and are diagnostic area samples, not exact walkable polygons. TRI map
version and visible render surfaces can disagree. No Valve-exact visibility claim.
