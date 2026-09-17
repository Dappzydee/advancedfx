#pragma once
#include "Worker.h"
#include <array>

namespace udv {
struct OverlayFrame {
    std::shared_ptr<const Frame> analysis;
    std::array<float,16> matrix{};
    std::array<float,4> vision{1,0,0,0.45f}, gap{1,0.65f,0,0.65f};
    uint64_t epoch=0;
};
// Engine thread only, except takeOverlay/currentEpoch (render thread).
void capturePose(const float* eye,const float* angles,float fov,int width,int height);
void captureMatrix(const float* matrix);
void endFrame();
void present();
bool takeOverlay(OverlayFrame&);
uint64_t currentEpoch();
void levelReset();
void shutdown();
void checkDemo();
}
