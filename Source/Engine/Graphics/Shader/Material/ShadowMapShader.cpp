#include "Engine/Core/Common/Misc.h"
#include "Engine/Graphics/Core/GpuResourceUtils.h"
#include "ShadowMapShader.h"

#include "Engine/Graphics/Shader/Pass/ShadowMap.h"

ShadowMapShader::ShadowMapShader(ID3D11Device* device) {
    // 影書き込み専用の頂点シェーダーをロード
    GpuResourceUtils::LoadVertexShader(
        device,
        "Assets/Shader/ShadowMapVS.cso",
        ModelResource::InputElementDescs.data(),
        static_cast<UINT>(ModelResource::InputElementDescs.size()),
        inputLayout.GetAddressOf(),
        vertexShader.GetAddressOf());

    // VSに定数バッファ(CbMeshなど)が含まれている場合に備えてリフレクションを初期化
    // (現在は空でも、将来的にスキンメッシュ用の行列などが必要になった際に役立つ)
    InitializeReflection(device, "Assets/Shader/ShadowMapVS.cso");
}

void ShadowMapShader::Begin(const RenderContext& rc) {
    ID3D11DeviceContext* dc = rc.deviceContext;

    dc->IASetInputLayout(inputLayout.Get());
    dc->VSSetShader(vertexShader.Get(), nullptr, 0);

    // 影書き込み時はピクセルシェーダーを無効化
    dc->PSSetShader(nullptr, nullptr, 0);

    // リフレクションで管理している全バッファをVSにバインド
    for (auto& pair : _constantBuffers) {
        pair.second.BindVS(dc);
    }
}

void ShadowMapShader::Update(const RenderContext& rc, const Model::Mesh& mesh) {
    ID3D11DeviceContext* dc = rc.deviceContext;

    // シャドウマップ生成パスでは、色やテクスチャは基本的に不要なので
    // UpdateMaterialProperties を呼ぶ必要性は薄いです。
    // もしアルファテスト（葉っぱの影など）をするなら必要になりますが、
    // 単純な影なら何もしなくてOKです。

    // もし CbMesh が HLSL 内に定義されていれば更新する
    if (_constantBuffers.count("CbMesh")) {
        // 頂点情報だけ更新しとけばいいのでカラーは別にいい

        //auto& cb = _constantBuffers["CbMesh"];
        //cb.SetValue("materialColor", mesh.material->data->baseColor); // 影には不要だが一応セット
        //cb.UpdateBuffer(dc);
    }

}

void ShadowMapShader::End(const RenderContext& rc) {
    // 全解除
    UnbindResources(rc.deviceContext);
}