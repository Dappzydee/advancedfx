#include "Core.h"
#include <iostream>
#include <stdexcept>
using namespace udv;
static void check(bool value,const char* name) { if(!value) throw std::runtime_error(name); }
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
        Geometry floor({{{-128,-128,0},{128,-128,0},{128,128,0}},{{-128,-128,0},{128,128,0},{-128,128,0}}});
        auto candidates=generateCandidates(floor);
        check(candidates.size()==49,"floor support and deduplication");
        for(auto& x:candidates) check(x.stances==3,"floor headroom");
        auto result=analyze(floor,candidates,p,1000);
        check(result.classes.size()==49&&result.tested>0,"analysis");
        check(analyze(floor,candidates,p,1000,[]{return true;}).classes.empty(),"analysis cancellation");
        check(generateCandidates(floor,32,[]{return true;}).empty(),"candidate cancellation");
        std::cout<<"UDV semantics, FOV, segments, candidates and cancellation passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
