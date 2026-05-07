#pragma once
#include <memory>
#include <vector>
#include <wrl.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include "Engine/Graphics/Resource/Model.h"
#include "Engine/Graphics/Core/RenderContext.h"
#include "Engine/Graphics/Renderer/Pipeline/RenderPipeline.h"

class ModelRenderer
{
public:
    ModelRenderer(ID3D11Device* device);
    ~ModelRenderer();

    // 毎フレームの描画実行（パイプラインを回す）
    void Render(const RenderContext& rc);

    // =========================================================
    // 舞台監督 (EnvironmentSystem) や 各Pass から呼ばれるAPI
    // =========================================================

    void BindAllGlobalConstantBuffers(ID3D11DeviceContext* dc);
    void UpdateSkeletonConstants(ID3D11DeviceContext* dc, const Model::Mesh& mesh);

    
private:
    RenderPipeline _pipeline; // 全ての描画を司るパイプライン

  
    // スケルトン用定数バッファ
    ID3D11Buffer* _cbSkeletonBuffer = nullptr;
    static const int MAX_BONES = 256;
    struct CbSkeleton {
        DirectX::XMFLOAT4X4 boneTransforms[MAX_BONES];
    };
    CbSkeleton m_cbSkeletonCache = {};

    // 内部初期化・解放関数
    void InitializeGlobalConstantBuffers(ID3D11Device* device);
    void UnbindAllResources(ID3D11DeviceContext* dc);
};