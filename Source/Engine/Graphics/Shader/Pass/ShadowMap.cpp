// ShadowMap.cpp
#include "ShadowMap.h"
#include <imgui.h>
#include "Engine/Core/Common/Misc.h"
#include "Engine/Graphics/Core/GpuResourceUtils.h"



// コンストラクタ
ShadowMap::ShadowMap(ID3D11Device* device, UINT width, UINT height)
{
    HRESULT hr = S_OK;

    this->width = width;
    this->height = height;


    // 1. テクスチャの器を作成 (Texture2DArray)
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = SHADOW_CASCADE_COUNT; // ★変更: 3枚の配列にする
    desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> depthBuffer;
    hr = device->CreateTexture2D(&desc, nullptr, depthBuffer.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    // 2. 書き込む用の口 (DSV) をカスケードの枚数分作る
    depthStencilViews.resize(SHADOW_CASCADE_COUNT);
    for (int i = 0; i < SHADOW_CASCADE_COUNT; ++i) {
        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
        dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY; // ★配列として扱う
        dsvDesc.Texture2DArray.FirstArraySlice = i;                 // ★i番目の層を指定
        dsvDesc.Texture2DArray.ArraySize = 1;                       // 1層だけ書き込む

        hr = device->CreateDepthStencilView(depthBuffer.Get(), &dsvDesc, depthStencilViews[i].GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
    }

    // 3. 読み込む用の口 (SRV) を作成 (3枚まとめてシェーダーに送る)
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY; // ★配列として読む
    srvDesc.Texture2DArray.MostDetailedMip = 0;
    srvDesc.Texture2DArray.MipLevels = 1;
    srvDesc.Texture2DArray.FirstArraySlice = 0;
    srvDesc.Texture2DArray.ArraySize = SHADOW_CASCADE_COUNT;    // ★3枚全部読む

    hr = device->CreateShaderResourceView(depthBuffer.Get(), &srvDesc, shaderResourceView.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    // サンプラと定数バッファの作成は既存のまま
    D3D11_SAMPLER_DESC sampDesc{};
    sampDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
    sampDesc.BorderColor[0] = 1.0f;
    sampDesc.BorderColor[1] = 1.0f;
    sampDesc.BorderColor[2] = 1.0f;
    sampDesc.BorderColor[3] = 1.0f;
    device->CreateSamplerState(&sampDesc, samplerState.GetAddressOf());

    GpuResourceUtils::CreateDynamicConstantBuffer(device, sizeof(CbShadow), constantBuffer.GetAddressOf());

    viewport.Width = (float)width;
    viewport.Height = (float)height;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
}



// カスケードシャドウマップ用切り替え関数
// cascadeIndex で指定された層（DSV）をターゲットにしてクリア＆セット
void ShadowMap::Activate(ID3D11DeviceContext* dc, int cascadeIndex)
{
    dc->ClearDepthStencilView(depthStencilViews[cascadeIndex].Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    dc->RSSetViewports(1, &viewport);
    dc->OMSetRenderTargets(0, nullptr, depthStencilViews[cascadeIndex].Get());
}

// 元に戻す
void ShadowMap::Deactivate(ID3D11DeviceContext* dc)
{
    ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
    // スロット10番決め打ちではなく、適切な解除が必要ですが、
    // ここは現状のアーキテクチャに合わせて呼び出し側で安全に管理されている前提とします。
    dc->OMSetRenderTargets(0, nullptr, nullptr);
}


//void ShadowMap::DrawGUI() {
//    if (ImGui::CollapsingHeader(u8"ShadowMap 設定 (Control)")) {
//
//        // 影の浮き・縞模様の調整
//        ImGui::DragFloat(u8"影のバイアス (Bias)", &params.shadowBias, 0.0001f, 0.0f, 0.05f, "%.4f");
//        ImGui::Separator();
//
//        // 影の濃さ・境界の減衰
//        ImGui::DragFloat(u8"影の減衰 (Attenuation)", &params.shadowAttenuation, 0.01f, 0.0f, 2.0f);
//        ImGui::ColorEdit4(u8"影の色 (Shadow Color)", &params.shadowColor.x);
//        ImGui::Separator();
//
//        // ライトが照らす範囲
//        ImGui::DragFloat(u8"ライトの投影範囲 (Ortho Size)", &params.orthoSize, 0.1f, 1.0f, 100.0f);
//
//        if (shaderResourceView) {
//            ImGui::Text(u8"シャドウマップ・バッファの確認:");
//            // 深度値が見えやすいようにプレビュー表示
//            ImGui::Image((void*)shaderResourceView.Get(), ImVec2(256, 256),
//                ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImVec4(1, 1, 1, 1));
//        }
//    }
//}

void ShadowMap::Bind(ID3D11DeviceContext* dc, int srvSlot, int samplerSlot, int cbSlot)
{
    // 1. テクスチャのバインド
    if (srvSlot >= 0) {
        ID3D11ShaderResourceView* srvs[] = { shaderResourceView.Get() };
        dc->PSSetShaderResources((UINT)srvSlot, 1, srvs);
    }

    // 2. サンプラーのバインド
    if (samplerSlot >= 0) {
        ID3D11SamplerState* samplers[] = { samplerState.Get() };
        dc->PSSetSamplers((UINT)samplerSlot, 1, samplers);
    }

    // 3. 定数バッファのバインド
    if (constantBuffer && cbSlot >= 0) {
        ID3D11Buffer* cbs[] = { constantBuffer.Get() };
        // 座標変換に使うためVS、影の濃さなどの判定に使うためPSの両方にセット
        dc->VSSetConstantBuffers((UINT)cbSlot, 1, cbs);
        dc->PSSetConstantBuffers((UINT)cbSlot, 1, cbs);
    }
}

void ShadowMap::UpdateConstantBuffer(ID3D11DeviceContext* dc, const CbShadow& data)
{
    if (!constantBuffer) return;

    // GPUリソースをマップして書き込み可能にする
    D3D11_MAPPED_SUBRESOURCE mappedResource;
	// D3D11_MAP_WRITE_DISCARD を使って、古いデータは破棄する
    HRESULT hr = dc->Map(
        constantBuffer.Get(),
        0,
        D3D11_MAP_WRITE_DISCARD,
        0,
        &mappedResource
    );

    if (SUCCEEDED(hr))
    {
        // CPU上の構造体データをGPUバッファへコピー
        memcpy(mappedResource.pData, &data, sizeof(CbShadow));

        // アンマップしてGPU側のアクセスを再開
        dc->Unmap(constantBuffer.Get(), 0);
    }
    else
    {
        // エラーハンドリング
        OutputDebugStringA("SHADOWMAP CONSTANT BUFFER MAP FAILED\n");
    }
}
