# UDV and UDV2 glossary

Use these terms when discussing behavior, tests, UI, and future changes. **UDV**
means the original proof-of-concept project in `repos/Ultimate-Demo-Viewer`.
**UDV2** means the new HLAE-based implementation in `repos/advancedfx`.
Older documents may call the original project *UDV1*; that is the same project
as UDV, not a third version. UDV's visible-map-surface calculation does not
define UDV2's enemy-position semantics.

## The two classifications

At one **candidate position** and in one **enemy stance**, let:

- `P sees E`: at least one sampled point on a hypothetical enemy's body is
  inside the analyzed player's field of view and has an unobstructed line of
  sight from the player's eye.
- `E sees P`: at least one sampled point on the analyzed player's body has an
  unobstructed line of sight from the hypothetical enemy's eye. The hypothetical
  enemy may turn toward the player, so its recorded facing direction is irrelevant.

| Canonical term | Short alias in existing code/UI | Rule for one stance | Meaning of a marked floor position |
| --- | --- | --- | --- |
| **Visible enemy position** | Vision | `P sees E` | A hypothetical enemy in this stance at this position could be seen by the analyzed player. The floor itself need not be visible. |
| **Unseen exposure position** | Exposure gap, gap | `E sees P` AND NOT `P sees E` | A hypothetical enemy here could see the player while remaining unseen by that player. |
| **Neither classification** | None | Neither rule holds | No conclusion about safety; the enemy may be occluded in both directions, or the position may be outside analysis scope. |

Example: an enemy standing behind the player's camera in open space is an
**unseen exposure position**: `E sees P` is true, while `P sees E` is false
because the enemy is outside the player's field of view. Behind a solid wall
that blocks both directions, the position has neither classification.

Do not use *vision* alone to mean a visible floor surface, a cone without
occlusion, or everything the player could potentially spot after turning.
Do not use *gap* to mean every place the player cannot see. Neither
classification predicts whether an enemy actually occupies the position or
whether a shot would hit.

## Inputs and display

| Term | Definition |
| --- | --- |
| **Analyzed player (P)** | The selected demo player whose current pose, eye, and field of view define the analysis. |
| **Hypothetical enemy (E)** | An imagined enemy placed at a candidate position; it is not an observed CS2 entity. |
| **Candidate position** | A sampled floor point with approximate support and headroom for an enemy. It is not proof that the location is reachable or legal in-game. |
| **Enemy stance** | Standing or crouching. UDV2 evaluates each plausible stance separately and stores separate classification bits. |
| **Body sample** | One of five approximate points representing a body: head, chest, pelvis, or either lateral chest point. These are not CS2 bones or exact hitboxes. |
| **Field of view (FOV)** | The analyzed player's current viewing frustum. It limits `P sees E`; it does not limit `E sees P`. |
| **Line of sight (LOS)** | An unobstructed segment through the geometry supplied to UDV2, between an eye and a body sample. It is not Valve's visibility result. |
| **Floor marker / overlay** | A colored square centered on a candidate position. It visualizes a sampled classification, not a continuous painted walkable area. |
| **Display priority** | If any stance yields an unseen exposure position, the current overlay uses the gap color even if another stance yields a visible enemy position. Classification bits still retain both facts. Red therefore does not show every position where *some* stance is visible. |

The current default colors are red for visible enemy positions and orange for
unseen exposure positions. Colors are configurable and are not part of either
definition. The `mirv_udv vision` command currently toggles the whole overlay,
including gap markers; its name is an existing command, not a narrower semantic
definition.

## Geometry and implementation status

| Term | Definition |
| --- | --- |
| **Collision scene / snapshot** | UDV2-owned copied geometry plus a map/revision identity, consumed by the analysis worker. The input and conversion paths exist; a producer that extracts it directly from CS2 does not. |
| **TRI** | A raw triangle file used as a diagnostic geometry input. It is not UDV2's required production source. |
| **Static geometry approximation** | Visibility based on the supplied geometry and body samples. It currently does not account for smoke, other players, exact animations, or unrepresented dynamic objects. |

The classifier is in `AfxHookSource2/UDV/Core.cpp` and `GpuKernel.h`; stance
masks and display priority are in `Core.h`; floor markers are drawn in
`Overlay.cpp`. These semantics have portable tests, but the HLAE bridge and
overlay have not yet been validated in a live CS2 demo.
