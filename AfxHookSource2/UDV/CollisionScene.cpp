#include "CollisionScene.h"
#include <cmath>
#include <stdexcept>

namespace udv {
namespace {
constexpr size_t maxTriangles=10000000;
void require(bool ok,const char* reason) { if(!ok) throw std::runtime_error(reason); }
void cancelled(const Cancel& cancel) { if(cancel&&cancel()) throw std::runtime_error("Collision conversion cancelled"); }
bool bounded(Vec3 p) {
    return finite(p)&&std::abs(p.x)<=1000000&&std::abs(p.y)<=1000000&&std::abs(p.z)<=1000000;
}
Vec3 transform(const std::array<float,12>& m,Vec3 p) {
    return {m[0]*p.x+m[1]*p.y+m[2]*p.z+m[3],m[4]*p.x+m[5]*p.y+m[6]*p.z+m[7],m[8]*p.x+m[9]*p.y+m[10]*p.z+m[11]};
}
void append(std::vector<Triangle>& out,Vec3 a,Vec3 b,Vec3 c) {
    require(out.size()<maxTriangles,"Collision triangle budget exceeded");
    require(length(cross(b-a,c-a))>1e-6f,"Degenerate collision triangle");
    out.push_back({a,b,c});
}
}
std::vector<Triangle> convertCollision(const CollisionScene& scene,const Cancel& cancel) {
    require(!scene.mapName.empty(),"Collision snapshot has no map identity");
    require(scene.shapes.size()<=100000,"Too many collision shapes");
    std::vector<Triangle> out;
    size_t shapeIndex=0;
    try { for(;shapeIndex<scene.shapes.size();++shapeIndex) {
        const auto& shape=scene.shapes[shapeIndex];
        cancelled(cancel);
        require(shape.sight!=SightPolicy::Unresolved,"Unresolved collision sight policy");
        if(shape.sight==SightPolicy::NonOccluding) continue;
        require(shape.sight==SightPolicy::BlocksSight,"Invalid collision sight policy");
        require(shape.kind==ShapeKind::IndexedMesh||shape.kind==ShapeKind::ConvexFaces,"Unsupported sight-blocking shape");
        require(!shape.vertices.empty()&&shape.vertices.size()<=maxTriangles*3,"Invalid collision vertex count");
        const auto& m=shape.toWorld;
        for(float f:m) require(std::isfinite(f),"Nonfinite collision transform");
        const double det=double(m[0])*(double(m[5])*m[10]-double(m[6])*m[9])-
            double(m[1])*(double(m[4])*m[10]-double(m[6])*m[8])+
            double(m[2])*(double(m[4])*m[9]-double(m[5])*m[8]);
        require(std::abs(det)>1e-9,"Singular collision transform");
        std::vector<Vec3> world; world.reserve(shape.vertices.size());
        for(size_t i=0;i<shape.vertices.size();++i) {
            if(i%1024==0) cancelled(cancel);
            require(bounded(shape.vertices[i]),"Invalid collision vertex");
            auto p=transform(m,shape.vertices[i]); require(bounded(p),"Invalid world collision vertex"); world.push_back(p);
        }
        auto vertex=[&](uint32_t i) { require(i<world.size(),"Collision index out of range"); return world[i]; };
        if(shape.kind==ShapeKind::IndexedMesh) {
            require(shape.faces.empty()&&!shape.indices.empty()&&shape.indices.size()%3==0,"Invalid triangle topology");
            require(shape.indices.size()/3<=maxTriangles-out.size(),"Collision triangle budget exceeded");
            for(size_t i=0;i<shape.indices.size();i+=3) {
                if(i%3072==0) cancelled(cancel);
                append(out,vertex(shape.indices[i]),vertex(shape.indices[i+1]),vertex(shape.indices[i+2]));
            }
        } else {
            require(shape.indices.empty()&&!shape.faces.empty()&&shape.faces.size()<=maxTriangles,"Invalid convex topology");
            for(const auto& face:shape.faces) {
                cancelled(cancel);
                require(face.size()>=3&&face.size()<=256,"Invalid convex face size");
                std::vector<Vec3> points; points.reserve(face.size());
                for(auto index:face) points.push_back(vertex(index));
                auto normal=cross(points[1]-points[0],points[2]-points[0]); const float norm=length(normal);
                require(norm>1e-6f,"Degenerate convex face"); normal=normal*(1/norm);
                // Supporting-edge test rejects concavity, wrong order and bow-ties.
                // A fan is valid only for a simple, planar convex polygon.
                for(size_t i=0;i<points.size();++i) {
                    require(std::abs(dot(normal,points[i]-points[0]))<=0.01f,"Nonplanar convex face");
                    const auto edge=points[(i+1)%points.size()]-points[i]; const float len=length(edge);
                    require(len>1e-5f,"Repeated convex face vertex");
                    for(size_t j=0;j<points.size();++j)
                        require(dot(cross(edge,points[j]-points[i]),normal)>=-0.001f*len,"Nonconvex or unordered face");
                }
                for(size_t i=1;i+1<points.size();++i) append(out,points[0],points[i],points[i+1]);
            }
        }
    } } catch(const std::runtime_error& error) {
        throw std::runtime_error("Collision shape["+std::to_string(shapeIndex)+"] id="+
            std::to_string(scene.shapes[shapeIndex].id)+": "+error.what());
    }
    cancelled(cancel);
    return out;
}
}
