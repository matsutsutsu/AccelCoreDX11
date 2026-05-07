#include "BloomPass.h"
#include "Engine/Graphics/Core/GpuResourceUtils.h"
#include "Engine/Graphics/Shader/ShaderResources.h"
#include <algorithm> // for max
#include <memory>

#include "Engine/Graphics/Core/Graphics.h"


// --- シェーダー管理用内部クラス ---
struct BloomResources
{
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> psExtract;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> psDown;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> psUp;
    Microsoft::WRL::ComPtr<ID3D11Buffer> cbBloom;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;

    struct CbBloomData {
        float threshold;
        float softKnee;
        float intensity;
        float padding;
        float texelWidth;
        float texelHeight;
        float pad2[2];
    };

    void Initialize(ID3D11Device* device)
    {
        // 頂点シェーダー (入力レイアウトなし)
        GpuResourceUtils::LoadVertexShader(device, "Assets/Shader/FullscreenVS.cso", nullptr, 0, nullptr, vs.GetAddressOf());

        // ピクセルシェーダー群
        GpuResourceUtils::LoadPixelShader(device, "Assets/Shader/BloomExtractionPS.cso", psExtract.GetAddressOf());
        GpuResourceUtils::LoadPixelShader(device, "Assets/Shader/BloomDownsamplePS.cso", psDown.GetAddressOf());
        GpuResourceUtils::LoadPixelShader(device, "Assets/Shader/BloomUpsamplePS.cso", psUp.GetAddressOf());

        // 定数バッファ
        GpuResourceUtils::CreateDynamicConstantBuffer(device, sizeof(CbBloomData), cbBloom.GetAddressOf());

        // サンプラー (Clamp推奨)
        D3D11_SAMPLER_DESC sampDesc = {};
        sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP; // 端の色を引き延ばす
        sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        device->CreateSamplerState(&sampDesc, sampler.GetAddressOf());
    }
};

// 静的インスタンスでリソースを持つ（簡易実装）
// 本来はメンバにするか ResourceManager 管理推奨
static std::unique_ptr<BloomResources> g_bloomResources;

// ----------------------------------------------------------------------------

// デストラクタでハンドルを解放
BloomPass::~BloomPass()
{
    for (auto h : _downChain) ResourceManager::Instance().UnloadRenderTarget(h);
    for (auto h : _upChain) ResourceManager::Instance().UnloadRenderTarget(h);
    _downChain.clear();
    _upChain.clear();
}

void BloomPass::Initialize(ID3D11Device* device, int width, int height)
{
    // シェーダー読み込み (初回のみ)
    if (!g_bloomResources) {
        g_bloomResources = std::make_unique<BloomResources>();
        g_bloomResources->Initialize(device);
    }

    // 古いハンドルがあれば先に破棄する
    for (auto h : _downChain) ResourceManager::Instance().UnloadRenderTarget(h);
    for (auto h : _upChain) ResourceManager::Instance().UnloadRenderTarget(h);

    // リサイズ対応のため、一度クリア
    _downChain.clear();
    _upChain.clear();

    // メモリ再確保を避けるためにreserve推奨
    _downChain.reserve(kMaxIterations);
    _upChain.reserve(kMaxIterations);

    // 縮小チェーンの作成
    // Full -> Half -> Quarter ... と半分ずつ小さくしていく
    UINT w = width;
    UINT h = height;

    // kMaxIterationsの回数分の縮小・拡大ループを作る
    for (int i = 0; i < kMaxIterations; ++i)
    {
        // 次のサイズ（最低1ピクセル）
        // サイズを半分にする (例: 1920 -> 960 -> 480 -> 240...)
        w = (std::max)(1u, w / 2);
        h = (std::max)(1u, h / 2);


        // HDRフォーマット (R16G16B16A16_FLOAT) が必須！
        // これじゃないと1.0以上の明るさが保存できず、ブルームが光りません
        // HDR（光の強さ）を保存できる特別な画用紙を用意する

        // --- ダウンサンプリング用 ---
        // 管理人にRenderTargetを作ってもらい、チケットを保存
        RenderTargetHandle downHandle = ResourceManager::Instance().CreateRenderTarget(w, h, DXGI_FORMAT_R16G16B16A16_FLOAT);
        _downChain.push_back(downHandle);

        // --- アップサンプリング用 ---
        RenderTargetHandle upHandle = ResourceManager::Instance().CreateRenderTarget(w, h, DXGI_FORMAT_R16G16B16A16_FLOAT);
        _upChain.push_back(upHandle);
    }
}



ID3D11ShaderResourceView* BloomPass::Process(
    ID3D11DeviceContext* dc,
    ID3D11ShaderResourceView* inputSceneSRV,
    const BloomConfigComponent& config)
{
    if (!g_bloomResources || _downChain.empty()) return inputSceneSRV;

    // --- 1. 共通ステート設定 ---
    dc->IASetInputLayout(nullptr); // InputLayout不要
    dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    dc->VSSetShader(g_bloomResources->vs.Get(), nullptr, 0);

    // =================================================================
    // 【修正箇所】ブレンドステートを「上書きモード」にリセットする
    // これをしないと、パーティクル等の加算設定が残って焼き付きます
    // =================================================================
    // 第1引数は nullptr でデフォルト（Opaque）になることが多いですが、
    // エンジン側の RenderState クラスから Opaque を取得するのが確実です。
    // ここでは Graphics クラス経由か、もしくは nullptr (Default State) を試します。

    // もし Graphics::Instance() が参照できるなら:
     dc->OMSetBlendState(Graphics::Instance().GetRenderState()->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);

    // もし単純にデフォルト（上書き）に戻したいだけなら nullptr でOKな場合が多いです:
    //dc->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    // =================================================================


    ID3D11SamplerState* samplers[] = { g_bloomResources->sampler.Get() };
    dc->PSSetSamplers(SLOT_SMP_BLOOM, 1, samplers);

    // 定数バッファの基本設定
    BloomResources::CbBloomData cbData;
    cbData.threshold = config.threshold;
    cbData.softKnee = config.softKnee;
    cbData.intensity = config.intensity;
    // TexelSizeはループ内で設定

    // --- 2. 抽出パス (Input -> Down[0]) ---
    {
        // 定数バッファ更新 (入力サイズ基準)
        // ハンドルから実体を取得
        RenderTarget* down0 = ResourceManager::Instance().GetRenderTarget(_downChain[0]);
        if (!down0) return inputSceneSRV;

        cbData.texelWidth = 1.0f / (down0->GetWidth() * 2.0f);
        cbData.texelHeight = 1.0f / (down0->GetHeight() * 2.0f);

        D3D11_MAPPED_SUBRESOURCE mapped;
        dc->Map(g_bloomResources->cbBloom.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        memcpy(mapped.pData, &cbData, sizeof(cbData));
        dc->Unmap(g_bloomResources->cbBloom.Get(), 0);
        dc->PSSetConstantBuffers(SLOT_CB_BLOOM, 1, g_bloomResources->cbBloom.GetAddressOf());

        // シェーダー設定
        dc->PSSetShader(g_bloomResources->psExtract.Get(), nullptr, 0);
        // 元画像（シーン画像）からまぶしいところだけを抽出する

        ID3D11ShaderResourceView* srvs[] = { inputSceneSRV };
        dc->PSSetShaderResources(SLOT_SRV_BLOOM_IN, 1, srvs);

        // 描画 (Target: Down[0])
        down0->Activate(dc);
        dc->Draw(3, 0); // 三角形1枚

        // SRV解除
        ID3D11ShaderResourceView* nullSRV[] = { nullptr };
        dc->PSSetShaderResources(SLOT_SRV_BLOOM_IN, 1, nullSRV);
    }

    // --- 3. ダウンサンプリングループ (Down[i] -> Down[i+1]) ---
    dc->PSSetShader(g_bloomResources->psDown.Get(), nullptr, 0);

    for (size_t i = 0; i < _downChain.size() - 1; ++i)
    {
        // 今のシーン画像の中身から次の（半分）画像に書く
        // ハンドルから実体を取得
        RenderTarget *src = ResourceManager::Instance().GetRenderTarget(_downChain[i]);
        RenderTarget *dst = ResourceManager::Instance().GetRenderTarget(_downChain[i + 1]);
        if (!src || !dst) continue;

        // 定数バッファ更新 (Sourceサイズ)
        cbData.texelWidth  = 1.0f / src->GetWidth();
        cbData.texelHeight = 1.0f / src->GetHeight();

        D3D11_MAPPED_SUBRESOURCE mapped;
        dc->Map(g_bloomResources->cbBloom.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        memcpy(mapped.pData, &cbData, sizeof(cbData));
        dc->Unmap(g_bloomResources->cbBloom.Get(), 0);

        // 先に dst (出力) をアクティブにする。
        // これにより、src (直前の出力) が RTV から外れ、読み込めるようになる。
        dst->Activate(dc);

        // セット & 描画
        // その後に src を SRV (入力) としてセット
        ID3D11ShaderResourceView *srvs[] = {src->GetSRV()};
        dc->PSSetShaderResources(SLOT_SRV_BLOOM_IN, 1, srvs);

        dc->Draw(3, 0);

        // 解除
        ID3D11ShaderResourceView* nullSRV[] = { nullptr };
        dc->PSSetShaderResources(SLOT_SRV_BLOOM_IN, 1, nullSRV);

        // RTVも解除してパイプラインから完全に剥がす
        ID3D11RenderTargetView *nullRTV[] = {nullptr};
        dc->OMSetRenderTargets(1, nullRTV, nullptr);
    }

    // --- 4. アップサンプリングループ (Down[last] -> Up[last-1] ... -> Up[0]) ---
    dc->PSSetShader(g_bloomResources->psUp.Get(), nullptr, 0);

    // 最初の入力は「一番小さいDownバッファ」
    RenderTarget *lastDown = ResourceManager::Instance().GetRenderTarget(_downChain.back());
    ID3D11ShaderResourceView *currentInputSRV = lastDown ? lastDown->GetSRV() : nullptr;

    for (int i = (int)_downChain.size() - 2; i >= 0; --i)
    {
        // 小さい画像を拡大したもの　＋　保存しておいた鮮明な画像（downChain）
        // ＝　良い感じにボケてなじんだ画像を作成する    
        
        // 書き込み先
        RenderTarget *dst     = ResourceManager::Instance().GetRenderTarget(_upChain[i]);
        // 加算合成する高解像度画像
        RenderTarget *highRes = ResourceManager::Instance().GetRenderTarget(_downChain[i]);

        if (!dst || !highRes) continue;

        // 定数バッファ更新 (入力テクスチャのサイズ。※ここではフィルター半径調整用)
        // アップサンプル時は少し半径を広げると綺麗にボケます
        float radius = config.radius;
        cbData.texelWidth  = (1.0f / dst->GetWidth()) * radius;
        cbData.texelHeight = (1.0f / dst->GetHeight()) * radius;

        D3D11_MAPPED_SUBRESOURCE mapped;
        dc->Map(g_bloomResources->cbBloom.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        memcpy(mapped.pData, &cbData, sizeof(cbData));
        dc->Unmap(g_bloomResources->cbBloom.Get(), 0);

        dst->Activate(dc);

        // セット (t0: 低解像度入力, t1: 加算する高解像度Downバッファ)
        ID3D11ShaderResourceView *srvs[] = {currentInputSRV, highRes->GetSRV()};
        dc->PSSetShaderResources(SLOT_SRV_BLOOM_IN, 2, srvs);

        dc->Draw(3, 0);

        // 解除
        ID3D11ShaderResourceView* nullSRV[] = { nullptr, nullptr };
        dc->PSSetShaderResources(SLOT_SRV_BLOOM_IN, 2, nullSRV);

        // ★追加: RTVも解除してパイプラインから完全に剥がす
        ID3D11RenderTargetView *nullRTV[] = {nullptr};
        dc->OMSetRenderTargets(1, nullRTV, nullptr);

        // 次の入力は、今書き込んだUpバッファ
        currentInputSRV = dst->GetSRV();
    }

    // 最終結果（Up[0]）を返す
    RenderTarget* finalUp = ResourceManager::Instance().GetRenderTarget(_upChain[0]);
    return finalUp ? finalUp->GetSRV() : nullptr;
}