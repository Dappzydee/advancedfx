#include "Core.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstring>
#include <fstream>
#include <limits>
#include <numeric>
#include <set>
#include <stdexcept>
#include <tuple>

namespace udv {
Vec3 operator+(Vec3 a, Vec3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vec3 operator-(Vec3 a, Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vec3 operator*(Vec3 a, float s) { return {a.x*s,a.y*s,a.z*s}; }
float dot(Vec3 a, Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
Vec3 cross(Vec3 a, Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
float length(Vec3 a) { return std::sqrt(dot(a,a)); }
bool finite(Vec3 a) { return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z); }
static Vec3 minimum(Vec3 a,Vec3 b) { return {std::min(a.x,b.x),std::min(a.y,b.y),std::min(a.z,b.z)}; }
static Vec3 maximum(Vec3 a,Vec3 b) { return {std::max(a.x,b.x),std::max(a.y,b.y),std::max(a.z,b.z)}; }

std::vector<Triangle> Geometry::readTri(const std::string& path,const Cancel& cancel) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file) throw std::runtime_error("Cannot open TRI file");
    const auto bytes=file.tellg();
    if(bytes<=0 || bytes%36 || bytes>360000000) throw std::runtime_error("Invalid TRI size (limit 10M triangles)");
    file.seekg(0);
    std::vector<Triangle> out;
    out.reserve(static_cast<size_t>(bytes)/36);
    // Awpy TRI on supported Windows/Linux machines is headerless little-endian float32.
    for(std::streamoff i=0;i<bytes;i+=36) {
        if(i%(4096*36)==0&&cancel&&cancel()) throw std::runtime_error("Map load cancelled");
        unsigned char raw[36]; float v[9];
        if(!file.read(reinterpret_cast<char*>(raw),36)) throw std::runtime_error("Truncated TRI");
        for(int j=0;j<9;++j) {
            const auto p=raw+j*4;
            uint32_t word=uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);
            std::memcpy(v+j,&word,4);
            if(!std::isfinite(v[j]) || std::abs(v[j])>1000000) throw std::runtime_error("Invalid TRI coordinate");
        }
        out.push_back({{v[0],v[1],v[2]},{v[3],v[4],v[5]},{v[6],v[7],v[8]}});
    }
    return out;
}
Geometry::Geometry(std::vector<Triangle> triangles,const Cancel& cancel):triangles_(std::move(triangles)) {
    if(triangles_.size()>10000000) throw std::runtime_error("Too many triangles");
    for(const auto& t:triangles_) if(!finite(t.a)||!finite(t.b)||!finite(t.c)) throw std::runtime_error("Nonfinite geometry");
    order_.resize(triangles_.size());
    std::iota(order_.begin(),order_.end(),0u);
    if(!order_.empty()) build(0,static_cast<uint32_t>(order_.size()),cancel);
}
uint32_t Geometry::build(uint32_t begin,uint32_t end,const Cancel& cancel,unsigned depth) {
    if(cancel&&cancel()) throw std::runtime_error("Map build cancelled");
    BvhNode n;
    n.lo={1e30f,1e30f,1e30f}; n.hi={-1e30f,-1e30f,-1e30f};
    for(auto i=begin;i<end;++i) {
        const auto& t=triangles_[order_[i]];
        n.lo=minimum(n.lo,minimum(t.a,minimum(t.b,t.c)));
        n.hi=maximum(n.hi,maximum(t.a,maximum(t.b,t.c)));
    }
    auto id=static_cast<uint32_t>(nodes_.size()); nodes_.push_back(n);
    if(end-begin<=8||depth>=48) { nodes_[id].begin=begin; nodes_[id].count=end-begin; }
    else {
        // Binned surface-area heuristic avoids large overlapping children on
        // map meshes with long walls and uneven triangle density.
        Vec3 centLo{1e30f,1e30f,1e30f},centHi{-1e30f,-1e30f,-1e30f};
        auto centroid=[&](uint32_t index) {
            const auto& t=triangles_[index]; return (t.a+t.b+t.c)*(1.0f/3);
        };
        for(auto i=begin;i<end;++i) { auto c=centroid(order_[i]); centLo=minimum(centLo,c); centHi=maximum(centHi,c); }
        Vec3 extent=centHi-centLo;
        int axis=extent.y>extent.x ? 1:0; if(extent.z>extent[axis]) axis=2;
        auto mid=begin+(end-begin)/2;
        struct Bin { Vec3 lo{1e30f,1e30f,1e30f},hi{-1e30f,-1e30f,-1e30f}; uint32_t count=0; };
        auto merge=[](Bin a,const Bin& b) { if(b.count) { a.lo=minimum(a.lo,b.lo); a.hi=maximum(a.hi,b.hi); a.count+=b.count; } return a; };
        auto cost=[](const Bin& b) { auto e=b.hi-b.lo; return b.count ? double(e.x*e.y+e.y*e.z+e.z*e.x)*b.count:0; };
        double best=std::numeric_limits<double>::infinity(); int bestAxis=-1,bestSplit=0;
        for(int a=0;a<3;++a) {
            if(extent[a]<1e-5f) continue;
            std::array<Bin,16> bins{},left{},right{};
            for(auto i=begin;i<end;++i) {
                const auto index=order_[i];
                int b=std::clamp(static_cast<int>((centroid(index)[a]-centLo[a])*16/extent[a]),0,15);
                const auto& t=triangles_[index]; auto& bin=bins[b]; ++bin.count;
                bin.lo=minimum(bin.lo,minimum(t.a,minimum(t.b,t.c)));
                bin.hi=maximum(bin.hi,maximum(t.a,maximum(t.b,t.c)));
            }
            left[0]=bins[0]; right[15]=bins[15];
            for(int b=1;b<16;++b) left[b]=merge(left[b-1],bins[b]);
            for(int b=14;b>=0;--b) right[b]=merge(right[b+1],bins[b]);
            for(int b=0;b<15;++b) {
                const double c=cost(left[b])+cost(right[b+1]);
                // Bound recursion depth even with adversarial geometry.
                if(left[b].count>=(end-begin)/16&&right[b+1].count>=(end-begin)/16&&
                   left[b].count&&right[b+1].count&&c<best) { best=c; bestAxis=a; bestSplit=b; }
            }
        }
        if(bestAxis>=0) {
            auto it=std::partition(order_.begin()+begin,order_.begin()+end,[&](uint32_t i) {
                int bin=std::clamp(static_cast<int>((centroid(i)[bestAxis]-centLo[bestAxis])*16/extent[bestAxis]),0,15);
                return bin<=bestSplit;
            });
            mid=static_cast<uint32_t>(it-order_.begin());
        } else {
            std::nth_element(order_.begin()+begin,order_.begin()+mid,order_.begin()+end,[&](uint32_t a,uint32_t b) {
                return centroid(a)[axis]<centroid(b)[axis];
            });
        }
        const auto left=build(begin,mid,cancel,depth+1), right=build(mid,end,cancel,depth+1);
        nodes_[id].left=left; nodes_[id].right=right;
    }
    return id;
}
static bool hitBox(Vec3 lo,Vec3 hi,Vec3 o,Vec3 d,float low,float high) {
    // Conservative bounds keep grazing hits independent of BVH partition order.
    lo=lo-Vec3{0.01f,0.01f,0.01f}; hi=hi+Vec3{0.01f,0.01f,0.01f};
    for(int i=0;i<3;++i) {
        if(std::abs(d[i])<1e-12f) { if(o[i]<lo[i]||o[i]>hi[i]) return false; }
        else {
            float a=(lo[i]-o[i])/d[i], b=(hi[i]-o[i])/d[i];
            if(a>b) std::swap(a,b);
            low=std::max(low,a); high=std::min(high,b);
            if(low>high) return false;
        }
    }
    return true;
}
static bool hitTriangle(const Triangle& t,Vec3 o,Vec3 d,float low,float high) {
    const Vec3 e=t.b-t.a, f=t.c-t.a, p=cross(d,f), s=o-t.a;
    const double det=dot(e,p);
    if(std::abs(det)<1e-9) return false;
    const double u=dot(s,p)/det;
    if(u<0||u>1) return false;
    const Vec3 q=cross(s,e);
    const double v=dot(d,q)/det;
    if(v<0||u+v>1) return false;
    const double distance=dot(f,q)/det;
    return distance>=low && distance<=high;
}
bool Geometry::blocked(Vec3 from,Vec3 to) const {
    if(!finite(from)||!finite(to)) return true;
    auto d=to-from; const float len=length(d);
    if(len<0.02f || nodes_.empty()) return false;
    d=d*(1.0f/len);
    // 0.01 world units at each endpoint prevents self-intersections, not cover bypass.
    std::array<uint32_t,64> stack{}; size_t count=1;
    while(count) {
        const auto& n=nodes_[stack[--count]];
        if(!hitBox(n.lo,n.hi,from,d,0.01f,len-0.01f)) continue;
        if(n.count) {
            for(uint32_t i=n.begin;i<n.begin+n.count;++i)
                if(hitTriangle(triangles_[order_[i]],from,d,0.01f,len-0.01f)) return true;
        } else { stack[count++]=n.left; stack[count++]=n.right; }
    }
    return false;
}
std::vector<Candidate> generateCandidates(const Geometry& g,float spacing,const Cancel& cancel) {
    if(!std::isfinite(spacing)||spacing<16||spacing>128) throw std::runtime_error("Spacing must be 16..128");
    std::vector<Candidate> out;
    std::set<std::tuple<int,int,int>> seen;
    size_t probes=0;
    for(const auto& t:g.triangles()) {
        if(cancel&&cancel()) return {};
        Vec3 n=cross(t.b-t.a,t.c-t.a); float len=length(n);
        if(len<1e-6f||std::abs(n.z)/len<0.707107f) continue;
        n=n*(1/len); if(n.z<0) n=n*(-1);
        auto lo=minimum(t.a,minimum(t.b,t.c)), hi=maximum(t.a,maximum(t.b,t.c));
        const int x0=static_cast<int>(std::ceil(lo.x/spacing)), x1=static_cast<int>(std::floor(hi.x/spacing));
        const int y0=static_cast<int>(std::ceil(lo.y/spacing)), y1=static_cast<int>(std::floor(hi.y/spacing));
        for(int x=x0;x<=x1;++x) for(int y=y0;y<=y1;++y) {
            if(++probes>20000000) throw std::runtime_error("Candidate generation budget exceeded; increase spacing");
            if((probes%256)==0&&cancel&&cancel()) return {};
            Vec3 p={x*spacing,y*spacing,0}; p.z=t.a.z-(n.x*(p.x-t.a.x)+n.y*(p.y-t.a.y))/n.z;
            const float a=dot(cross(t.b-t.a,p-t.a),n), b=dot(cross(t.c-t.b,p-t.b),n), c=dot(cross(t.a-t.c,p-t.c),n);
            if(!((a>=-0.01f&&b>=-0.01f&&c>=-0.01f)||(a<=0.01f&&b<=0.01f&&c<=0.01f))) continue;
            if(!seen.emplace(x,y,static_cast<int>(std::round(p.z/2))).second) continue;
            uint8_t stances=0;
            for(int stance=0;stance<2;++stance) {
                const float height=stance ? 54.0f:72.0f;
                bool clear=true;
                for(Vec3 offset: {Vec3{0,0,0},Vec3{12,12,0},Vec3{-12,12,0},Vec3{12,-12,0},Vec3{-12,-12,0}}) {
                    Vec3 base=p+offset;
                    base.z=p.z-(n.x*offset.x+n.y*offset.y)/n.z;
                    if(!g.blocked(base+Vec3{0,0,2},base-Vec3{0,0,4}) ||
                       g.blocked(base+Vec3{0,0,2},base+Vec3{0,0,height}) ||
                       g.blocked(p+Vec3{0,0,36},base+Vec3{0,0,36})) { clear=false; break; }
                }
                if(clear) stances|=stance ? Crouching:Standing;
            }
            if(stances) out.push_back({p,n,stances});
            if(out.size()>500000) throw std::runtime_error("Too many candidates; increase spacing");
        }
    }
    return out;
}
bool valid(const Pose& p) {
    return finite(p.feet)&&finite(p.eye)&&finite(p.angles)&&std::isfinite(p.horizontalFov)&&
        p.horizontalFov>1&&p.horizontalFov<179&&std::isfinite(p.aspect)&&p.aspect>0.1f&&p.aspect<10;
}
bool inFov(const Pose& p,Vec3 point) {
    if(!valid(p)||!finite(point)) return false;
    constexpr float rad=0.0174532925199433f;
    float pitch=p.angles.x*rad,yaw=p.angles.y*rad,roll=p.angles.z*rad;
    Vec3 forward={std::cos(pitch)*std::cos(yaw),std::cos(pitch)*std::sin(yaw),-std::sin(pitch)};
    Vec3 right={std::sin(yaw),-std::cos(yaw),0};
    Vec3 up=cross(right,forward);
    Vec3 rolledRight=right*std::cos(roll)-up*std::sin(roll);
    Vec3 rolledUp=up*std::cos(roll)+right*std::sin(roll);
    auto d=point-p.eye; float z=dot(d,forward);
    const float tangent=std::tan(p.horizontalFov*rad*0.5f);
    return z>0 && std::abs(dot(d,rolledRight))<=z*tangent && std::abs(dot(d,rolledUp))<=z*tangent/p.aspect;
}
static std::array<Vec3,5> body(Vec3 feet,float eyeHeight) {
    // Head, chest, pelvis and lateral chest samples. No bones or weapon geometry.
    return {feet+Vec3{0,0,eyeHeight+2}, feet+Vec3{0,0,eyeHeight*0.78f},
        feet+Vec3{0,0,eyeHeight*0.5f},feet+Vec3{8,0,eyeHeight*0.78f},feet+Vec3{-8,0,eyeHeight*0.78f}};
}
Classification classify(const Geometry& g,const Pose& p,const Candidate& c) {
    Classification out;
    if(!valid(p)) return out;
    const auto myBody=body(p.feet,std::clamp(p.eye.z-p.feet.z,28.0f,72.0f));
    for(int stance=0;stance<2;++stance) {
        const uint8_t bit=stance ? Crouching:Standing;
        if(!(c.stances&bit)) continue;
        const float height=stance ? 46.0f:64.0f;
        bool vision=false, exposed=false;
        for(Vec3 b:body(c.feet,height)) if(inFov(p,b)&&!g.blocked(p.eye,b)) { vision=true; break; }
        if(vision) out.vision|=bit;
        else {
            for(Vec3 a:myBody) if(!g.blocked(c.feet+Vec3{0,0,height},a)) { exposed=true; break; }
            if(exposed) out.gap|=bit;
        }
    }
    return out;
}
Result analyze(const Geometry& g,const std::vector<Candidate>& candidates,const Pose& p,float range,const Cancel& cancel) {
    const auto start=std::chrono::steady_clock::now();
    Result out; out.pose=p; out.classes.resize(candidates.size());
    if(!valid(p)||!std::isfinite(range)||range<=0) return out;
    for(size_t i=0;i<candidates.size();++i) {
        if((i%64)==0&&cancel&&cancel()) { out.classes.clear(); return out; }
        const auto d=candidates[i].feet-p.feet;
        if(dot(d,d)>range*range || dot(d,d)<32*32) continue;
        ++out.tested; out.classes[i]=classify(g,p,candidates[i]);
    }
    out.milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    return out;
}
}
