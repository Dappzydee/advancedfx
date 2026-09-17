#include "Worker.h"
#include <exception>

namespace udv {
Map::Map(std::string n,Geometry g,float s,const Cancel& cancel)
    :name(std::move(n)),geometry(std::move(g)),candidates(generateCandidates(geometry,s,cancel)),spacing(s) {}
Worker::Worker() { thread_=std::thread([this]{run();}); }
Worker::~Worker() {
    stop_=true; ++epoch_; wake_.notify_one();
    if(thread_.joinable()) thread_.join();
}
void Worker::load(std::string name,std::string path,float spacing) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++epoch_; ++serial_; pending_.reset(); latest_.reset(); map_.reset();
    resetCompute_=true;
    load_=Load{std::move(name),std::move(path),spacing,epoch_.load(),std::nullopt,0};
    status_="loading geometry"; wake_.notify_one();
}
void Worker::loadSnapshot(std::string name,uint64_t revision,std::vector<Triangle> triangles,float spacing) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++epoch_; ++serial_; pending_.reset(); latest_.reset(); map_.reset(); resetCompute_=true;
    load_=Load{std::move(name),{},spacing,epoch_.load(),std::move(triangles),revision};
    status_="building collision snapshot"; wake_.notify_one();
}
void Worker::enable(bool value) {
    std::lock_guard<std::mutex> lock(mutex_);
    enabled_=value;
    ++serial_; pending_.reset(); latest_.reset();
    wake_.notify_one();
}
void Worker::invalidate(bool unload) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++serial_; pending_.reset(); latest_.reset();
    if(unload) { ++epoch_; load_.reset(); map_.reset(); resetCompute_=true; status_="no map loaded"; wake_.notify_one(); }
}
void Worker::submit(Pose pose,float range) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(!enabled_||!valid(pose)) return;
    pending_=Job{pose,range,epoch_.load(),++serial_};
    wake_.notify_one();
}
std::shared_ptr<const Frame> Worker::latest() const { std::lock_guard<std::mutex> lock(mutex_); return latest_; }
std::shared_ptr<const Map> Worker::map() const { std::lock_guard<std::mutex> lock(mutex_); return map_; }
std::string Worker::status() const { std::lock_guard<std::mutex> lock(mutex_); return status_; }
std::string Worker::backend() const { std::lock_guard<std::mutex> lock(mutex_); return backendName_; }
void Worker::backend(BackendMode mode) {
    std::lock_guard<std::mutex> lock(mutex_);
    backendMode_=mode; ++serial_; pending_.reset(); latest_.reset();
    resetCompute_=true; wake_.notify_one();
    backendName_="pending backend selection";
}
void Worker::run() {
    std::unique_ptr<Compute> compute;
    BackendMode currentMode=BackendMode::Auto;
    while(!stop_) {
        std::optional<Load> load;
        std::optional<Job> job;
        std::shared_ptr<const Map> map;
        BackendMode mode;
        bool reset=false;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait(lock,[&]{return stop_||resetCompute_||load_||(enabled_&&pending_&&map_);});
            if(stop_) return;
            reset=resetCompute_; resetCompute_=false;
            if(load_) { load=std::move(load_); load_.reset(); }
            else if(enabled_&&pending_&&map_) { job=pending_; pending_.reset(); map=map_; }
            busy_=true;
            mode=backendMode_;
        }
        try {
            if(reset) compute.reset();
            if(load) {
                auto cancel=[&]{return stop_||epoch_!=load->epoch;};
                auto triangles=load->triangles ? std::move(*load->triangles):Geometry::readTri(load->path,cancel);
                if(!cancel()) {
                    auto built=std::make_shared<Map>(load->name,Geometry(std::move(triangles),cancel),load->spacing,cancel);
                    built->fromSnapshot=load->triangles.has_value(); built->collisionRevision=load->revision;
                    std::lock_guard<std::mutex> lock(mutex_);
                    if(!cancel()) { map_=std::move(built); status_="ready"; }
                }
            } else if(job) {
                auto cancel=[&]{return stop_||epoch_!=job->epoch||serial_!=job->serial;};
                if(!compute||mode!=currentMode) {
                    std::string diagnostic;
                    compute=makeCompute(mode,diagnostic); currentMode=mode;
                    std::lock_guard<std::mutex> lock(mutex_);
                    backendName_=compute->name();
                    if(!diagnostic.empty()) backendName_+=" (emergency fallback: "+diagnostic+")";
                }
                auto frame=std::make_shared<Frame>(); frame->map=map;
                try { frame->result=compute->run(map,job->pose,job->range,cancel); }
                catch(const std::exception& e) {
                    if(mode!=BackendMode::Auto||std::string(compute->name())=="cpu") throw;
                    const std::string reason=e.what();
                    std::string unused; compute=makeCompute(BackendMode::Cpu,unused);
                    { std::lock_guard<std::mutex> lock(mutex_); backendName_="cpu (emergency fallback: "+reason+")"; }
                    frame->result=compute->run(map,job->pose,job->range,cancel);
                }
                frame->result.generation=job->serial;
                std::lock_guard<std::mutex> lock(mutex_);
                if(!cancel()&&enabled_) { latest_=std::move(frame); status_="ready"; }
            }
        } catch(const std::exception& e) {
            std::lock_guard<std::mutex> lock(mutex_);
            if((load&&epoch_==load->epoch)||(job&&epoch_==job->epoch&&serial_==job->serial)) {
                status_=e.what(); latest_.reset(); pending_.reset();
            }
        }
        busy_=false;
    }
}
}
