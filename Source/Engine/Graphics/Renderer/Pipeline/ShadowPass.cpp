#include "ShadowPass.h"
#include "Engine/Graphics/Renderer/ModelRenderer.h"
#include "Engine/Graphics/Shader/ShaderRegistry.h"
#include "Engine/Graphics/Core/Graphics.h"
#include "Engine/Graphics/Core/Camera.h"
#include "Engine/Graphics/Core/Light.h"
#include "Engine/Graphics/Shader/ShaderResources.h"
#include "Engine/Graphics/Renderer/RenderQueue.h"
#include "Engine/Graphics/Shader/Pass/ShadowUtils.h"
#include <algorithm>


void ShadowPass::Initialize(ID3D11Device* device) {
    _psoShadow = PipelineState(PipelineStateDesc::DefaultOpaque());
    _instanceBuffer.Create(device, 8192);
}

void ShadowPass::Execute(const RenderContext& rc) {

    ZoneScopedN("Pass: Shadow");

    RenderQueue* queue = rc.renderQueue;
    ModelRenderer* renderer = rc.modelRenderer;

    if (!queue || !renderer) return;

    const auto& shadowCommands = queue->GetShadowCommands();
    if (shadowCommands.empty()) return;

    ID3D11DeviceContext* dc = rc.deviceContext;
    auto& shadowMap = rc.shadowMap;

    // 現在のレンダーターゲットとビューポートを退避
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> oldRTV;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> oldDSV;
    dc->OMGetRenderTargets(1, oldRTV.GetAddressOf(), oldDSV.GetAddressOf());

    UINT numViewports = 1;
    D3D11_VIEWPORT oldViewport;
    dc->RSGetViewports(&numViewports, &oldViewport);

    for (int cascadeIndex = 0; cascadeIndex < 3; ++cascadeIndex)
    {
        shadowMap->Activate(dc, cascadeIndex);
        _psoShadow.Apply(dc, rc.renderState);

        DirectX::XMFLOAT3 lightDirection = rc.lightManager->GetDirectionalLight().direction;

        DirectX::XMMATRIX lightVPs[3];
        CalculateCascadeMatrices(
            XMLoadFloat4x4(&rc.camera->GetView()),
            rc.camera->GetFovY(),
            rc.camera->GetAspect(),
            lightDirection,
            shadowMap->params.cascadeSplits,
            lightVPs
        );

        ShadowMap::CbShadow cbData{};
        XMStoreFloat4x4(&cbData.lightViewProjection[0], lightVPs[0]);
        XMStoreFloat4x4(&cbData.lightViewProjection[1], lightVPs[1]);
        XMStoreFloat4x4(&cbData.lightViewProjection[2], lightVPs[2]);
        cbData.cascadeSplits = { shadowMap->params.cascadeSplits[0], shadowMap->params.cascadeSplits[1], shadowMap->params.cascadeSplits[2], 0.0f };
        cbData.shadowColor = shadowMap->params.shadowColor;
        cbData.shadowAttenuation = shadowMap->params.shadowAttenuation;
        cbData.shadowBias = shadowMap->params.shadowBias;
        cbData.currentCascadeIndex = cascadeIndex;

        shadowMap->UpdateConstantBuffer(dc, cbData);
        shadowMap->Bind(dc, -1, -1, SLOT_CB_SHADOW);

        Shader* shader = ShaderRegistry::Instance().GetShader("ShadowMap"_hash, Graphics::Instance().GetDevice());
        if (!shader) {
            shadowMap->Deactivate(dc);
            return;
        }

        shader->Begin(rc);
        dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        struct ShadowSortItem {
            const Model::Mesh* mesh;
            DirectX::XMFLOAT4X4  worldMatrix;  // ノード合成済み行列を格納
            uint8_t               cascadeMask;
        };

        std::vector<ShadowSortItem> sortedItems;
        sortedItems.reserve(shadowCommands.size());

        // 直前に設定したメッシュを記憶するポインタ
        const Model::Mesh* lastBoundMesh = nullptr;

        for (const auto& cmd : shadowCommands) {
            if (!cmd.mesh) continue;

            ShadowSortItem item;
            item.mesh = cmd.mesh;
            item.cascadeMask = cmd.cascadeMask;

            // =========================================================
            // ShadowCommand の worldMatrix はエンティティ行列のみなので、
            // OpaquePass と同様に mesh.node->worldTransform を合成する。
            // =========================================================
            bool isSkinned = !cmd.mesh->bones.empty();
            if (!isSkinned && cmd.mesh->node) {
                DirectX::XMMATRIX meshLocal = DirectX::XMLoadFloat4x4(&cmd.mesh->node->globalTransform);
                DirectX::XMMATRIX entityWorld = DirectX::XMLoadFloat4x4(&cmd.worldMatrix);
                DirectX::XMStoreFloat4x4(&item.worldMatrix, meshLocal * entityWorld);
            }
            else {
                item.worldMatrix = cmd.worldMatrix;
            }

            sortedItems.push_back(item);
        }

        std::sort(sortedItems.begin(), sortedItems.end(),
            [](const ShadowSortItem& a, const ShadowSortItem& b) {
                return a.mesh < b.mesh;
            });

        struct InstanceBatch {
            const Model::Mesh* mesh = nullptr;
            std::vector<InstanceData> data = {};
        };
        std::vector<InstanceBatch> batches;
        batches.reserve(16);

        for (const auto& info : sortedItems) {
            if (!info.mesh || !info.mesh->data) continue;
            if ((info.cascadeMask & (1 << cascadeIndex)) == 0) continue;

            bool isSkinned = !info.mesh->bones.empty();
            if (isSkinned) {
                // スキンドメッシュ：個別描画（インスタンシング不可）
                renderer->UpdateSkeletonConstants(dc, *info.mesh);

                // =========================================================
                // ★最適化1：前回と違うメッシュが来た時「だけ」ステートを更新する
                // =========================================================
                if (lastBoundMesh != info.mesh) {
                    UINT stride = sizeof(ModelResource::Vertex);
                    UINT offset = 0;
                    dc->IASetVertexBuffers(0, 1, info.mesh->data->vertexBuffer.GetAddressOf(), &stride, &offset);
                    dc->IASetIndexBuffer(info.mesh->data->indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
                    shader->Update(rc, *info.mesh);
                    lastBoundMesh = info.mesh;
                }

                dc->DrawIndexed((UINT)info.mesh->data->indices.size(), 0, 0);
            }
            else {
                // 静的メッシュ：ノード合成済みの worldMatrix でバッチ登録
                if (!batches.empty() && batches.back().mesh == info.mesh) {
                    batches.back().data.push_back({ info.worldMatrix });
                }
                else {
                    InstanceBatch newBatch;
                    newBatch.mesh = info.mesh;
                    newBatch.data.push_back({ info.worldMatrix });
                    batches.push_back(newBatch);
                }
            }
        }

        for (const auto& batch : batches) {
            shader->Update(rc, *batch.mesh);

            // =========================================================
            // ★最適化2：頂点バッファのセットを while ループの「外」に引き上げる
            // =========================================================
            UINT stride = sizeof(ModelResource::Vertex);
            UINT zero = 0;
            dc->IASetVertexBuffers(0, 1, batch.mesh->data->vertexBuffer.GetAddressOf(), &stride, &zero);
            dc->IASetIndexBuffer(batch.mesh->data->indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

            size_t totalCount = batch.data.size();
            size_t offset = 0;

            while (offset < totalCount) {
                size_t drawCount = (std::min)(totalCount - offset, InstanceBuffer::MAX_INSTANCES);

                // ポインタのオフセット計算だけで直接GPUに書き込む（光の速さ）
                _instanceBuffer.Write(dc, batch.data.data() + offset, drawCount);

                _instanceBuffer.Bind(dc, SLOT_SRV_INSTANCE);

                dc->DrawIndexedInstanced((UINT)batch.mesh->data->indices.size(), (UINT)drawCount, 0, 0, 0);
                offset += drawCount;
            }
        }

        shader->End(rc);
        _instanceBuffer.Unbind(dc, SLOT_SRV_INSTANCE);
    }

    // 退避しておいたメインキャンバスを復元
    dc->OMSetRenderTargets(1, oldRTV.GetAddressOf(), oldDSV.Get());
    dc->RSSetViewports(1, &oldViewport);
}