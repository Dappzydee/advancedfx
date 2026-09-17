#pragma once
#include "Core.h"
#include <memory>
namespace udv {
struct Map;
enum class BackendMode { Auto, Cuda, Cpu };
class Compute {
public:
    virtual ~Compute()=default;
    virtual Result run(const std::shared_ptr<const Map>&,const Pose&,float,const Cancel&)=0;
    virtual const char* name() const=0;
};
// Construct, use and destroy on the analysis worker thread.
std::unique_ptr<Compute> makeCompute(BackendMode,std::string& diagnostic);
std::unique_ptr<Compute> makeCudaCompute();
}
