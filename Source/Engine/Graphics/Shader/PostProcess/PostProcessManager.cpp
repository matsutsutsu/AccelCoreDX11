#include "PostProcessManager.h"
#include "Engine/Graphics/Core/GpuResourceUtils.h"
#include "Engine/Graphics/Shader/ShaderResources.h"

struct CbToneMap {
    float exposure;
    float padding[3];
};

void PostProcessManager::Initialize(ID3D11Device* device, UINT width, UINT height)
{
    // シェーダー読み込み
    _toneMapRes = std::make_unique<ToneMapResources>();
    GpuResourceUtils::LoadVertexShader(device, "Assets/Shader/FullscreenVS.cso", nullptr, 0, nullptr, _toneMapRes->vs.GetAddressOf());
    GpuResourceUtils::LoadPixelShader(device, "Assets/Shader/ToneMapPS.cso", _toneMapRes->ps.GetAddressOf());
    GpuResourceUtils::CreateDynamicConstantBuffer(device, sizeof(CbToneMap), _toneMapRes->cbToneMap.GetAddressOf());

    // サンプラー
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    device->CreateSamplerState(&sampDesc, _toneMapRes->sampler.GetAddressOf());

    // バッファ作成
    Resize(device, width, height);
}

// デストラクタで部屋を返す
PostProcessManager::~PostProcessManager()
{
    if (_sceneHDR.IsValid()) ResourceManager::Instance().UnloadRenderTarget(_sceneHDR);
    if (_outputSDR.IsValid()) ResourceManager::Instance().UnloadRenderTarget(_outputSDR);
}

void PostProcessManager::Resize(ID3D11Device* device, UINT width, UINT height)
{
    if (width == 0 || height == 0) return;

    // ★追加: 古いものがあれば破棄
    if (_sceneHDR.IsValid()) ResourceManager::Instance().UnloadRenderTarget(_sceneHDR);
    if (_outputSDR.IsValid()) ResourceManager::Instance().UnloadRenderTarget(_outputSDR);

    // シーン用: HDR (R16G16B16A16_FLOAT)
   // 管理人に作ってもらう
    _sceneHDR = ResourceManager::Instance().CreateRenderTarget(width, height, DXGI_FORMAT_R16G16B16A16_FLOAT);
    
    // 出力用: SDR (R8G8B8A8_UNORM)
    _outputSDR = ResourceManager::Instance().CreateRenderTarget(width, height, DXGI_FORMAT_R8G8B8A8_UNORM);
    // ブルームパスもリサイズ
    _bloomPass.Initialize(device, width, height);
}

void PostProcessManager::BeginScene(ID3D11DeviceContext* dc, const float clearColor[4], ID3D11DepthStencilView* dsv)
{
    // HDRバッファをクリアして、描画先に設定
    // 深度バッファ(dsv)は、これまでのものを使い回す（Zテストするため）
    RenderTarget *sceneHDR = ResourceManager::Instance().GetRenderTarget(_sceneHDR);
    if (!sceneHDR) return;

    // =========================================================
    // 安全装置: 解放済みメモリ等から異常なサイズが返ってきた場合は
    // ポストプロセスをスキップしてGPUクラッシュを防ぐ
    // =========================================================
    float w = static_cast<float>(sceneHDR->GetWidth());
    float h = static_cast<float>(sceneHDR->GetHeight());
    if (w <= 0.0f || w > 16384.0f || h <= 0.0f || h > 16384.0f) {
        return;
    }

    sceneHDR->Clear(dc, clearColor);

    // RTV切り替え
    ID3D11RenderTargetView *rtv = sceneHDR->GetRTV();
    dc->OMSetRenderTargets(1, &rtv, dsv);

    // ビューポート設定
    D3D11_VIEWPORT vp = {};
    vp.Width          = w;
    vp.Height         = h;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    dc->RSSetViewports(1, &vp);
}

ID3D11ShaderResourceView* PostProcessManager::EndScene(ID3D11DeviceContext* dc, ID3D11RenderTargetView* targetRTV)
{
    // =========================================================================
    // HDRバッファ(_sceneHDR)を読み込む前に、Render Targetとしての登録を解除する。
    // これをしないと、DirectXのルールにより入力テクスチャが強制的に NULL にされます。
    // =========================================================================

    RenderTarget *sceneHDR  = ResourceManager::Instance().GetRenderTarget(_sceneHDR);
    RenderTarget *outputSDR = ResourceManager::Instance().GetRenderTarget(_outputSDR);
    if (!sceneHDR || !outputSDR) return nullptr;

    // =========================================================================
    // ゴミデータ（解放済みメモリ）アクセスに対する緊急安全装置
    // もしシーン遷移直後でメモリが破棄され、異常なサイズ(3722305024等)になっていたら
    // 直ちに描画を中止し、DirectXのクラッシュを防ぐ。
    // =========================================================================
    float outW = (float)outputSDR->GetWidth();
    float outH = (float)outputSDR->GetHeight();
    if (outW <= 0.0f || outW > 16384.0f || outH <= 0.0f || outH > 16384.0f) {
        return nullptr; // 異常を検知したら何もせずに帰る
    }


    ID3D11RenderTargetView* nullRTVs[] = { nullptr };
    dc->OMSetRenderTargets(1, nullRTVs, nullptr);

    // 1. ブルーム処理 (SceneHDR -> BloomTexture)
    ID3D11ShaderResourceView* bloomSRV = nullptr;
    if (bloomConfig.enable) {
        bloomSRV = _bloomPass.Process(dc, sceneHDR->GetSRV(), bloomConfig);
    }

    // 出力先の設定
    if (targetRTV) {
        // 指定されたRTV（例：バックバッファ）に描く
        dc->OMSetRenderTargets(1, &targetRTV, nullptr); // 深度バッファは不要

        // ビューポートを画面サイズ（最大サイズ）に戻す！ 
        // これを忘れると、ブルームの最後のパス（半分のサイズ）の設定が残ってしまいます
        D3D11_VIEWPORT vp = {};
        vp.Width          = (float)outputSDR->GetWidth();
        vp.Height         = (float)outputSDR->GetHeight();
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        vp.TopLeftX = 0.0f;
        vp.TopLeftY = 0.0f;
        dc->RSSetViewports(1, &vp);
    }
    else {
        // 2. トーンマップ合成 (SceneHDR + Bloom -> OutputSDR)
        // 指定がなければ内部のSDRバッファに描く (ImGui用)
        outputSDR->Activate(dc);
        // Activateの中でRSSetViewportsしているため、こちらはそのままでOK
    }

    // ステート設定
    dc->IASetInputLayout(nullptr);
    dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    dc->VSSetShader(_toneMapRes->vs.Get(), nullptr, 0);
    dc->PSSetShader(_toneMapRes->ps.Get(), nullptr, 0);

    ID3D11SamplerState* samplers[] = { _toneMapRes->sampler.Get() };
    dc->PSSetSamplers(SLOT_SMP_DEFAULT, 1, samplers);

    // 定数バッファ更新
    CbToneMap cbData;
    cbData.exposure = toneMapConfig.exposure; // コンポーネントの値を使用
    D3D11_MAPPED_SUBRESOURCE mapped;
    dc->Map(_toneMapRes->cbToneMap.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, &cbData, sizeof(CbToneMap));
    dc->Unmap(_toneMapRes->cbToneMap.Get(), 0);
    dc->PSSetConstantBuffers(SLOT_CB_TONEMAP, 1, _toneMapRes->cbToneMap.GetAddressOf());

    // テクスチャセット
    // t0: シーン, t1: ブルーム
    ID3D11ShaderResourceView *srvs[] = {sceneHDR->GetSRV(), bloomSRV};
    dc->PSSetShaderResources(SLOT_SRV_BLOOM_IN, 2, srvs);

    // 描画（フルスクリーン）
    dc->Draw(3, 0);

    // 解除
    ID3D11ShaderResourceView* nullSRVs[] = { nullptr, nullptr };
    dc->PSSetShaderResources(SLOT_SRV_BLOOM_IN, 2, nullSRVs);

    // 最終的な画像 を返す
    return outputSDR->GetSRV();
}