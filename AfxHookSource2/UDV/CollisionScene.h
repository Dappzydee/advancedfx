#pragma once
#include "Core.h"

namespace udv {
// UDV-owned representation, NOT an engine memory layout. Native extraction must
// copy coherent values and resolve sight-blocking policy before publishing.
enum class ShapeKind { IndexedMesh, ConvexFaces, Unsupported };
enum class SightPolicy { Unresolved, BlocksSight, NonOccluding };
struct CollisionShape {
    uint64_t id=0;
    ShapeKind kind=ShapeKind::Unsupported;
    SightPolicy sight=SightPolicy::Unresolved;
    std::array<float,12> toWorld{1,0,0,0, 0,1,0,0, 0,0,1,0}; // row-major affine
    std::vector<Vec3> vertices;
    std::vector<uint32_t> indices; // Mesh: triangle triples.
    std::vector<std::vector<uint32_t>> faces; // Hull: ordered convex face loops.
};
struct CollisionScene {
    std::string mapName;
    uint64_t revision=0;
    std::vector<CollisionShape> shapes;
};
// Throws on unsupported occluders, unresolved policy or malformed geometry.
// No partial scene is returned; silently dropping a wall would fabricate vision.
std::vector<Triangle> convertCollision(const CollisionScene&,const Cancel& cancel={});
}
