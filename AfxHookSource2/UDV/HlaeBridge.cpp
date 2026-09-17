#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "HlaeBridge.h"
#include "../ClientEntitySystem.h"
#include "../SchemaSystem.h"
#include "../MirvTime.h"
#include "../WrpConsole.h"
#include "../../shared/FovScaling.h"
#include "../../deps/release/prop/cs2/sdk_src/public/cdll_int.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <limits>
#include <sstream>

extern SOURCESDK::CS2::ISource2EngineToClient* g_pEngineToClient;
extern CEntityInstance* (__fastcall* g_ClientDll_GetSplitScreenPlayer)(int);

namespace udv {
namespace {
// Never create/join a worker in DllMain (Windows loader lock).
std::unique_ptr<Worker> worker;
bool enabled=false, havePose=false;
int targetIndex=-1;
float explicitFov=0,range=2000;
Pose lastPose, submittedPose;
bool haveSubmitted=false;
std::string expectedMap;
std::array<float,4> vision{1,0,0,0.45f},gap{1,0.65f,0,0.65f};
std::chrono::steady_clock::time_point lastSubmit{};
std::atomic<uint64_t> epoch{0};
std::mutex packetsMutex;
std::deque<std::optional<OverlayFrame>> packets;

void clear(bool unload=false) {
    ++epoch; havePose=false; haveSubmitted=false;
    if(worker) worker->invalidate(unload);
    std::lock_guard<std::mutex> lock(packetsMutex); packets.clear();
}
Worker& getWorker() { if(!worker) worker=std::make_unique<Worker>(); return *worker; }
CEntityInstance* entity(int index) {
    return index>=0&&index<32768&&g_pEntityList&&*g_pEntityList&&g_GetEntityFromIndex
        ? static_cast<CEntityInstance*>(g_GetEntityFromIndex(*g_pEntityList,index)):nullptr;
}
CEntityInstance* resolve(SOURCESDK::CS2::CBaseHandle h) {
    if(!h.IsValid()) return nullptr;
    auto e=entity(h.GetEntryIndex());
    return e&&e->GetHandle()==h ? e:nullptr;
}
CEntityInstance* selected() {
    CEntityInstance* e=targetIndex>=0 ? entity(targetIndex):
        (g_ClientDll_GetSplitScreenPlayer ? g_ClientDll_GetSplitScreenPlayer(0):nullptr);
    if(e&&e->IsPlayerController()) e=resolve(e->GetPlayerPawnHandle());
    if(targetIndex<0&&e&&e->IsPlayerPawn()) {
        const auto mode=e->GetObserverMode();
        // Source 2 OBS_MODE_IN_EYE = 2; chase/free camera cannot supply player FOV.
        if(mode==2) e=resolve(e->GetObserverTarget());
        else if(mode!=0) return nullptr;
    }
    return e&&e->IsPlayerPawn()&&e->GetHealth()>0 ? e:nullptr;
}
bool same(const Pose& a,const Pose& b) {
    return a.tick==b.tick&&a.target==b.target&&length(a.eye-b.eye)<0.01f&&length(a.feet-b.feet)<0.01f&&
        length(a.angles-b.angles)<0.01f&&a.horizontalFov==b.horizontalFov&&a.aspect==b.aspect;
}
bool number(const char* value,float& out) {
    char* end=nullptr; out=std::strtof(value,&end);
    return end!=value&&*end=='\0'&&std::isfinite(out);
}
}
uint64_t currentEpoch() { return epoch.load(); }
void levelReset() { clear(true); expectedMap.clear(); }
void shutdown() { enabled=false; clear(true); worker.reset(); }
void checkDemo() {
    if(enabled&&(!g_pEngineToClient||!g_pEngineToClient->IsPlayingDemo())) {
        if(havePose) clear();
    }
}
void capturePose(const float* rawEye,const float* rawAngles,float fov,int width,int height) {
    if(!enabled) return;
    auto fail=[] { if(havePose) clear(); };
    if(!g_pEngineToClient||!g_pEngineToClient->IsPlayingDemo()||width<=0||height<=0) { fail(); return; }
    const char* mapName=g_pEngineToClient->GetLevelNameShort();
    if(!mapName||expectedMap!=mapName) { fail(); return; }
    // Existing helpers assume resolved schema and a live scene node. Guard these
    // preconditions here rather than adding alternate offsets/signatures.
    const auto& o=g_clientDllOffsets;
    if(!o.C_BaseEntity.m_pGameSceneNode||!o.CGameSceneNode.m_vecAbsOrigin||!o.C_BaseEntity.m_iHealth||
       !o.CEntityInstance.m_pEntity||!o.CBasePlayerController.m_hPawn||
       !o.C_BasePlayerPawn.m_pObserverServices||!o.CPlayer_ObserverServices.m_iObserverMode||
       !o.CPlayer_ObserverServices.m_hObserverTarget) { fail(); return; }
    auto e=selected();
    if(!e||!*(void**)(reinterpret_cast<unsigned char*>(e)+o.C_BaseEntity.m_pGameSceneNode)) { fail(); return; }
    Pose pose;
    if(!g_MirvTime.GetCurrentDemoTick(pose.tick)) { fail(); return; }
    e->GetOrigin(pose.feet.x,pose.feet.y,pose.feet.z);
    float eye[3], angles[3]; e->GetRenderEyeOrigin(eye); e->GetRenderEyeAngles(angles);
    pose.eye={eye[0],eye[1],eye[2]}; pose.angles={angles[0],angles[1],angles[2]};
    pose.target=static_cast<uint32_t>(e->GetHandle().ToInt()); pose.aspect=float(width)/height;
    if(targetIndex<0) {
        // Reject chase/freecam/death views, but allow small render interpolation differences.
        if(length(pose.eye-Vec3{rawEye[0],rawEye[1],rawEye[2]})>8) { fail(); return; }
        pose.eye={rawEye[0],rawEye[1],rawEye[2]};
        pose.angles={rawAngles[0],rawAngles[1],rawAngles[2]};
        pose.horizontalFov=static_cast<float>(Apply_FovScaling(width,height,fov,FovScaling_AlienSwarm));
    } else {
        if(explicitFov<=0) { fail(); return; }
        pose.horizontalFov=explicitFov; // Explicit target is not necessarily the current camera.
    }
    if(!valid(pose)) { fail(); return; }
    if(havePose&&(pose.target!=lastPose.target||pose.tick<lastPose.tick||pose.tick-lastPose.tick>16||
                 length(pose.feet-lastPose.feet)>128)) clear();
    lastPose=pose; havePose=true;
    const auto now=std::chrono::steady_clock::now();
    if(!worker||worker->busy()||!worker->map()||
       (haveSubmitted&&same(pose,submittedPose))||now-lastSubmit<std::chrono::milliseconds(100)) return;
    // Capture the current pose when compute becomes available, never enqueue a timeline.
    worker->submit(pose,range); submittedPose=pose; haveSubmitted=true; lastSubmit=now;
}
void captureMatrix(const float* matrix) {
    if(!enabled||!havePose||!worker) return;
    auto result=worker->latest();
    if(!result||result->result.pose.target!=lastPose.target||lastPose.tick<result->result.pose.tick||
       lastPose.tick-result->result.pose.tick>32) return;
    OverlayFrame packet; packet.analysis=std::move(result); packet.epoch=epoch;
    packet.vision=vision; packet.gap=gap;
    std::copy(matrix,matrix+16,packet.matrix.begin());
    std::lock_guard<std::mutex> lock(packetsMutex);
    if(packets.size()>=16) packets.clear();
    if(!packets.empty()&&packets.back()) packets.back()=std::move(packet);
    else packets.emplace_back(std::move(packet));
}
void endFrame() {
    if(!enabled) return;
    std::lock_guard<std::mutex> lock(packetsMutex);
    if(packets.size()>=16) packets.clear();
    packets.emplace_back(std::nullopt);
}
void present() {
    std::lock_guard<std::mutex> lock(packetsMutex);
    while(!packets.empty()) { bool boundary=!packets.front(); packets.pop_front(); if(boundary) break; }
}
bool takeOverlay(OverlayFrame& out) {
    std::unique_lock<std::mutex> lock(packetsMutex,std::try_to_lock);
    if(!lock||packets.empty()||!packets.front()) return false;
    out=std::move(*packets.front()); packets.pop_front();
    return out.epoch==epoch;
}
void command(advancedfx::ICommandArgs* args) {
    const int argc=args->ArgC();
    const std::string sub=argc>1?args->ArgV(1):"";
    float value=0;
    if(sub=="vision"&&argc==3&&number(args->ArgV(2),value)&&(value==0||value==1)) {
        clear(); enabled=value!=0; getWorker().enable(enabled); return;
    }
    if(sub=="load"&&(argc==4||argc==5)) {
        float spacing=64;
        if(argc==5&&(!number(args->ArgV(4),spacing)||spacing<16||spacing>128)) {
            advancedfx::Warning("UDV: spacing must be 16..128\n"); return;
        }
        clear(true); expectedMap=args->ArgV(2);
        getWorker().load(expectedMap,args->ArgV(3),spacing); return;
    }
    if(sub=="target"&&argc==3) {
        if(std::string(args->ArgV(2))=="auto") { targetIndex=-1; clear(); return; }
        if(number(args->ArgV(2),value)&&value>=0&&value<32768&&value==std::floor(value)) {
            targetIndex=static_cast<int>(value); clear();
            advancedfx::Message("UDV: explicit target requires 'mirv_udv fov <horizontal degrees>'\n"); return;
        }
    }
    if(sub=="fov"&&argc==3&&number(args->ArgV(2),value)&&value>1&&value<179) { explicitFov=value; clear(); return; }
    if(sub=="range"&&argc==3&&number(args->ArgV(2),value)&&value>=128&&value<=8000) { range=value; clear(); return; }
    if(sub=="color"&&argc==7) {
        std::array<float,4> color;
        bool ok=true; for(int i=0;i<4;++i) ok=number(args->ArgV(i+3),color[i])&&color[i]>=0&&color[i]<=1&&ok;
        const std::string which=args->ArgV(2);
        if(ok&&(which=="vision"||which=="gap")) { (which=="vision"?vision:gap)=color; return; }
    }
    if(sub=="status") {
        auto& w=getWorker(); auto m=w.map(); auto r=w.latest();
        advancedfx::Message("UDV: enabled=%d state=%s map=%s candidates=%zu busy=%d target=%d tick=%d FOV=%.2f range=%.0f analysis_ms=%.2f tested=%zu\n",
            enabled,w.status().c_str(),m?m->name.c_str():"none",m?m->candidates.size():0,w.busy(),targetIndex,
            havePose?lastPose.tick:-1,havePose?lastPose.horizontalFov:0,range,r?r->result.milliseconds:0,r?r->result.tested:0);
        return;
    }
    advancedfx::Message("mirv_udv load <map name> <quoted TRI path> [spacing 16..128, default 64]\n"
        "mirv_udv vision 0|1\nmirv_udv target auto|<pawn/controller entity index>\n"
        "mirv_udv fov <explicit-target horizontal degrees>\nmirv_udv range <128..8000>\n"
        "mirv_udv color vision|gap <r g b a in 0..1>\nmirv_udv status\n"
        "Experimental static geometry; reload after map changes. Automatic FOV requires first-person spectating.\n");
}
}
CON_COMMAND(mirv_udv,"Live static-geometry vision and reciprocal exposure gaps for demos.") { udv::command(args); }
