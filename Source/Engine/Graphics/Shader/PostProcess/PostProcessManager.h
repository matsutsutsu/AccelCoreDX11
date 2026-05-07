#pragma once
#include <memory>
#include <d3d11.h>
#include <wrl/client.h>
#include "BloomPass.h"
#include "Engine/Graphics/Core/RenderTarget.h"
#include "Engine/Graphics/Resource/ResourceManager.h" 
#include "Engine/GamePlay/Graphics/PostProcess/ToneMapConfigComponent.h" // インクルード追加

// ポストプロセス全体を管理するクラス
class PostProcessManager
{
public:
    void Initialize(ID3D11Device* device, UINT width, UINT height);

    // デストラクタ
    ~PostProcessManager();

    void Resize(ID3D11Device* device, UINT width, UINT height);

    // シーン描画の開始（HDRバッファに切り替え）
    // clearColor: 背景色, dsv: 深度バッファ(使い回す)
    void BeginScene(ID3D11DeviceContext* dc, const float clearColor[4], ID3D11DepthStencilView* dsv);

    // シーン描画の終了とポストプロセスの適用
    // 戻り値: 最終的なSDR画像 (ImGuiで表示可能)
    // 出力先のRTVを指定できるようにする (デフォルトは nullptr で内部バッファ使用)
    ID3D11ShaderResourceView* EndScene(ID3D11DeviceContext* dc, ID3D11RenderTargetView* targetRTV = nullptr);

    // パラメータ（ECSからコピーしてくる用）
    BloomConfigComponent bloomConfig;
    ToneMapConfigComponent toneMapConfig;

private:
    // シーン描画用 HDRバッファ
  RenderTargetHandle _sceneHDR{};

    // 最終出力用 SDRバッファ
  RenderTargetHandle _outputSDR{};

    // ブルーム処理クラス
    BloomPass _bloomPass;

    // トーンマップ用リソース
    struct ToneMapResources
    {
        Microsoft::WRL::ComPtr<ID3D11VertexShader> vs;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> ps;
        Microsoft::WRL::ComPtr<ID3D11Buffer> cbToneMap;
        Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
    };
    std::unique_ptr<ToneMapResources> _toneMapRes;
};