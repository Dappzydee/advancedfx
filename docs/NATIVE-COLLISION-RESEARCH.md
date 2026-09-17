# Native collision access: offline findings

Research checkpoint: 2026-09-17, implementation baseline `137f5627`.
Scope: source inspection and public text/JSON downloads only. No game execution,
process attachment, game-library loading, installation edits or third-party code
execution. No external code copied into UDV2 and no new dependency added.

## Conclusion

The inspected sources do **not** establish a callable Windows CS2 client interface
for collision-world enumeration or a safe snapshot lifetime. The existing owned
scene converter is ready to receive copied values, but is not an extractor.
Do not implement a native adapter using server addresses or guessed layouts.

## Evidence and consequences

| Inspected source | Verified observation | Consequence for UDV2 |
| --- | --- | --- |
| Local `AfxHookSource2/SchemaSystem.cpp`, `getOffsetsFromSchemaSystem` / `HookSchemaSystem` | Only `client.dll` scopes are scanned; discovered offsets are copied into selected fields, then the lookup map is cleared. | Existing schema infrastructure is reusable, but does not currently expose physics/model module schemas or preserve arbitrary fields. Schema metadata alone does not locate live objects or establish ownership. |
| Local `ClientEntitySystem.cpp`, `New_OnAddEntity` / `New_OnRemoveEntity` | Add notification follows the original call; remove notification precedes original removal, with HLAE reference invalidation. | Reuse these sites for identity/invalidation if needed. They are not proof that physics shapes already exist, are immutable, or cover static world geometry. |
| Local `main.cpp`, level initialization / frame-stage callbacks | Existing level reset and before/after frame-stage sites are available. | No duplicate hook is inherently needed for scheduling. No inspected contract establishes a physics read lock or safe copy stage. |
| [AlliedModders CS2 tree](https://github.com/alliedmodders/hl2sdk/tree/3b9adbdf39b4dead8d5d2307072cc47e9ba19112) | Recursive filename search for physics/trace/collision found `public/gametrace.h`, not a physics-world header. Its [tracker](https://github.com/alliedmodders/hl2sdk/issues/132) names query/world interfaces as work areas. | A research lead, not an available verified client world-enumeration SDK. Filename search is not proof no relevant declaration exists anywhere. |
| [ModSharp CTrace.h](https://github.com/Kxnrl/modsharp-public/blob/6f682d91978b506d4475d9019096b9927537740c/Engine/src/cstrike/type/CTrace.h) | Trace wrappers explicitly call `address::server`; hit results expose body/shape pointers and a triangle index. | A hit query is not complete geometry enumeration. These pointers have no demonstrated lifetime suitable for our worker. Server calls are not verified client-demo calls. |
| [ModSharp query manager](https://github.com/Kxnrl/modsharp-public/blob/6f682d91978b506d4475d9019096b9927537740c/Sharp.Core/Managers/PhysicsQueryManager.cs) | Initialization resolves server filter vtables and gamedata addresses for the query object and trace functions. | Not a drop-in HLAE interface. No signatures, offsets or implementation copied; inspected files carry AGPL notices. |
| [ModSharp collision properties](https://github.com/Kxnrl/modsharp-public/blob/6f682d91978b506d4475d9019096b9927537740c/Engine/src/cstrike/type/CCollisionProperty.h) | Schema-backed attributes include interaction masks, collision group and identity fields. | Filtering needs more than positions. Client availability and sight semantics remain unverified. |
| [Source 2 Viewer aggregate reader](https://github.com/ValveResourceFormat/ValveResourceFormat/blob/master/ValveResourceFormat/Resource/ResourceTypes/PhysAggregateData.cs), inspected 2026-09-17 | Resource data includes parts, bind poses, bone relationships, surface-property hashes and collision attributes/tags. | A model resource may describe local geometry but still needs instance transforms and current scene state. Serialized resource fields are not a verified live-memory ABI. |

## Candidate routes — not implemented

1. **Live physics world:** locate the client world, enumerate active bodies/shapes,
   copy topology, current transforms and filtering metadata. Closest match to the
   requirement, but world access, enumeration ABI and lifetime are all unresolved.
2. **Loaded model/physics resources:** resolve resources already used by world and
   prop instances, combine resource geometry with instance state. Potentially
   reusable schema/resource infrastructure, but resource access, complete world
   coverage, reference retention and mutation semantics remain unresolved too.
3. **Engine traces:** useful as a future comparison oracle once the client query
   ABI/filter is verified. They do not supply a complete GPU scene, and replacing
   GPU classification with CPU engine traces would violate the primary-backend goal.

Reading installed VPK resources would be a separate asset-reading route, not proof
of live extraction. Do not silently substitute that or resurrect external TRI as
the production requirement.

## Safe snapshot contract to establish

- Record exact Windows client/module build identity before accepting any ABI.
- Establish scene readiness and either a documented/verified read-safe callback,
  retained immutable resources, or physics synchronization. Merely running on the
  main thread is not evidence that asynchronous physics cannot mutate the data.
- Copy values while ownership is valid; never enqueue engine pointers. Do not
  spread a copy across frames without retention plus a consistency check.
- Capture shape IDs/generations, transforms and map revision coherently. Reject or
  restart on unload/replacement. Publish only complete snapshots; do conversion,
  BVH construction and GPU upload on the worker. Measure copy cost separately.
- Prove static world coverage as well as prop coverage. Entity enumeration alone
  is insufficient evidence. Determine whether hidden/broken shapes remain present.

## Filtering issue to resolve before wiring

UDV currently uses the same geometry for floor support/headroom and sight rays.
Its `SightPolicy` is per shape. This is insufficient to express a movement-only
clip surface or mixed sight-blocking materials within one mesh. The future adapter
may need per-face grouping and separate movement/sight geometry views. Do not mark
all physical collision opaque, or drop non-occluding geometry from movement tests
without an explicit accuracy decision. Surface hashes/tags are evidence to inspect,
not an established glass/opacity policy.

## Next bounded milestone

Obtain matching Windows client/schema evidence **offline** to evaluate the two
geometry routes above: a world/resource access chain, shape layouts, instance
transforms, and an ownership/synchronization contract. If unavailable, report these
specific missing facts rather than adding speculative native calls. Full Windows
host compilation and standalone CUDA parity can proceed independently without
Steam/CS2. Live acceptance remains subject to separate explicit authorization.
