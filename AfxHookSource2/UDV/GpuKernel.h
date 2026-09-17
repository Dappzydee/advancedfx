#pragma once
#include "Core.h"
#include <cmath>

#ifdef __CUDACC__
#define UDV_HD __host__ __device__
#else
#define UDV_HD
#endif

// Plain-data kernel implementation also compiled by portable parity tests.
// No allocation, STL containers, engine pointers or CUDA runtime calls here.
namespace udv::gpu {
struct Scene {
    const Triangle* triangles;
    const BvhNode* nodes;
    const uint32_t* order;
    uint32_t nodeCount;
};
struct View {
    Vec3 feet,eye,forward,right,up;
    float tangent,aspect,bodyHeight,rangeSquared;
};
UDV_HD inline Vec3 add(Vec3 a,Vec3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
UDV_HD inline Vec3 sub(Vec3 a,Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
UDV_HD inline Vec3 scale(Vec3 a,float b) { return {a.x*b,a.y*b,a.z*b}; }
UDV_HD inline float dp(Vec3 a,Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
UDV_HD inline Vec3 cp(Vec3 a,Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
UDV_HD inline float axis(Vec3 a,int i) { return i==0?a.x:i==1?a.y:a.z; }
UDV_HD inline bool box(const BvhNode& n,Vec3 o,Vec3 d,float high) {
    float low=0.01f;
    for(int i=0;i<3;++i) {
        const float direction=axis(d,i),origin=axis(o,i),lo=axis(n.lo,i)-0.01f,hi=axis(n.hi,i)+0.01f;
        if(fabsf(direction)<1e-12f) { if(origin<lo||origin>hi) return false; }
        else {
            float a=(lo-origin)/direction,b=(hi-origin)/direction;
            low=fmaxf(low,fminf(a,b)); high=fminf(high,fmaxf(a,b));
            if(low>high) return false;
        }
    }
    return true;
}
UDV_HD inline bool triangle(const Triangle& t,Vec3 o,Vec3 d,float high) {
    const Vec3 e=sub(t.b,t.a),f=sub(t.c,t.a),p=cp(d,f),s=sub(o,t.a);
    const double determinant=dp(e,p);
    if(fabs(determinant)<1e-9) return false;
    const double u=dp(s,p)/determinant;
    if(u<0||u>1) return false;
    const Vec3 q=cp(s,e);
    const double v=dp(d,q)/determinant;
    if(v<0||u+v>1) return false;
    const double distance=dp(f,q)/determinant;
    return distance>=0.01&&distance<=high;
}
UDV_HD inline bool blocked(Scene scene,Vec3 from,Vec3 to) {
    Vec3 d=sub(to,from); const float len=sqrtf(dp(d,d));
    if(len<0.02f||!scene.nodeCount) return false;
    d=scale(d,1.0f/len);
    uint32_t stack[64]; unsigned count=1; stack[0]=0;
    while(count) {
        const auto& n=scene.nodes[stack[--count]];
        if(!box(n,from,d,len-0.01f)) continue;
        if(n.count) {
            for(uint32_t i=n.begin;i<n.begin+n.count;++i)
                if(triangle(scene.triangles[scene.order[i]],from,d,len-0.01f)) return true;
        } else {
            // CPU builder enforces depth <=48. Fail closed on an invalid tree.
            if(count+2>64) return true;
            stack[count++]=n.left; stack[count++]=n.right;
        }
    }
    return false;
}
UDV_HD inline Vec3 sample(Vec3 feet,float height,int index) {
    return add(feet,{index==3?8.0f:index==4?-8.0f:0.0f,0,
        index==0?height+2:index==2?height*0.5f:height*0.78f});
}
UDV_HD inline bool fov(const View& p,Vec3 point) {
    auto d=sub(point,p.eye); float z=dp(d,p.forward);
    return z>0&&fabsf(dp(d,p.right))<=z*p.tangent&&fabsf(dp(d,p.up))<=z*p.tangent/p.aspect;
}
UDV_HD inline Classification classify(Scene scene,const View& p,const Candidate& c) {
    Classification out{};
    const auto distance=sub(c.feet,p.feet); const float squared=dp(distance,distance);
    if(squared>p.rangeSquared||squared<32*32) return out;
    for(int stance=0;stance<2;++stance) {
        const uint8_t bit=stance?Crouching:Standing;
        if(!(c.stances&bit)) continue;
        const float height=stance?46.0f:64.0f;
        bool visible=false;
        for(int i=0;i<5;++i) {
            Vec3 b=sample(c.feet,height,i);
            if(fov(p,b)&&!blocked(scene,p.eye,b)) { visible=true; break; }
        }
        if(visible) out.vision|=bit;
        else for(int i=0;i<5;++i) {
            if(!blocked(scene,add(c.feet,{0,0,height}),sample(p.feet,p.bodyHeight,i))) { out.gap|=bit; break; }
        }
    }
    return out;
}
inline View prepare(const Pose& p,float range) {
    constexpr float rad=0.0174532925199433f;
    const float pitch=p.angles.x*rad,yaw=p.angles.y*rad,roll=p.angles.z*rad;
    Vec3 forward{cosf(pitch)*cosf(yaw),cosf(pitch)*sinf(yaw),-sinf(pitch)};
    Vec3 right{sinf(yaw),-cosf(yaw),0},up=cp(right,forward);
    return {p.feet,p.eye,forward,sub(scale(right,cosf(roll)),scale(up,sinf(roll))),
        add(scale(up,cosf(roll)),scale(right,sinf(roll))),tanf(p.horizontalFov*rad*0.5f),p.aspect,
        fminf(72,fmaxf(28,p.eye.z-p.feet.z)),range*range};
}
}
#undef UDV_HD
