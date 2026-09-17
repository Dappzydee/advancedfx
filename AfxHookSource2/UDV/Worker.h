#pragma once
#include "Core.h"
#include "Compute.h"
#include "CollisionScene.h"
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

namespace udv {
struct Map {
    std::string name;
    uint64_t collisionRevision=0;
    bool fromSnapshot=false;
    Geometry geometry;
    std::vector<Candidate> candidates;
    float spacing;
    Map(std::string name, Geometry geometry, float spacing, const Cancel& cancel);
};
struct Frame {
    std::shared_ptr<const Map> map;
    Result result;
};
class Worker {
public:
    Worker();
    ~Worker();
    Worker(const Worker&)=delete;
    Worker& operator=(const Worker&)=delete;
    void load(std::string name,std::string path,float spacing);
    // Owns world-space collision triangles; no engine pointers cross threads.
    // The adapter must capture a coherent scene on an engine-safe callback.
    void loadSnapshot(std::string name,uint64_t revision,std::vector<Triangle> triangles,float spacing=32);
    void loadScene(CollisionScene scene,float spacing=32);
    void enable(bool value);
    void invalidate(bool unload=false);
    void submit(Pose pose,float range=2000);
    void backend(BackendMode);
    std::string backend() const;
    std::shared_ptr<const Frame> latest() const;
    std::shared_ptr<const Map> map() const;
    std::string status() const;
    bool busy() const { return busy_.load(); }
private:
    struct Load {
        std::string name,path;
        float spacing;
        uint64_t epoch;
        std::optional<std::vector<Triangle>> triangles;
        uint64_t revision=0;
        std::optional<CollisionScene> scene;
    };
    struct Job { Pose pose; float range; uint64_t epoch,serial; };
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::thread thread_;
    std::atomic<bool> stop_{false},busy_{false};
    std::atomic<uint64_t> epoch_{0},serial_{0};
    bool enabled_=false;
    BackendMode backendMode_=BackendMode::Auto;
    bool resetCompute_=false;
    std::string backendName_="not initialized (GPU preferred)";
    std::optional<Load> load_;
    std::optional<Job> pending_;
    std::shared_ptr<const Map> map_;
    std::shared_ptr<const Frame> latest_;
    std::string status_="no map loaded";
    void run();
};
}
