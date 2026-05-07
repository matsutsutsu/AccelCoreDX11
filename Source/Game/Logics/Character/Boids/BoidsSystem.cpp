//#include "BoidsSystem.h"
//#include "Engine/Graphics/Core/Graphics.h"
//#include "Engine/Graphics/Core/GpuResourceUtils.h"
//#include "ECS/System/CCL_SystemRegistry.h"
//#include "Game/Core/SystemPriority.h"
//
//// --- Update System ---
//void BoidsUpdateSystem::Initialize(ID3D11Device* device) {
//    GpuResourceUtils::LoadComputeShader(device, "Assets/Shader/BoidsUpdateCS.cso", _cs.GetAddressOf());
//}
//
//void BoidsUpdateSystem::Update(float dt) {
//    auto* dc = Graphics::Instance().GetDeviceContext();
//
//    ForEach([&](BoidsSwarmComponent& swarm) {
//        if (!swarm.isInitialized) return;
//
//        swarm.params.deltaTime = dt;
//
//        // 定数バッファの更新
//        D3D11_MAPPED_SUBRESOURCE mapped;
//        dc->Map(swarm.constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
//        memcpy(mapped.pData, &swarm.params, sizeof(CbBoidsParams));
//        dc->Unmap(swarm.constantBuffer.Get(), 0);
//
//        // Compute Shader 実行
//        dc->CSSetShader(_cs.Get(), nullptr, 0);
//        ID3D11Buffer* cbs[] = { swarm.constantBuffer.Get() };
//        dc->CSSetConstantBuffers(10, 1, cbs);
//        dc->CSSetUnorderedAccessViews(0, 1, swarm.computeUAV.GetAddressOf(), nullptr);
//
//        int threadGroups = (swarm.params.boidsCount + 255) / 256;
//        dc->Dispatch(threadGroups, 1, 1);
//
//        // 解除 (RenderでSRVとして使うため必須)
//        ID3D11UnorderedAccessView* nullUAV = nullptr;
//        dc->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
//        });
//}
//
//// --- Render System ---
//void BoidsRenderSystem::Update(float dt) {
//    // ※Graphicsクラスに GetBoidsRenderer() を追加しておく必要があります
//    auto* renderer = Graphics::Instance().GetBoidsRenderer();
//    if (!renderer) return;
//
//    ForEach([&](BoidsSwarmComponent& swarm) {
//        renderer->Draw(&swarm); // 引換券を渡すだけ
//        });
//}
//
//REGISTER_RENDER_SYSTEM(BoidsUpdateSystem, Priority::RenderStage::PreMain);
//REGISTER_RENDER_SYSTEM(BoidsRenderSystem, Priority::RenderStage::MainDraw);