#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Overlay.h"
#include "HlaeBridge.h"
#include "../../shared/AfxConsole.h"
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <chrono>
#include <cstring>

namespace udv {
using Microsoft::WRL::ComPtr;
namespace {
const char shader[]=R"(
cbuffer Camera : register(b0) { float4x4 viewProjection; float4 options; };
struct Input { float3 feet:POSITION; float3 normal:NORMAL; float4 color:COLOR; };
struct Output { float4 position:SV_POSITION; float4 color:COLOR; };
Output vs(Input i,uint vertex:SV_VertexID) {
    const float2 corners[6]={float2(-1,-1),float2(1,-1),float2(1,1),float2(-1,-1),float2(1,1),float2(-1,1)};
    float2 offset=corners[vertex]*options.x;
    float3 p=i.feet+float3(offset,0);
    p.z+=0.75-dot(i.normal.xy,offset)/max(i.normal.z,0.01);
    Output o;
    // Same column-major HLSL / row-vector convention as HLAE CampathDrawer.
    o.position=mul(float4(p,1),viewProjection);
    o.color=i.color;
    return o;
}
float4 ps(Output i):SV_TARGET { return i.color; }
)";
struct Instance { Vec3 feet,normal; std::array<float,4> color; };
struct Constants { std::array<float,16> matrix; std::array<float,4> options; };
struct Renderer {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> deferred;
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
    ComPtr<ID3D11InputLayout> layout;
    ComPtr<ID3D11Buffer> instances,constants;
    ComPtr<ID3D11RasterizerState> raster;
    ComPtr<ID3D11BlendState> blend;
    ComPtr<ID3D11DepthStencilState> depth;
    std::shared_ptr<const Frame> cached;
    std::array<float,4> vision{},gap{};
    UINT count=0,capacity=0;
    bool failed=false;

    bool initialize(ID3D11Device* d) {
        device=d;
        ComPtr<ID3DBlob> vertex,pixel,errors;
        if(FAILED(D3DCompile(shader,sizeof(shader)-1,"UDV overlay",nullptr,nullptr,"vs","vs_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&vertex,&errors)) ||
           FAILED(D3DCompile(shader,sizeof(shader)-1,"UDV overlay",nullptr,nullptr,"ps","ps_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&pixel,&errors))) return false;
        if(FAILED(d->CreateDeferredContext(0,&deferred))||
           FAILED(d->CreateVertexShader(vertex->GetBufferPointer(),vertex->GetBufferSize(),nullptr,&vs))||
           FAILED(d->CreatePixelShader(pixel->GetBufferPointer(),pixel->GetBufferSize(),nullptr,&ps))) return false;
        D3D11_INPUT_ELEMENT_DESC elements[]={
            {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,24,D3D11_INPUT_PER_INSTANCE_DATA,1}};
        if(FAILED(d->CreateInputLayout(elements,3,vertex->GetBufferPointer(),vertex->GetBufferSize(),&layout))) return false;
        D3D11_BUFFER_DESC buffer{};
        buffer.ByteWidth=sizeof(Constants); buffer.Usage=D3D11_USAGE_DYNAMIC;
        buffer.BindFlags=D3D11_BIND_CONSTANT_BUFFER; buffer.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
        if(FAILED(d->CreateBuffer(&buffer,nullptr,&constants))) return false;
        D3D11_RASTERIZER_DESC r{};
        r.FillMode=D3D11_FILL_SOLID; r.CullMode=D3D11_CULL_NONE; r.DepthClipEnable=TRUE; r.MultisampleEnable=TRUE;
        if(FAILED(d->CreateRasterizerState(&r,&raster))) return false;
        D3D11_BLEND_DESC b{};
        auto& rt=b.RenderTarget[0]; rt.BlendEnable=TRUE;
        rt.SrcBlend=D3D11_BLEND_SRC_ALPHA; rt.DestBlend=D3D11_BLEND_INV_SRC_ALPHA; rt.BlendOp=D3D11_BLEND_OP_ADD;
        rt.SrcBlendAlpha=D3D11_BLEND_ONE; rt.DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA; rt.BlendOpAlpha=D3D11_BLEND_OP_ADD;
        rt.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
        if(FAILED(d->CreateBlendState(&b,&blend))) return false;
        // Mirrors CampathDrawer's depth convention. Never write the game's depth.
        D3D11_DEPTH_STENCIL_DESC z{};
        z.DepthEnable=TRUE; z.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO; z.DepthFunc=D3D11_COMPARISON_LESS_EQUAL;
        return SUCCEEDED(d->CreateDepthStencilState(&z,&depth));
    }
    bool upload(const OverlayFrame& frame) {
        if(cached==frame.analysis&&vision==frame.vision&&gap==frame.gap) return true;
        const auto& candidates=frame.analysis->map->candidates;
        const auto& classes=frame.analysis->result.classes;
        if(candidates.size()!=classes.size()) return false;
        std::vector<Instance> data;
        data.reserve(candidates.size());
        for(size_t i=0;i<candidates.size();++i) {
            const auto area=classes[i].area();
            if(area==Area::None) continue;
            data.push_back({candidates[i].feet,candidates[i].normal,area==Area::Gap?frame.gap:frame.vision});
        }
        if(data.size()>capacity) {
            instances.Reset();
            D3D11_BUFFER_DESC desc{};
            desc.ByteWidth=static_cast<UINT>(data.size()*sizeof(Instance));
            desc.Usage=D3D11_USAGE_DYNAMIC; desc.BindFlags=D3D11_BIND_VERTEX_BUFFER;
            desc.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
            if(FAILED(device->CreateBuffer(&desc,nullptr,&instances))) { capacity=0; return false; }
            capacity=static_cast<UINT>(data.size());
        }
        if(!data.empty()) {
            D3D11_MAPPED_SUBRESOURCE mapped{};
            if(FAILED(deferred->Map(instances.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped))) return false;
            std::memcpy(mapped.pData,data.data(),data.size()*sizeof(Instance));
            deferred->Unmap(instances.Get(),0);
        }
        count=static_cast<UINT>(data.size());
        cached=frame.analysis; vision=frame.vision; gap=frame.gap;
        return true;
    }
};
Renderer renderer;
std::atomic<double> cpuMs{0};
}
double overlayCpuMilliseconds() { return cpuMs.load(); }
void resetOverlayDevice() { renderer=Renderer{}; }
void drawOverlay(ID3D11DeviceContext* immediate,const D3D11_VIEWPORT* viewport,ID3D11RenderTargetView* target,ID3D11DepthStencilView* depth) {
    OverlayFrame frame;
    if(!takeOverlay(frame)||!immediate||!viewport||!target||!depth) return;
    const auto start=std::chrono::steady_clock::now();
    auto& r=renderer;
    ComPtr<ID3D11Device> device; immediate->GetDevice(&device);
    if(r.device.Get()!=device.Get()) resetOverlayDevice();
    if(r.failed) return;
    if(!r.device&&!r.initialize(device.Get())) {
        r.failed=true; advancedfx::Warning("UDV: D3D11 initialization failed; overlay suppressed\n"); return;
    }
    if(!r.upload(frame)) { r.cached.reset(); return; }
    if(!r.count) return;
    Constants values{frame.matrix,{frame.analysis->map->spacing*0.35f,0,0,0}};
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if(FAILED(r.deferred->Map(r.constants.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped))) return;
    std::memcpy(mapped.pData,&values,sizeof(values)); r.deferred->Unmap(r.constants.Get(),0);
    auto context=r.deferred.Get();
    context->OMSetRenderTargets(1,&target,depth);
    context->OMSetBlendState(r.blend.Get(),nullptr,0xffffffff);
    context->OMSetDepthStencilState(r.depth.Get(),0);
    context->RSSetState(r.raster.Get()); context->RSSetViewports(1,viewport);
    context->IASetInputLayout(r.layout.Get()); context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    UINT stride=sizeof(Instance),offset=0;
    ID3D11Buffer* buffer=r.instances.Get(); context->IASetVertexBuffers(0,1,&buffer,&stride,&offset);
    buffer=r.constants.Get(); context->VSSetConstantBuffers(0,1,&buffer);
    context->VSSetShader(r.vs.Get(),nullptr,0); context->PSSetShader(r.ps.Get(),nullptr,0);
    context->GSSetShader(nullptr,nullptr,0); context->HSSetShader(nullptr,nullptr,0); context->DSSetShader(nullptr,nullptr,0);
    context->DrawInstanced(6,r.count,0,0);
    ComPtr<ID3D11CommandList> commands;
    if(SUCCEEDED(context->FinishCommandList(FALSE,&commands))&&frame.epoch==currentEpoch())
        immediate->ExecuteCommandList(commands.Get(),TRUE);
    cpuMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
}
}
