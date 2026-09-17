#include "Compute.h"
#include "Worker.h"
#include <iostream>

// Run on the deployment GPU. Unlike the host algorithm tests, this executes CUDA.
int main(int argc,char** argv) {
    try {
        std::string reason;
        std::unique_ptr<udv::Compute> gpu;
        try { gpu=udv::makeCompute(udv::BackendMode::Cuda,reason); }
        catch(const std::exception& e) { std::cerr<<"SKIP: GPU validation unavailable: "<<e.what()<<'\n'; return 77; }
        // Exercise occlusion/asymmetry on-device, not only open-space arithmetic.
        for(int scenario=0;scenario<4;++scenario) {
            std::vector<udv::Triangle> triangles;
            if(scenario>=2) {
                const float x=scenario==2?50.0f:10.0f,low=scenario==2?-100.0f:50.0f;
                triangles={{{x,-500,low},{x,500,low},{x,500,200}},{{x,-500,low},{x,500,200},{x,-500,200}}};
            }
            auto fixture=std::make_shared<udv::Map>("fixture",udv::Geometry(std::move(triangles)),32,udv::Cancel{});
            fixture->candidates={{{scenario==1?-100.0f:100.0f,0,0},{0,0,1},udv::Standing}};
            udv::Pose p; p.eye={0,0,64};
            auto result=gpu->run(fixture,p,2000,{});
            const auto expected=scenario==0?udv::Area::Vision:scenario==2?udv::Area::None:udv::Area::Gap;
            if(result.classes.size()!=1||result.classes[0].area()!=expected) throw std::runtime_error("GPU semantic fixture failed");
        }
        std::shared_ptr<udv::Map> map;
        if(argc>1) map=std::make_shared<udv::Map>("validation",udv::Geometry(udv::Geometry::readTri(argv[1])),argc>2?std::stof(argv[2]):32,udv::Cancel{});
        else {
            map=std::make_shared<udv::Map>("synthetic",udv::Geometry(std::vector<udv::Triangle>{}),32,udv::Cancel{});
            map->candidates={{{100,0,0},{0,0,1},3},{{-100,0,0},{0,0,1},3},{{0,100,0},{0,0,1},3}};
        }
        if(map->candidates.empty()) throw std::runtime_error("No candidates");
        size_t mismatches=0;
        std::cout<<"backend="<<gpu->name()<<" candidates="<<map->candidates.size()<<'\n';
        for(size_t i=0;i<20;++i) {
            udv::Pose pose;
            pose.feet=argc>1?map->candidates[i*map->candidates.size()/20].feet:udv::Vec3{};
            pose.eye=pose.feet+udv::Vec3{0,0,i%2?46.0f:64.0f}; pose.angles={float(int(i%5)-2)*10,float(i*37),0};
            pose.horizontalFov=i%3?106.26f:30; pose.aspect=16.0f/9;
            auto cpu=udv::analyze(map->geometry,map->candidates,pose,2000);
            auto result=gpu->run(map,pose,2000,{});
            if(result.classes.size()!=cpu.classes.size()) throw std::runtime_error("GPU output size mismatch");
            size_t different=0;
            for(size_t c=0;c<cpu.classes.size();++c) if(cpu.classes[c].vision!=result.classes[c].vision||cpu.classes[c].gap!=result.classes[c].gap) ++different;
            mismatches+=different;
            std::cout<<"pose="<<i<<" mismatches="<<different<<" cpu_ms="<<cpu.milliseconds
                <<" gpu_interval_ms="<<result.gpuMilliseconds<<" gpu_total_ms="<<result.milliseconds<<'\n';
        }
        if(!gpu->run(map,udv::Pose{},2000,[]{return true;}).classes.empty()) throw std::runtime_error("Cancelled GPU job published data");
        return mismatches?1:0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
