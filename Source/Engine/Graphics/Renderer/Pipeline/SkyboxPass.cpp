#include "SkyboxPass.h"
#include "Engine/Graphics/Core/Graphics.h"
#include "Engine/Graphics/Core/Camera.h"
#include "Engine/Graphics/Core/GpuResourceUtils.h" 
#include "Engine/Graphics/Shader/ShaderResources.h"


void SkyboxPass::Initialize(ID3D11Device* device) {
    // 1. GpuResourceUtils を使用したシェーダーのロード
    HRESULT hr = GpuResourceUtils::LoadVertexShader(
        device,
        "Assets/Shader/SkyboxVS.cso",
        nullptr, 0, // InputElementなし
        nullptr,
        &_vertexShader);
    _ASSERT_EXPR(SUCCEEDED(hr), L"SkyboxVSのロードに失敗");


    hr = GpuResourceUtils::LoadPixelShader(
        device,
        "Assets/Shader/SkyboxPS.cso",
        &_pixelShader);
    _ASSERT_EXPR(SUCCEEDED(hr), L"SkyboxPSのロードに失敗");

    // 2. GpuResourceUtils を使用した Dynamic 定数バッファの作成
    hr = GpuResourceUtils::CreateDynamicConstantBuffer(device, sizeof(CbSkybox), &_cbSkybox);
    _ASSERT_EXPR(SUCCEEDED(hr), L"Skybox用定数バッファの作成に失敗");

    // 高解像度の背景テクスチャをロード
    hr = GpuResourceUtils::LoadTexture(
        device,
        "Assets/Textures/IBL/skybox_highres.dds",
        &_skyboxTex
    );
    _ASSERT_EXPR(SUCCEEDED(hr), L"Skybox背景のロードに失敗");

}

void SkyboxPass::Execute(const RenderContext& rc) {

    ZoneScopedN("Pass: Skybox");

    if (!rc.camera) return;

    ID3D11DeviceContext* dc = rc.deviceContext;

    // 1. 逆行列 (Inverse View-Projection) を計算
    DirectX::XMMATRIX V = DirectX::XMLoadFloat4x4(&rc.camera->GetView());
    DirectX::XMMATRIX P = DirectX::XMLoadFloat4x4(&rc.camera->GetProjection());
    DirectX::XMMATRIX invVP = DirectX::XMMatrixInverse(nullptr, V * P);

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(dc->Map(_cbSkybox.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        CbSkybox* cb = (CbSkybox*)mapped.pData;
        DirectX::XMStoreFloat4x4(&cb->invViewProj, invVP);
        dc->Unmap(_cbSkybox.Get(), 0);
    }

    // 2. パイプラインへのセット
    dc->VSSetShader(_vertexShader.Get(), nullptr, 0);
    dc->PSSetShader(_pixelShader.Get(), nullptr, 0);

    // 定義した SLOT_CB_SKYBOX (2) にバインド
    dc->VSSetConstantBuffers(SLOT_CB_SKYBOX, 1, _cbSkybox.GetAddressOf());

    // t17 に高解像度テクスチャをセット
    dc->PSSetShaderResources(SLOT_SRV_SKYBOX_BG, 1, _skyboxTex.GetAddressOf());

    // 頂点バッファは「空（null）」
    dc->IASetInputLayout(nullptr);
    dc->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    dc->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    //  TestLessEqual と SolidCullNone の適用
    dc->OMSetDepthStencilState(rc.renderState->GetDepthStencilState(DepthState::TestLessEqual), 0);
    dc->RSSetState(rc.renderState->GetRasterizerState(RasterizerState::SolidCullNone));

    // 3. 描画 (3頂点の三角形を1枚だけ呼び出す)
    dc->Draw(3, 0);

    // 4. ステートの復元
    dc->OMSetDepthStencilState(rc.renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
    dc->RSSetState(rc.renderState->GetRasterizerState(RasterizerState::SolidCullBack));
}