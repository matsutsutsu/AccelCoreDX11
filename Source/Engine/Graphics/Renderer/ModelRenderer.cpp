#include "ModelRenderer.h"
#include "Engine/Graphics/Core/Graphics.h"
#include "Engine/Graphics/Shader/ShaderResources.h" // SLOT定義用
#include "Engine/Graphics/Renderer/RenderQueue.h"

#include "Engine/Graphics/Renderer/Pipeline/SetupPass.h"
#include "Engine/Graphics/Renderer/Pipeline/ShadowPass.h"
#include "Engine/Graphics/Renderer/Pipeline/OpaquePass.h"
#include "Engine/Graphics/Renderer/Pipeline/SkyboxPass.h"
#include "Engine/Graphics/Renderer/Pipeline/TransparentPass.h"
#include "Engine/Graphics/Renderer/Pipeline/ParticlePass.h"
#include "Engine/Graphics/Renderer/Pipeline/PrimitivePass.h"
#include "Engine/Graphics/Renderer/Pipeline/PostProcessPass.h"

ModelRenderer::ModelRenderer(ID3D11Device* device)
{
    // グローバル定数バッファの初期化
    InitializeGlobalConstantBuffers(device);

    // =========================================================
    // レンダーパイプライン組み立て
    // =========================================================
    
    // 1. 準備工程 (HDRバッファのクリアとバインド)
    _pipeline.AddPass(std::make_shared<SetupPass>());
    
    // 2. 影の描画 (ライト視点での深度書き込み)
    _pipeline.AddPass(std::make_shared<ShadowPass>());
    
    // 3. 不透明オブジェクト (Zバッファと色の書き込み)
    _pipeline.AddPass(std::make_shared<OpaquePass>());

    //  Skybox (Zバッファを参照し、空いている隙間にだけ空を描く)
    _pipeline.AddPass(std::make_shared<SkyboxPass>());
    
    // 4. 半透明オブジェクト (Zテストしつつ奥から描画)
    _pipeline.AddPass(std::make_shared<TransparentPass>());
    
    // 5. パーティクル (半透明モデルのさらに上に描画)
    _pipeline.AddPass(std::make_shared<ParticlePass>());
    
    // 6. プリミティブ/デバッグ線 (ワールド空間の最後に描く)
    _pipeline.AddPass(std::make_shared<PrimitivePass>());
    
    // 7. ポストプロセス (HDR->SDR変換、ブルーム合成、画面への出力)
    _pipeline.AddPass(std::make_shared<PostProcessPass>());

    // 全パスの初期化
    _pipeline.InitializeAll(device);


    OutputDebugStringA("ModelRenderer: Initialized pipeline architecture.\n");
}

ModelRenderer::~ModelRenderer()
{
    // パイプライン（各Pass）を明示的に破棄
    _pipeline.Clear();

    // 自身が確保したスケルトン用の定数バッファを解放（メモリリーク防止）
    if (_cbSkeletonBuffer) {
        _cbSkeletonBuffer->Release();
        _cbSkeletonBuffer = nullptr;
    }
}

void ModelRenderer::InitializeGlobalConstantBuffers(ID3D11Device* device)
{
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeof(CbSkeleton);
    // 16バイトアライメント
    if (desc.ByteWidth % 16 != 0) desc.ByteWidth = ((desc.ByteWidth + 15) / 16) * 16;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    device->CreateBuffer(&desc, nullptr, &_cbSkeletonBuffer);
}

void ModelRenderer::Render(const RenderContext& rc)
{
    // ★ 全ての描画パスを順番に実行 ★
    _pipeline.ExecuteAll(rc);

    // 最後にリソースのバインドを解除して安全な状態に戻す
    UnbindAllResources(rc.deviceContext);
}

void ModelRenderer::UpdateSkeletonConstants(ID3D11DeviceContext* dc, const Model::Mesh& mesh)
{
    if (!_cbSkeletonBuffer) return;

    if (!mesh.bones.empty()) {
        size_t count = mesh.bones.size();
        if (count > MAX_BONES) count = MAX_BONES;

        for (size_t i = 0; i < count; ++i) {
            const Model::Bone& bone = mesh.bones[i];
            DirectX::XMMATRIX matOffset = DirectX::XMLoadFloat4x4(&bone.data->offsetTransform);
            DirectX::XMMATRIX matWorld = DirectX::XMLoadFloat4x4(&bone.node->worldTransform);
            DirectX::XMMATRIX matFinal = matOffset * matWorld;
            DirectX::XMStoreFloat4x4(&m_cbSkeletonCache.boneTransforms[i], matFinal);
        }
    }
    else {
        m_cbSkeletonCache.boneTransforms[0] = mesh.node->worldTransform;
    }

    dc->UpdateSubresource(_cbSkeletonBuffer, 0, nullptr, &m_cbSkeletonCache, 0, 0);

    ID3D11Buffer* buffer = _cbSkeletonBuffer;
    dc->VSSetConstantBuffers(SLOT_CB_SKELETON, 1, &buffer);
}

void ModelRenderer::BindAllGlobalConstantBuffers(ID3D11DeviceContext* dc)
{
    // CbScene (スロット b8)
    ID3D11Buffer* sceneBuf = Graphics::Instance().GetSceneConstantBuffer();
    if (sceneBuf) {
        dc->VSSetConstantBuffers(SLOT_CB_SCENE, 1, &sceneBuf);
        dc->PSSetConstantBuffers(SLOT_CB_SCENE, 1, &sceneBuf);
    }

    // CbSkeleton (スロット b12)
    if (_cbSkeletonBuffer) {
        dc->VSSetConstantBuffers(SLOT_CB_SKELETON, 1, &_cbSkeletonBuffer);
    }
}

void ModelRenderer::UnbindAllResources(ID3D11DeviceContext* dc)
{
    ID3D11Buffer* nullCB = nullptr;
    ID3D11SamplerState* nullSampler = nullptr;
    ID3D11ShaderResourceView* nullSRV = nullptr;

    dc->VSSetConstantBuffers(SLOT_CB_SCENE, 1, &nullCB);
    dc->PSSetConstantBuffers(SLOT_CB_SCENE, 1, &nullCB);
    dc->VSSetConstantBuffers(SLOT_CB_SKELETON, 1, &nullCB);
    dc->PSSetConstantBuffers(SLOT_CB_SKELETON, 1, &nullCB);

    dc->PSSetSamplers(0, 1, &nullSampler);

    dc->VSSetConstantBuffers(SLOT_CB_LIGHT, 1, &nullCB);
    dc->PSSetConstantBuffers(SLOT_CB_LIGHT, 1, &nullCB);

    dc->VSSetConstantBuffers(SLOT_CB_SHADOW, 1, &nullCB);
    dc->PSSetConstantBuffers(SLOT_CB_SHADOW, 1, &nullCB);

    dc->PSSetShaderResources(SLOT_SRV_SHADOW, 1, &nullSRV);
    dc->PSSetSamplers(SLOT_SMP_SHADOW, 1, &nullSampler);
}