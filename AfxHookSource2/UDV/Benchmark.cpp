#include "Core.h"
#include <algorithm>
#include <chrono>
#include <iostream>
int main(int argc,char** argv) {
    if(argc<2) { std::cerr<<"Usage: udv_benchmark map.tri [spacing]\n"; return 1; }
    try {
        auto start=std::chrono::steady_clock::now();
        udv::Geometry geometry(udv::Geometry::readTri(argv[1]));
        auto candidates=udv::generateCandidates(geometry,argc>2?std::stof(argv[2]):32);
        std::cout<<"triangles="<<geometry.triangles().size()<<" candidates="<<candidates.size()
                 <<" load_build_candidates_ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<'\n';
        if(candidates.empty()) return 2;
        std::vector<double> times;
        for(size_t i=0;i<20;++i) {
            udv::Pose pose; pose.feet=candidates[i*candidates.size()/20].feet;
            pose.eye=pose.feet+udv::Vec3{0,0,64}; pose.angles.y=float(i*37); pose.horizontalFov=106.26f; pose.aspect=16.0f/9;
            auto r=udv::analyze(geometry,candidates,pose,2000);
            size_t vision=0,gap=0;
            for(auto c:r.classes) { vision+=c.area()==udv::Area::Vision; gap+=c.area()==udv::Area::Gap; }
            times.push_back(r.milliseconds);
            std::cout<<"pose="<<i<<" tested="<<r.tested<<" vision="<<vision<<" gap="<<gap<<" ms="<<r.milliseconds<<'\n';
        }
        std::sort(times.begin(),times.end());
        std::cout<<"median_ms="<<times[10]<<" max_ms="<<times.back()<<'\n';
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
