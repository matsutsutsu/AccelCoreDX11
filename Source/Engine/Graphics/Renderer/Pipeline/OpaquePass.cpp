#include "OpaquePass.h"
#include "Engine/Graphics/Renderer/ModelRenderer.h"
#include "Engine/Graphics/Shader/ShaderRegistry.h"
#include "Engine/Graphics/Core/Graphics.h"
#include "Engine/Graphics/Core/Camera.h" 
#include "Engine/Graphics/Renderer/RenderQueue.h"
#include "Engine/Graphics/Shader/ShaderResources.h"
#include <algorithm>

void OpaquePass::Initialize(ID3D11Device* device) {
    _psoOpaque = PipelineState(PipelineStateDesc::DefaultOpaque());
    _instanceBuffer.Create(device, 8192);
}

void OpaquePass::Execute(const RenderContext& rc) {

    ZoneScopedN("Pass: Opaque");

    ID3D11DeviceContext* dc = rc.deviceContext;

    RenderQueue* queue = rc.renderQueue;
    ModelRenderer* renderer = rc.modelRenderer;

    if (!queue || !renderer) return;

    if (rc.shadowMap) {
        rc.shadowMap->Bind(dc, SLOT_SRV_SHADOW, SLOT_SMP_SHADOW, SLOT_CB_SHADOW);
    }

    ID3D11SamplerState* samplerStates[] = { rc.renderState->GetSamplerState(SamplerState::LinearWrap) };
    dc->PSSetSamplers(SLOT_SMP_LINEAR, 1, samplerStates);

    _psoOpaque.Apply(dc, rc.renderState);

    DirectX::XMVECTOR CameraPosition = DirectX::XMLoadFloat3(&rc.camera->GetEye());

    queue->SortAndBuildBatches();

    const auto& sortKeys = queue->GetSortedKeys();
    const auto& opaqueCommands = queue->GetOpaqueCommands();

    // メッシュごとのインスタンスデータ再構築用キャッシュ
    static std::vector<InstanceData> instanceDataCache;

    size_t i = 0;
    while (i < sortKeys.size()) {
        uint32_t currentIndex = sortKeys[i].originalIndex;
        const auto& current = opaqueCommands[currentIndex];

        Shader* shader = ShaderRegistry::Instance().GetShader(current.shaderHash, Graphics::Instance().GetDevice());
        if (!shader) {
            i++;
            continue;
        }

        shader->Begin(rc);

        size_t batchStart = i;
        size_t batchCount = 0;
        uint64_t currentBatchKey = sortKeys[batchStart].value;

        while (i < sortKeys.size()) {
            if (sortKeys[i].value != currentBatchKey) break;
            batchCount++;
            i++;
        }

        // =========================================================
        // ★修正：インスタンスバッファの構築をメッシュループ内に移動。
        //   各メッシュが持つ mesh.node->worldTransform（GLTFノード階層で
        //   定義されたモデル内オフセット）を entity の worldMatrix に
        //   掛け合わせることで、パーツが正しい位置に描画される。
        //
        //   旧コード：ループ外で1回だけ entity.worldMatrix をセット
        //            → 全メッシュが同じ座標に描画されてしまう
        //   新コード：ループ内でメッシュごとに
        //            meshNode.worldTransform × entity.worldMatrix を合成
        // =========================================================
        const auto& meshes = current.model->GetMeshes();
        for (size_t meshIdx = 0; meshIdx < meshes.size(); ++meshIdx) {
            const Model::Mesh& mesh = meshes[meshIdx];

            // --- マテリアルオーバーライド ---
            Model::Mesh& mutableMesh = const_cast<Model::Mesh&>(mesh);
            Model::Material* originalMatPtr = mutableMesh.material;
            MaterialData* overrideData = nullptr;

            if (meshIdx < current.overrideCount) {
                MaterialHandle handle = current.overrideMaterials[meshIdx];
                if (handle.IsValid()) {
                    overrideData = ResourceManager::Instance().GetMaterial(handle);
                }
            }

            Model::Material tempMatWrapper;
            bool applyOverride = (overrideData != nullptr);
            if (applyOverride) {
                tempMatWrapper.data = std::shared_ptr<MaterialData>(overrideData, [](MaterialData*) {});
                tempMatWrapper.name = "InstanceOverride";
                mutableMesh.material = &tempMatWrapper;
            }

            // --- 透明判定 ---
            auto& matData = *mutableMesh.material->data;
            float alpha = 1.0f;
            if (matData.colors.count("materialColor")) alpha = matData.colors.at("materialColor").w;
            bool isTransparent = (matData.alphaMode == AlphaMode::Blend || (alpha > 0.01f && alpha < 0.99f));
            bool isSkinned = !mesh.bones.empty();

            // ★ メッシュごとにインスタンスバッファを再構築
            instanceDataCache.clear();
            instanceDataCache.reserve(batchCount);

            for (size_t k = 0; k < batchCount; ++k) {
                uint32_t idx = sortKeys[batchStart + k].originalIndex;
                const auto& cmd = opaqueCommands[idx];

                InstanceData data;
                data.customParams = cmd.customParams;

                if (isSkinned) {
                    // スキンドメッシュ：UpdateTransform() でエンティティの worldMatrix が
                    // すでにボーン行列に焼き込まれている。シェーダー側の Skinning() が
                    // ワールド座標まで完結するため、インスタンス行列は単位行列でよい。
                    DirectX::XMStoreFloat4x4(&data.world, DirectX::XMMatrixIdentity());
                }
                else {
                    // 静的メッシュ：mesh.node->worldTransform（モデル内ノードオフセット）×
                    //               entity.worldMatrix（ECS ワールド行列）を合成する
                    if (mesh.node) {
                        DirectX::XMMATRIX meshLocal = DirectX::XMLoadFloat4x4(&mesh.node->globalTransform);
                        DirectX::XMMATRIX entityWorld = DirectX::XMLoadFloat4x4(&cmd.worldMatrix);
                        DirectX::XMStoreFloat4x4(&data.world, meshLocal * entityWorld);
                    }
                    else {
                        // node が null のフォールバック（通常は発生しない）
                        data.world = cmd.worldMatrix;
                    }
                }
                instanceDataCache.push_back(data);
            }

            _instanceBuffer.Write(dc, instanceDataCache);
            _instanceBuffer.Bind(dc, SLOT_SRV_INSTANCE);

            // --- 描画 ---
            if (isTransparent) {
                // 半透明メッシュは TransparentPass へ転送
                for (size_t k = 0; k < batchCount; ++k) {
                    const auto& info = opaqueCommands[sortKeys[batchStart + k].originalIndex];

                    TransparencyCommand tInfo;
                    tInfo.shaderHash = current.shaderHash;
                    tInfo.mesh = &mesh;
                    tInfo.customParams = info.customParams;

                    if (meshIdx < info.overrideCount) tInfo.overrideMaterial = info.overrideMaterials[meshIdx];
                    else                               tInfo.overrideMaterial = MaterialHandle{};

                    // ★ 半透明コマンドにもノード合成済みの行列を格納する
                    if (!isSkinned && mesh.node) {
                        DirectX::XMMATRIX meshLocal = DirectX::XMLoadFloat4x4(&mesh.node->globalTransform);
                        DirectX::XMMATRIX entityWorld = DirectX::XMLoadFloat4x4(&info.worldMatrix);
                        DirectX::XMStoreFloat4x4(&tInfo.worldMatrix, meshLocal * entityWorld);
                    }
                    else {
                        tInfo.worldMatrix = info.worldMatrix;
                    }

                    // ソート用の距離はノード合成後の座標から算出
                    DirectX::XMVECTOR WorldPos = DirectX::XMLoadFloat4x4(&tInfo.worldMatrix).r[3];
                    tInfo.distance = DirectX::XMVectorGetX(
                        DirectX::XMVector3Length(DirectX::XMVectorSubtract(WorldPos, CameraPosition)));

                    queue->SubmitDeferredTransparent(tInfo);
                }
            }
            else {
                if (isSkinned) {

                    shader->Update(rc, mesh);
                    UINT stride = sizeof(ModelResource::Vertex);
                    UINT offset = 0;
                    dc->IASetVertexBuffers(0, 1, mesh.data->vertexBuffer.GetAddressOf(), &stride, &offset);
                    dc->IASetIndexBuffer(mesh.data->indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
                    dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

                    // スキンドメッシュ：インスタンシング不可、1体ずつ描画
                    for (size_t k = 0; k < batchCount; ++k) {
                        // ★ 修正1: 実際のコマンド配列から、k番目のエンティティの情報を取得する
                        const auto& cmd = opaqueCommands[sortKeys[batchStart + k].originalIndex];

                        // ★ 修正1: 直前で勝手に instData を作らず、上で正しく作ったキャッシュを使う！
                          // (スキンドメッシュ用の単位行列 + カスタムパラメータが確実に入っています)
                        _instanceBuffer.Write(dc, &instanceDataCache[k], 1);
                        _instanceBuffer.Bind(dc, SLOT_SRV_INSTANCE);

                        // ★ 修正2: 1体目の mesh ではなく、現在の描画対象である cmd.model (k体目) の
                        // 正しいメッシュから「自分のボーン行列」を取り出してGPUに送る！
                        const Model::Mesh& instanceMesh = cmd.model->GetMeshes()[meshIdx];
                        renderer->UpdateSkeletonConstants(dc, instanceMesh);

                        // ★ 修正2: 1体だけインスタンス描画する (SV_InstanceID を機能させるため)
                        dc->DrawIndexedInstanced(static_cast<UINT>(mesh.data->indices.size()), 1, 0, 0, 0);
                    }
                }
                else {
                    // 静的メッシュ：インスタンシング描画
                    shader->Update(rc, mesh);
                    UINT stride = sizeof(ModelResource::Vertex);
                    UINT offset = 0;
                    dc->IASetVertexBuffers(0, 1, mesh.data->vertexBuffer.GetAddressOf(), &stride, &offset);
                    dc->IASetIndexBuffer(mesh.data->indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
                    dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

                    // ★ 修正2: 配列の型を XMFLOAT4X4 から InstanceData に変更する
                    static std::vector<InstanceData> instanceDataCache;
                    if (instanceDataCache.size() < batchCount) {
                        instanceDataCache.resize(batchCount);
                    }

                    for (size_t k = 0; k < batchCount; ++k) {
                        const auto& cmd = opaqueCommands[sortKeys[batchStart + k].originalIndex];
                        // ★ 修正3: InstanceData の world メンバに行列を代入
                        instanceDataCache[k].world = cmd.worldMatrix;
                        // instanceDataCache[k].customParams = ... ; // マテリアルパラメータ等
                    }

                    // ★ 修正3: 抽出した行列配列を使って、MAX_INSTANCES(8192)ごとに切り分けて一括描画
                    size_t currentOffset = 0;
                    while (currentOffset < batchCount) {
                        size_t drawCount = (std::min)(batchCount - currentOffset, (size_t)InstanceBuffer::MAX_INSTANCES);

                        // GPUに行列の配列を一括転送
                        _instanceBuffer.Write(dc, instanceDataCache.data() + currentOffset, drawCount);
                        _instanceBuffer.Bind(dc, SLOT_SRV_INSTANCE);

                        // まとめて描画！
                        dc->DrawIndexedInstanced(
                            static_cast<UINT>(mesh.data->indices.size()),
                            static_cast<UINT>(drawCount), 0, 0, 0);

                        currentOffset += drawCount;
                    }
                }
            }

            if (applyOverride) mutableMesh.material = originalMatPtr;

            _instanceBuffer.Unbind(dc, SLOT_SRV_INSTANCE);
        }

        shader->End(rc);
    }
}