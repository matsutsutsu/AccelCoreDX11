#include "TrailShader.h"
#include "Engine/Graphics/Core/GpuResourceUtils.h"

TrailShader::TrailShader(ID3D11Device* device)
{
    // =========================================================
    // ★重要: トレイル専用の頂点レイアウト
    // TrailRenderer::Vertex の構造 (float3, float4, float2) と完全に一致させます
    // =========================================================
    D3D11_INPUT_ELEMENT_DESC inputElementDescs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    // 頂点シェーダーのロード
    // ※ CSOファイルのパスはあなたのプロジェクトの階層に合わせて修正してください
    GpuResourceUtils::LoadVertexShader(device,
        "Assets/Shader/TrailVS.cso",
        inputElementDescs,
        ARRAYSIZE(inputElementDescs),
        inputLayout.GetAddressOf(),
        vertexShader.GetAddressOf());

    // ピクセルシェーダーのロード
    GpuResourceUtils::LoadPixelShader(
        device, "Assets/Shader/TrailPS.cso", pixelShader.GetAddressOf());

    // ※ トレイルはマテリアル定数バッファを自動転送しないため、InitializeReflection は不要です
}

void TrailShader::Begin(const RenderContext& rc)
{
    // パイプライン設定 (VS, PS, InputLayout をバインド)
    ApplyShaderPipeline(rc.deviceContext, vertexShader.Get(), pixelShader.Get(), inputLayout.Get());
}

void TrailShader::Update(const RenderContext& rc, const Model::Mesh& mesh)
{
    // トレイルは Model::Mesh を使わない特殊な描画を行うため、ここは空でOKです
    // （TransparentPass内で TrailRenderer::Render を直接呼ぶため）
}

void TrailShader::End(const RenderContext& rc)
{
    // リソース解除（テクスチャやシェーダーの紐付けを解く）
    UnbindResources(rc.deviceContext);
}