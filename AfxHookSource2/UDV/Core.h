#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace udv {
struct Vec3 {
    float x=0, y=0, z=0;
    float operator[](int i) const { return i == 0 ? x : i == 1 ? y : z; }
};
Vec3 operator+(Vec3 a, Vec3 b);
Vec3 operator-(Vec3 a, Vec3 b);
Vec3 operator*(Vec3 a, float s);
float dot(Vec3 a, Vec3 b);
Vec3 cross(Vec3 a, Vec3 b);
float length(Vec3 a);
bool finite(Vec3 a);
struct Triangle { Vec3 a,b,c; };
using Cancel = std::function<bool()>;
struct BvhNode { Vec3 lo,hi; uint32_t begin=0,count=0,left=0,right=0; };

class Geometry {
public:
    explicit Geometry(std::vector<Triangle> triangles,const Cancel& cancel={});
    static std::vector<Triangle> readTri(const std::string& path,const Cancel& cancel={});
    bool blocked(Vec3 from, Vec3 to) const;
    const std::vector<Triangle>& triangles() const { return triangles_; }
    const std::vector<BvhNode>& nodes() const { return nodes_; }
    const std::vector<uint32_t>& order() const { return order_; }
private:
    std::vector<Triangle> triangles_;
    std::vector<uint32_t> order_;
    std::vector<BvhNode> nodes_;
    uint32_t build(uint32_t begin, uint32_t end, const Cancel& cancel, unsigned depth=0);
};
enum : uint8_t { Standing=1, Crouching=2 };
struct Candidate { Vec3 feet; Vec3 normal; uint8_t stances=0; };
std::vector<Candidate> generateCandidates(const Geometry&, float spacing=32, const Cancel& cancel={});

struct Pose {
    Vec3 feet, eye;
    Vec3 angles; // Source pitch/yaw/roll, degrees.
    float horizontalFov=90, aspect=4.0f/3.0f;
    int tick=0;
    uint32_t target=0;
};
bool valid(const Pose&);
bool inFov(const Pose&, Vec3 point);
enum class Area : uint8_t { None, Vision, Gap };
struct Classification {
    uint8_t vision=0, gap=0; // Independent stance masks.
    Area area() const { return gap ? Area::Gap : vision ? Area::Vision : Area::None; }
};
Classification classify(const Geometry&, const Pose&, const Candidate&);
struct Result {
    Pose pose;
    uint64_t generation=0;
    std::vector<Classification> classes;
    double milliseconds=0;
    double gpuMilliseconds=-1; // CUDA event time, -1 when unavailable.
    size_t tested=0;
};
Result analyze(const Geometry&, const std::vector<Candidate>&, const Pose&, float range,
               const Cancel& cancel={});
}
