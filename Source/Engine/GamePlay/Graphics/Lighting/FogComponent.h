#pragma once
#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl.h>

struct FogComponent {
    DirectX::XMFLOAT4 color{0.7f, 0.7f, 0.8f, 1.0f};

    // --- 距離フォグ ---
    float start = 10.0f;
    float end   = 50.0f;

    // --- 高さフォグ ---
    float heightStart   = 0.0f; // 霧が発生する地面の高さ
    float heightDensity = 0.5f; // 高さによる霧の濃さ

    // --- ノイズフォグ (今回追加) ---
    char              noiseTexturePath[256] = {};   // テクスチャパス
    float             noiseScale            = 0.1f; // ノイズの粗さ (UVスケール)
    float             noiseStrength         = 0.0f; // ノイズの影響度 (0.0=無効)
    DirectX::XMFLOAT2 noiseSpeed{0.1f, 0.05f};      // 流れる速度

    // --- リムライト (Atmosphere Rim) ---
    // キャラクターやオブジェクトの輪郭を光らせ、背景との分離を良くします
    DirectX::XMFLOAT3 rimColor{1.0f, 1.0f, 1.0f}; // 発光色
    float             rimPower    = 3.0f;         // 輪郭の鋭さ (高いほど細い)
    float             rimStrength = 0.0f;         // 発光強度 (0.0で無効)

    bool              enabled = true;
    DirectX::XMFLOAT4 center{0.0f, 0.0f, 0.0f, 1.0f}; // プレイヤー中心座標

    // ランタイム用リソース (保存しない)
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> noiseTextureSRV;
};