#include "CollisionScene.h"
#include "Worker.h"
#include <chrono>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
using namespace udv;
static void check(bool ok,const char* why) { if(!ok) throw std::runtime_error(why); }
template<class F> static void rejects(F fn,const char* why) {
    bool threw=false; try { fn(); } catch(const std::exception&) { threw=true; } check(threw,why);
}
template<class F> static void await(F fn) {
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(!fn()) { check(std::chrono::steady_clock::now()<end,"Worker timeout"); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
}
int main() {
    try {
        CollisionShape floor;
        floor.kind=ShapeKind::IndexedMesh; floor.sight=SightPolicy::BlocksSight;
        floor.vertices={{-128,-128,0},{128,-128,0},{128,128,0},{-128,128,0}};
        floor.indices={0,1,2,0,2,3};
        CollisionScene scene{"fixture",1,{floor}};
        auto triangles=convertCollision(scene);
        check(triangles.size()==2,"Indexed mesh");
        Geometry original(triangles);
        check(original.blocked({0,0,10},{0,0,-10}),"Original plane");
        // Rotate local XY floor 90 degrees around Y and translate to x=100.
        scene.shapes[0].toWorld={0,0,1,100, 0,1,0,0, -1,0,0,0};
        Geometry moved(convertCollision(scene));
        check(moved.blocked({90,0,0},{110,0,0})&&!moved.blocked({0,0,10},{0,0,-10}),"World transform");
        scene.shapes[0]=floor;
        scene.shapes[0].kind=ShapeKind::ConvexFaces; scene.shapes[0].indices.clear(); scene.shapes[0].faces={{0,1,2,3}};
        check(convertCollision(scene).size()==2,"Convex face triangulation");
        scene.shapes[0].toWorld[0]=-2; scene.shapes[0].toWorld[5]=3;
        check(convertCollision(scene).size()==2,"Reflection and nonuniform scaling");
        auto invalid=scene; invalid.shapes[0].faces={{0,2,1,3}};
        rejects([&]{convertCollision(invalid);},"Reject bow-tie");
        invalid=scene; invalid.shapes[0].vertices[2].z=5;
        rejects([&]{convertCollision(invalid);},"Reject nonplanar polygon");
        invalid=scene; invalid.shapes[0].faces={{0,1,99}};
        rejects([&]{convertCollision(invalid);},"Reject invalid hull index");
        invalid={"fixture",2,{floor}}; invalid.shapes[0].indices[0]=99;
        rejects([&]{convertCollision(invalid);},"Reject invalid mesh index");
        invalid.shapes[0]=floor; invalid.shapes[0].indices={0,1};
        rejects([&]{convertCollision(invalid);},"Reject incomplete triangle");
        invalid.shapes[0]=floor; invalid.shapes[0].toWorld[0]=0;
        rejects([&]{convertCollision(invalid);},"Reject singular transform");
        invalid.shapes[0]=floor; invalid.shapes[0].vertices[0].x=std::numeric_limits<float>::quiet_NaN();
        rejects([&]{convertCollision(invalid);},"Reject NaN");
        invalid.shapes[0]=floor; invalid.shapes[0].sight=SightPolicy::Unresolved;
        rejects([&]{convertCollision(invalid);},"Reject unknown sight policy");
        invalid.shapes[0]=floor; invalid.shapes[0].kind=ShapeKind::Unsupported;
        rejects([&]{convertCollision(invalid);},"Reject unsupported occluder");
        invalid.shapes[0].sight=SightPolicy::NonOccluding;
        check(convertCollision(invalid).empty(),"Explicit non-occluder omitted");
        rejects([&]{convertCollision(scene,[]{return true;});},"Cancellation");
        Worker worker; worker.backend(BackendMode::Cpu); worker.enable(true);
        worker.loadScene({"fixture",10,{floor}});
        await([&]{return bool(worker.map());});
        check(worker.map()->fromSnapshot&&worker.map()->collisionRevision==10&&worker.map()->candidates.size()==49,"Scene worker preparation");
        Pose p; p.eye={0,0,64}; worker.submit(p);
        await([&]{return bool(worker.latest());});
        invalid.shapes[0].sight=SightPolicy::BlocksSight;
        worker.loadScene(invalid);
        await([&]{return worker.status()=="Unsupported sight-blocking shape";});
        check(!worker.latest()&&!worker.map(),"Invalid scene cannot expose stale or incomplete results");
        std::cout<<"Collision conversion, transforms, validation and worker tests passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
