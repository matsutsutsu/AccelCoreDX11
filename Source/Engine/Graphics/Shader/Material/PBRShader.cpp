#include "PBRShader.h"
#include "Engine/Graphics/Core/GpuResourceUtils.h"
#include "Engine/Graphics/Resource/ModelResource.h"

PBRShader::PBRShader(ID3D11Device *device)
{
    // 頂点シェーダーのロード
    // (PBRでも頂点レイアウトは共通のものを使用可能)
    GpuResourceUtils::LoadVertexShader(device,
        "Assets/Shader/PBR_VS.cso",
        ModelResource::InputElementDescs.data(),
        (UINT)ModelResource::InputElementDescs.size(),
        inputLayout.GetAddressOf(),
        vertexShader.GetAddressOf());

    // ピクセルシェーダーのロード
    GpuResourceUtils::LoadPixelShader(
        device, "Assets/Shader/PBR_PS.cso", pixelShader.GetAddressOf());

    // ★重要: リフレクションの初期化
    // これを呼ぶことで、シェーダー内の変数を解析し、
    // MaterialDataの値を自動で定数バッファに転送できるようにします
    InitializeReflection(device, "Assets/Shader/PBR_PS.cso");

}

void PBRShader::Begin(const RenderContext &rc)
{
    // パイプライン設定 (VS, PS, InputLayout, 定数バッファの一括バインド)
    ApplyShaderPipeline(rc.deviceContext, vertexShader.Get(), pixelShader.Get(), inputLayout.Get());
}

void PBRShader::Update(const RenderContext &rc, const Model::Mesh &mesh)
{
    // マテリアルプロパティの自動適用
    // Shaderクラスの共通処理を使って、テクスチャや数値をGPUに送る
    if (mesh.material && mesh.material->data) {
        UpdateMaterialProperties(rc.deviceContext, *mesh.material->data);
    }
}

void PBRShader::End(const RenderContext &rc)
{
    // リソース解除（テクスチャやシェーダーの紐付けを解く）
    UnbindResources(rc.deviceContext);
}