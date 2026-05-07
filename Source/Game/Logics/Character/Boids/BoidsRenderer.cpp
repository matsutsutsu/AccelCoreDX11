//#include "BoidsRenderer.h"
//#include "Engine/Graphics/Core/Graphics.h"
//#include "Engine/Graphics/Core/GpuResourceUtils.h"
//#include "Engine/Graphics/Shader/ShaderResources.h"
//
//void BoidsRenderer::Initialize(ID3D11Device* device) {
//    GpuResourceUtils::LoadVertexShader(device, "Assets/Shader/BoidsVS.cso", nullptr, 0, nullptr, vs.GetAddressOf());
//    GpuResourceUtils::LoadPixelShader(device, "Assets/Shader/BoidsPS.cso", ps.GetAddressOf());
//}
//
//void BoidsRenderer::BeginFrame() {
//    _drawCommands.clear();
//}
//
//void BoidsRenderer::Draw(BoidsSwarmComponent* swarm) {
//    if (swarm && swarm->isInitialized) {
//        _drawCommands.push_back(swarm);
//    }
//}
//
//void BoidsRenderer::Render(ID3D11DeviceContext* dc) {
//    if (_drawCommands.empty()) return;
//
//    dc->VSSetShader(vs.Get(), nullptr, 0);
//    dc->PSSetShader(ps.Get(), nullptr, 0);
//
//    // シーンのカメラ情報(CbScene)は事前にセットされている前提
//
//    for (auto* swarm : _drawCommands) {
//        if (!swarm->model || swarm->model->GetMeshes().empty()) continue;
//
//        // CSで計算した結果(SRV)をVSのt11にバインド
//        ID3D11ShaderResourceView* srvs[] = { swarm->computeSRV.Get() };
//        dc->VSSetShaderResources(11, 1, srvs);
//
//        auto& mesh = swarm->model->GetMeshes()[0];
//        UINT stride = sizeof(ModelResource::Vertex);
//        UINT offset = 0;
//        dc->IASetVertexBuffers(0, 1, mesh.data->vertexBuffer.GetAddressOf(), &stride, &offset);
//        dc->IASetIndexBuffer(mesh.data->indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
//        dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
//
//        // インスタンシング描画（CPUはループを回さない！）
//        dc->DrawIndexedInstanced(mesh.data->indexCount, swarm->params.boidsCount, 0, 0, 0);
//    }
//
//    // 跡片付け
//    ID3D11ShaderResourceView* nullSrv[] = { nullptr };
//    dc->VSSetShaderResources(11, 1, nullSrv);
//}