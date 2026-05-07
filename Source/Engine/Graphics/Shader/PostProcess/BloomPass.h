#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <vector>
#include "Engine/GamePlay/Graphics/PostProcess/BloomConfigComponent.h"
#include "Engine/Graphics/Resource/ResourceManager.h" 
#include "Engine/Graphics/Core/RenderTarget.h" 

class BloomPass {
public:
    void Initialize(ID3D11Device* device, int width, int height);

    // デストラクタを追加して、終了時に管理人に部屋を返却するようにする
    ~BloomPass();

    // シーン描画後のテクスチャを受け取り、ブルーム処理済みのテクスチャを返す
    ID3D11ShaderResourceView* Process(
        ID3D11DeviceContext* dc,
        ID3D11ShaderResourceView* inputSceneSRV,
        const BloomConfigComponent& config
    );

private:
    // MipMapチェーン（縮小バッファ群）
    // 例: 0=Half, 1=Quarter, 2=Eighth, 3=Sixteenth...
    static const int kMaxIterations = 6;

    // 実体ではなくハンドルの配列にする
    std::vector<RenderTargetHandle> _downChain;  // ダウンサンプリング用
    std::vector<RenderTargetHandle> _upChain;    // アップサンプリング用


};