# UDV2 live tactical analysis

Experimental native CS2 demo analysis hosted by AfxHookSource2. Vision marks
plausible enemy floor positions visible from the selected player's current POV.
Exposure gaps require reciprocal body visibility: the enemy can see the player
while that player cannot see the enemy. Static geometry only; not Valve visibility.

Implementation is in progress. The portable core will have its own CMake test
target; the host uses upstream Windows x64 presets (see ../BUILDING.md).
Windows/CS2 runtime validation is not available in this Linux development session.

See [architecture](ARCHITECTURE.md), [features](FEATURES.md),
[compatibility](COMPATIBILITY.md), [changelog](CHANGELOG.md), and the
[engineering report](../UDV2-HLAE-FEASIBILITY.md).
