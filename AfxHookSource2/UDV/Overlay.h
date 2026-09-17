#pragma once
#include <d3d11.h>
namespace udv {
void drawOverlay(ID3D11DeviceContext*,const D3D11_VIEWPORT*,ID3D11RenderTargetView*,ID3D11DepthStencilView*);
void resetOverlayDevice();
double overlayCpuMilliseconds();
}
