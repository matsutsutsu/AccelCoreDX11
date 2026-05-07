#include "TransparentPass.h"
#include "Engine/Graphics/Renderer/ModelRenderer.h"
#include "Engine/Graphics/Shader/ShaderRegistry.h"
#include "Engine/Graphics/Core/Graphics.h"
#include "Engine/Graphics/Renderer/RenderQueue.h"
#include "Engine/Graphics/Shader/ShaderResources.h"
#include <algorithm>

void TransparentPass::Initialize(ID3D11Device* device) {
    _psoTransparent = PipelineState(PipelineStateDesc::DefaultTransparent());
    _instanceBuffer.Create(device, 8192);
}

void TransparentPass::Execute(const RenderContext& rc) {

    ZoneScopedN("Pass: Transparent");

    ID3D11DeviceContext* dc = rc.deviceContext;

    RenderQueue* queue = rc.renderQueue;
    ModelRenderer* renderer = rc.modelRenderer;

    if (!queue || !renderer) return;

    if (rc.shadowMap) {
        rc.shadowMap->Bind(dc, SLOT_SRV_SHADOW, SLOT_SMP_SHADOW, SLOT_CB_SHADOW);
    }

    _psoTransparent.Apply(dc, rc.renderState);

    // ECS直通分 + OpaquePass から転送されてきた半透明コマンドを合体
    auto allTrans = queue->GetDeferredTransparentCommands();
    for (const auto& t : queue->GetTransparentCommands()) {
        allTrans.push_back(t);
    }

    // 奥から手前へのソート
    std::sort(allTrans.begin(), allTrans.end(),
        [](const TransparencyCommand& lhs, const TransparencyCommand& rhs) {
            return lhs.distance > rhs.distance;
        });

    for (const TransparencyCommand& info : allTrans) {
        Shader* shader = ShaderRegistry::Instance().GetShader(info.shaderHash, Graphics::Instance().GetDevice());
        if (!shader) continue;

        shader->Begin(rc);

        const Model::Mesh& mesh = *info.mesh;
        Model::Mesh& mutableMesh = const_cast<Model::Mesh&>(mesh);
        Model::Material* originalMatPtr = mutableMesh.material;
        Model::Material    tempMatWrapper;

        bool applyOverride = info.overrideMaterial.IsValid();
        if (applyOverride) {
            MaterialData* ptr = ResourceManager::Instance().GetMaterial(info.overrideMaterial);
            if (ptr) {
                tempMatWrapper.data = std::shared_ptr<MaterialData>(ptr, [](MaterialData*) {});
                tempMatWrapper.name = "TranspOverride";
                mutableMesh.material = &tempMatWrapper;
            }
            else {
                applyOverride = false;
            }
        }

        // =========================================================
        // ★修正：OpaquePass と同じ理由で mesh.node->worldTransform を合成する。
        //
        //   TransparentPass に届く TransparencyCommand の worldMatrix は、
        //   OpaquePass 側で修正済みの「ノード合成後の行列」が格納されている。
        //   ただし ECS から直接 SubmitTransparent() された場合（GetTransparentCommands）
        //   はノード合成が行われていないため、ここで追加適用する。
        //
        //   スキンドメッシュ：UpdateTransform() でボーン行列に焼き込み済み。
        //                     インスタンス行列は単位行列とする。
        //   静的メッシュ    ：mesh.node->worldTransform × worldMatrix を合成。
        // =========================================================
        bool isSkinned = !mesh.bones.empty();

        InstanceData singleData;
        singleData.customParams = info.customParams;

        if (isSkinned) {
            // スキンドメッシュ：シェーダーの Skinning() がワールド座標まで完結する
            DirectX::XMStoreFloat4x4(&singleData.world, DirectX::XMMatrixIdentity());
        }
        else {
            // 静的メッシュ：OpaquePass から転送されてきたコマンドは合成済みだが、
            // ECS 直通コマンド（GetTransparentCommands）はまだ合成されていない。
            // TransparencyCommand には「どちら由来か」のフラグがないため、
            // 常に mesh.node->worldTransform を掛けることで統一する。
            //
            // ただし OpaquePass 側（GetDeferredTransparentCommands）の
            // worldMatrix はすでにノード合成済みであることに注意。
            // → OpaquePass の修正により、転送コマンドは「合成後行列」が
            //   格納されるようになったため、ここでは ECS 直通分だけ処理すればよい。
            //   しかし両者を区別するフラグが TransparencyCommand にないため、
            //   最もシンプルな解決策として TransparencyCommand に
            //   「ノード合成済み」フラグを追加することを推奨する。
            //
            // 暫定対応：OpaquePass 側で転送する際に worldMatrix を合成済みにしたため、
            //           ここでは info.worldMatrix をそのままセットする。
            //           ECS 直通コマンドを使う場合は SubmitTransparent() を呼ぶ前に
            //           呼び出し元でノード合成を行うこと（ModelRenderSystem側）。
            singleData.world = info.worldMatrix;
        }

        std::vector<InstanceData> temp = { singleData };
        _instanceBuffer.Write(dc, temp);
        _instanceBuffer.Bind(dc, SLOT_SRV_INSTANCE);

        // スキンドメッシュのみスケルトン定数バッファを更新
        if (isSkinned) {
            renderer->UpdateSkeletonConstants(dc, mesh);
        }

        shader->Update(rc, mesh);

        UINT stride = sizeof(ModelResource::Vertex);
        UINT offset = 0;
        dc->IASetVertexBuffers(0, 1, mesh.data->vertexBuffer.GetAddressOf(), &stride, &offset);
        dc->IASetIndexBuffer(mesh.data->indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        dc->DrawIndexed(static_cast<UINT>(mesh.data->indices.size()), 0, 0);

        if (applyOverride) mutableMesh.material = originalMatPtr;

        _instanceBuffer.Unbind(dc, SLOT_SRV_INSTANCE);
        shader->End(rc);
    }
}