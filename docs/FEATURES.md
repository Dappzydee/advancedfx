# Features

## Live Vision Area

Status: Experimental; core tested, native bridge/renderer not Windows-built or runtime verified.
FOV plus static LOS to representative enemy body points.
Default color: red. Candidate surfaces are approximate walkable space.

Commands: `mirv_udv load de_dust2 "C:/maps/de_dust2.tri" [spacing]`,
`mirv_udv vision 1|0`, `mirv_udv target auto|<entity index>`, `mirv_udv status`.
Load after the map starts; map initialization invalidates geometry.
Default spacing 64, range 2,000 units; `mirv_udv range <128..8000>`.
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
