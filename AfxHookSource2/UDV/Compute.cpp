#include "Compute.h"
#include "Worker.h"
#include <stdexcept>
namespace udv {
namespace {
class Cpu final: public Compute {
public:
    Result run(const std::shared_ptr<const Map>& map,const Pose& p,float range,const Cancel& cancel) override {
        return analyze(map->geometry,map->candidates,p,range,cancel);
    }
    const char* name() const override { return "cpu"; }
};
}
#ifndef UDV_HAS_CUDA
std::unique_ptr<Compute> makeCudaCompute() { throw std::runtime_error("CUDA backend not compiled; configure UDV_REQUIRE_CUDA=ON on target PC"); }
#endif
std::unique_ptr<Compute> makeCompute(BackendMode mode,std::string& diagnostic) {
    diagnostic.clear();
    if(mode!=BackendMode::Cpu) {
        try { return makeCudaCompute(); }
        catch(const std::exception& e) { if(mode==BackendMode::Cuda) throw; diagnostic=e.what(); }
    }
    return std::make_unique<Cpu>();
}
}
