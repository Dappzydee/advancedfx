#include "Compute.h"
#include "Worker.h"
#include "GpuKernel.h"
#include <cuda_runtime.h>
#include <chrono>
#include <stdexcept>
#include <thread>

namespace udv {
namespace {
void checked(cudaError_t error) {
    if(error!=cudaSuccess) throw std::runtime_error(std::string("CUDA: ")+cudaGetErrorString(error));
}
template<class T> struct DeviceBuffer {
    T* ptr=nullptr;
    ~DeviceBuffer() { if(ptr) cudaFree(ptr); }
    void allocate(size_t count) {
        if(ptr) { checked(cudaFree(ptr)); ptr=nullptr; }
        if(count) checked(cudaMalloc(reinterpret_cast<void**>(&ptr),sizeof(T)*count));
    }
    void upload(const std::vector<T>& data,cudaStream_t stream) {
        allocate(data.size());
        if(!data.empty()) checked(cudaMemcpyAsync(ptr,data.data(),sizeof(T)*data.size(),cudaMemcpyHostToDevice,stream));
    }
};
__global__ void classifyKernel(gpu::Scene scene,const Candidate* candidates,Classification* output,
                              gpu::View view,uint32_t begin,uint32_t end) {
    uint32_t i=begin+blockIdx.x*blockDim.x+threadIdx.x;
    if(i<end) output[i]=gpu::classify(scene,view,candidates[i]);
}
class Cuda final:public Compute {
    cudaStream_t stream_=nullptr;
    cudaEvent_t start_=nullptr,end_=nullptr;
    DeviceBuffer<Triangle> triangles_;
    DeviceBuffer<BvhNode> nodes_;
    DeviceBuffer<uint32_t> order_;
    DeviceBuffer<Candidate> candidates_;
    DeviceBuffer<Classification> output_;
    Classification* host_=nullptr;
    std::shared_ptr<const Map> map_;
    std::string name_;
    void wait() {
        for(;;) {
            auto result=cudaStreamQuery(stream_);
            if(result==cudaSuccess) return;
            if(result!=cudaErrorNotReady) checked(result);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
public:
    Cuda() {
        int count=0; checked(cudaGetDeviceCount(&count));
        if(!count) throw std::runtime_error("CUDA: no NVIDIA device available");
        // First compatible NVIDIA GPU; no D3D interop/device matching is required.
        int chosen=-1;
        cudaDeviceProp prop{};
        for(int i=0;i<count;++i) { checked(cudaGetDeviceProperties(&prop,i)); if(prop.major>=8) { chosen=i; break; } }
        if(chosen<0) throw std::runtime_error("CUDA: this build targets compute capability 8.0 or newer");
        checked(cudaSetDevice(chosen));
        name_=std::string("cuda / ")+prop.name;
        try {
            int least=0,greatest=0; checked(cudaDeviceGetStreamPriorityRange(&least,&greatest));
            checked(cudaStreamCreateWithPriority(&stream_,cudaStreamNonBlocking,least));
            checked(cudaEventCreate(&start_)); checked(cudaEventCreate(&end_));
        } catch(...) { if(start_) cudaEventDestroy(start_); if(stream_) cudaStreamDestroy(stream_); throw; }
    }
    ~Cuda() override {
        // Only the worker waits; never synchronize the D3D/render thread or reset
        // the process-wide CUDA device (other plugins may use it).
        if(stream_) cudaStreamSynchronize(stream_);
        if(host_) cudaFreeHost(host_);
        if(start_) cudaEventDestroy(start_);
        if(end_) cudaEventDestroy(end_);
        if(stream_) cudaStreamDestroy(stream_);
    }
    const char* name() const override { return name_.c_str(); }
    Result run(const std::shared_ptr<const Map>& map,const Pose& pose,float range,const Cancel& cancel) override {
        Result result; result.pose=pose;
        const auto start=std::chrono::steady_clock::now();
        if(!valid(pose)||!std::isfinite(range)||range<=0||(cancel&&cancel())) return result;
        if(map_!=map) {
            // Keep source vectors alive until all asynchronous copies complete.
            wait(); map_.reset();
            triangles_.upload(map->geometry.triangles(),stream_);
            nodes_.upload(map->geometry.nodes(),stream_);
            order_.upload(map->geometry.order(),stream_);
            candidates_.upload(map->candidates,stream_);
            output_.allocate(map->candidates.size());
            if(host_) { checked(cudaFreeHost(host_)); host_=nullptr; }
            if(!map->candidates.empty()) checked(cudaMallocHost(reinterpret_cast<void**>(&host_),map->candidates.size()*sizeof(Classification)));
            wait(); map_=map;
        }
        const auto count=static_cast<uint32_t>(map->candidates.size());
        gpu::Scene scene{triangles_.ptr,nodes_.ptr,order_.ptr,static_cast<uint32_t>(map->geometry.nodes().size())};
        const auto view=gpu::prepare(pose,range);
        checked(cudaEventRecord(start_,stream_));
        // Bounded launches allow cancellation between chunks, limiting queued GPU
        // work. Stream priority is only a scheduling hint, not a D3D frame budget.
        for(uint32_t begin=0;begin<count;begin+=2048) {
            if(cancel&&cancel()) return result;
            const uint32_t end=begin+2048<count?begin+2048:count;
            classifyKernel<<<(end-begin+127)/128,128,0,stream_>>>(scene,candidates_.ptr,output_.ptr,view,begin,end);
            checked(cudaGetLastError()); wait();
        }
        checked(cudaEventRecord(end_,stream_));
        if(count) checked(cudaMemcpyAsync(host_,output_.ptr,count*sizeof(Classification),cudaMemcpyDeviceToHost,stream_));
        wait();
        if(cancel&&cancel()) return result;
        float milliseconds=0; checked(cudaEventElapsedTime(&milliseconds,start_,end_)); result.gpuMilliseconds=milliseconds;
        if(count) result.classes.assign(host_,host_+count);
        for(const auto& c:map->candidates) { const auto d=c.feet-pose.feet; float squared=dot(d,d); if(squared>=32*32&&squared<=range*range) ++result.tested; }
        result.milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        return result;
    }
};
}
std::unique_ptr<Compute> makeCudaCompute() { return std::make_unique<Cuda>(); }
}
