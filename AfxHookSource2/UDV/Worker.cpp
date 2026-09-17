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
    load_=Load{std::move(name),std::move(path),spacing,epoch_.load()};
    status_="loading geometry"; wake_.notify_one();
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
    if(unload) { ++epoch_; load_.reset(); map_.reset(); status_="no map loaded"; }
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
void Worker::run() {
    while(!stop_) {
        std::optional<Load> load;
        std::optional<Job> job;
        std::shared_ptr<const Map> map;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait(lock,[&]{return stop_||load_||(enabled_&&pending_&&map_);});
            if(stop_) return;
            if(load_) { load=std::move(load_); load_.reset(); }
            else { job=pending_; pending_.reset(); map=map_; }
            busy_=true;
        }
        try {
            if(load) {
                auto cancel=[&]{return stop_||epoch_!=load->epoch;};
                auto triangles=Geometry::readTri(load->path,cancel);
                if(!cancel()) {
                    auto built=std::make_shared<Map>(load->name,Geometry(std::move(triangles),cancel),load->spacing,cancel);
                    std::lock_guard<std::mutex> lock(mutex_);
                    if(!cancel()) { map_=std::move(built); status_="ready"; }
                }
            } else if(job) {
                auto cancel=[&]{return stop_||epoch_!=job->epoch||serial_!=job->serial;};
                auto frame=std::make_shared<Frame>(); frame->map=map;
                frame->result=analyze(map->geometry,map->candidates,job->pose,job->range,cancel);
                frame->result.generation=job->serial;
                std::lock_guard<std::mutex> lock(mutex_);
                if(!cancel()&&enabled_) latest_=std::move(frame);
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
