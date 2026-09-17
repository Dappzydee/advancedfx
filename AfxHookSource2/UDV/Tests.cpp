#include "Core.h"
#include "Worker.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include <random>
#include <iostream>
#include <stdexcept>
using namespace udv;
static void check(bool value,const char* name) { if(!value) throw std::runtime_error(name); }
template<class F> static void await(F fn,const char* name) {
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(!fn()) {
        if(std::chrono::steady_clock::now()>end) throw std::runtime_error(name);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
static std::vector<Triangle> wall(float x,float low,float high) {
    return {{{x,-500,low},{x,500,low},{x,500,high}},{{x,-500,low},{x,500,high},{x,-500,high}}};
}
int main() {
    try {
        Pose p; p.eye={0,0,64};
        Candidate c{{100,0,0},{0,0,1},Standing};
        Geometry empty({});
        check(classify(empty,p,c).area()==Area::Vision,"open/symmetric engagement");
        c.feet.x=-100;
        check(classify(empty,p,c).area()==Area::Gap,"behind player");
        c.feet.x=100;
        Geometry closed(wall(50,-100,200));
        check(classify(closed,p,c).area()==Area::None,"fully occluded");
        // Eye near cover: all A->B samples cross the wall above its bottom,
        // whereas B eye can see A pelvis below it. This is not an FOV gap.
        Geometry corner(wall(10,50,200));
        check(classify(corner,p,c).area()==Area::Gap,"asymmetric cover");
        check(inFov(p,{100,99,64})&&!inFov(p,{100,101,64}),"horizontal FOV boundary");
        check(!inFov(p,{100,0,140}),"vertical FOV boundary");
        p.horizontalFov=20;
        check(!inFov(p,{100,30,64}),"scoped FOV");
        p.horizontalFov=90; p.angles.y=90;
        check(inFov(p,{0,100,64})&&!inFov(p,{100,0,64}),"yaw");
        p.angles={90,0,0}; check(inFov(p,{0,0,-100}),"pitch");
        p.angles={}; c.stances=Crouching;
        check(classify(empty,p,c).vision==Crouching,"crouching stance mask");
        check(closed.blocked({0,0,64},{100,0,64})&&closed.blocked({100,0,64},{0,0,64}),"two-sided rays");
        check(!closed.blocked({0,0,64},{40,0,64}),"finite ray extent");
        check(!closed.blocked({0,0,64},{0,100,64}),"parallel slab");
        // Compare the accelerated hierarchy to exhaustive single-triangle queries.
        // This validates pruning/partitioning independently of traversal order.
        std::mt19937 random(42);
        std::uniform_real_distribution<float> coord(-1000,1000);
        auto point=[&]{return Vec3{coord(random),coord(random),coord(random)};};
        std::vector<Triangle> triangles;
        std::vector<Geometry> leaves;
        for(int i=0;i<150;++i) { triangles.push_back({point(),point(),point()}); leaves.emplace_back(std::vector<Triangle>{triangles.back()}); }
        Geometry hierarchy(triangles);
        for(int i=0;i<1500;++i) {
            auto from=point(),to=point(); bool expected=false;
            for(const auto& leaf:leaves) if(leaf.blocked(from,to)) { expected=true; break; }
            check(hierarchy.blocked(from,to)==expected,"BVH versus exhaustive rays");
        }
        Geometry floor({{{-128,-128,0},{128,-128,0},{128,128,0}},{{-128,-128,0},{128,128,0},{-128,128,0}}});
        auto candidates=generateCandidates(floor);
        check(candidates.size()==49,"floor support and deduplication");
        for(auto& x:candidates) check(x.stances==3,"floor headroom");
        auto result=analyze(floor,candidates,p,1000);
        check(result.classes.size()==49&&result.tested>0,"analysis");
        check(analyze(floor,candidates,p,1000,[]{return true;}).classes.empty(),"analysis cancellation");
        check(generateCandidates(floor,32,[]{return true;}).empty(),"candidate cancellation");
        const auto path=std::filesystem::temp_directory_path()/
            ("udv-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".tri");
        {
            std::ofstream file(path,std::ios::binary);
            for(const auto& t:floor.triangles()) {
                for(auto v:{t.a,t.b,t.c}) for(float f:{v.x,v.y,v.z}) file.write(reinterpret_cast<const char*>(&f),4);
            }
        }
        check(Geometry::readTri(path.string()).size()==2,"TRI roundtrip");
        {
            Worker worker;
            worker.load("fixture",path.string(),32);
            await([&]{return bool(worker.map());},"worker map load");
            worker.submit(p);
            check(!worker.latest(),"disabled worker does not publish");
            worker.enable(true); p.tick=10; worker.submit(p);
            await([&]{return bool(worker.latest());},"worker publishes");
            check(worker.latest()->result.pose.tick==10,"result tick");
            worker.invalidate();
            check(!worker.latest(),"seek clears published result");
            for(int i=11;i<100;++i) { p.tick=i; worker.submit(p); }
            await([&]{auto r=worker.latest();return r&&r->result.pose.tick==99;},"latest wins");
            worker.enable(false); check(!worker.latest(),"disable clears result");
            worker.invalidate(true); check(!worker.map(),"unload clears map");
            worker.load("bad",path.string()+".missing",32);
            await([&]{return worker.status()=="Cannot open TRI file";},"load failure reported");
        }
        {
            std::ofstream file(path,std::ios::binary|std::ios::app); file.put('x');
        }
        bool rejected=false;
        try { Geometry::readTri(path.string()); } catch(const std::exception&) { rejected=true; }
        check(rejected,"partial TRI rejected");
        std::filesystem::remove(path);
        std::cout<<"UDV semantics, FOV, segments, candidates and cancellation passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
