#include "DissolveShader.h"
#include "Engine/Core/Common/Misc.h"
#include "Engine/Graphics/Core/GpuResourceUtils.h"

DissolveShader::DissolveShader(ID3D11Device *device)
{
    // VSはBasicVSを流用
    GpuResourceUtils::LoadVertexShader(device,
        "Assets/Shader/BasicVS.cso",
        ModelResource::InputElementDescs.data(),
        (UINT)ModelResource::InputElementDescs.size(),
        inputLayout.GetAddressOf(),
        vertexShader.GetAddressOf());

    // PSに今回作成したディゾルブシェーダーを指定
    GpuResourceUtils::LoadPixelShader(
        device, "Assets/Shader/DissolvePS.cso", pixelShader.GetAddressOf());

    // リフレクション解析実行 (これにより dissolveThreshold などが自動登録される)
    InitializeReflection(device, "Assets/Shader/DissolvePS.cso");
}

void DissolveShader::Begin(const RenderContext &rc)
{
    ApplyShaderPipeline(rc.deviceContext, vertexShader.Get(), pixelShader.Get(), inputLayout.Get());
}

void DissolveShader::Update(const RenderContext &rc, const Model::Mesh &mesh)
{
    if (mesh.material && mesh.material->data) {
        // リフレクションベースの自動適用 (全てよしなにやってくれる)
        UpdateMaterialProperties(rc.deviceContext, *mesh.material->data);
    }
}

void DissolveShader::End(const RenderContext &rc) { UnbindResources(rc.deviceContext); }