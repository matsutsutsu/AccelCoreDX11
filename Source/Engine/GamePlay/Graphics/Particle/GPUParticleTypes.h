#pragma once
#include <DirectXMath.h>

// HLSL側の定義とバイトレイアウトを完全に一致させる
struct Particle {
    int   state;   // 状態 (0:Dead, 1:Alive)
    float angle;   // 回転角
    int   pad0[2]; // HLSLのパディングに合わせる (16バイト境界維持)

    DirectX::XMFLOAT4 color; // 色

    DirectX::XMFLOAT3 position; // 位置
    float             mass;     // 質量

    float             angular_speed; // 回転速度
    DirectX::XMFLOAT3 velocity;      // 速度

    float             lifespan; // 寿命
    float             age;      // 経過時間
    DirectX::XMFLOAT2 size;     // サイズ(初期, 終了)

    unsigned int      chip;  // チップ番号
    unsigned int      type;  // タイプ
    DirectX::XMFLOAT2 scale; // 現在のスケール
};

// パーティクルシステムの設定パラメータ
// シェーダーの cbuffer PARTICLE_SYSTEM_CONSTANTS に対応
struct ParticleSystemConfig {
    DirectX::XMFLOAT4 emission_position      = {0, 0, 0, 1};
    // エミッター（発生源）の回転情報
    DirectX::XMFLOAT4 emission_rotation      = {0, 0, 0, 1};
    DirectX::XMFLOAT2 emission_offset        = {0, 1};
    DirectX::XMFLOAT2 emission_size          = {1, 0};
    DirectX::XMFLOAT2 emission_cone_angle    = {0, 3.14159f};
    DirectX::XMFLOAT2 emission_speed         = {1, 5};
    DirectX::XMFLOAT2 emission_angular_speed = {-1, 1};
    DirectX::XMFLOAT2 lifespan               = {1, 3};
    DirectX::XMFLOAT2 spawn_delay            = {0, 0.1f};
    DirectX::XMFLOAT2 fade_duration          = {0.2f, 0.2f};
    DirectX::XMFLOAT4 manual_color           = {1, 1, 1, 1};

    float time       = 0.0f;
    float delta_time = 0.0f;
    float noise_scale    = 1.0f; // ノイズの細かさ (周波数)
    float noise_strength = 1.0f; // ノイズの影響力 (強さ)

    float gravity     = -9.8f;
    // 速度連動ストレッチ係数 (0.0なら無効、大きいほど伸びる)
    float             velocity_stretch = 0.0f;
    //  UVスクロール速度 (X, Y)
    DirectX::XMFLOAT2 uv_scroll_speed = {0.0f, 0.0f};

    // アニメーション設定 (パディングを利用して追加)
    // 0: Random (ランダムな1枚で固定)
    // 1: Life (寿命に合わせて最初から最後まで再生)
    // 2: Loop (指定速度でループ) ... 今回はまず1まで実装
    int sprite_anim_mode = 0;

    // 0: Additive (加算・発光系)
    // 1: Transparent (半透明・煙/黒色系)
    int               render_mode = 0;
    DirectX::XMFLOAT2 padding1         = {0, 0}; // パディング

    DirectX::XMUINT2 sprite_sheet_grid = {1, 1};
    DirectX::XMFLOAT2 particle_scale      = {1, 1};

    // 2. ワールド座標を追加 (float3)
    //    burst_trigger (float) と合わせてちょうど 16バイト(float4) になるように配置
    DirectX::XMFLOAT3 world_position = {0, 0, 0};
    // 3. カラーモード
    int color_mode = 0;


    int max_particle_count = 10000;
    int type               = 0; // 0:Point, 1:Quad
    int emission_mode      = 0; // 0:Continuous, 1:Burst
    int burst_count        = 100;

    float burst_trigger      = 0.0f; // >0で発火
    float particle_offset_y  = 0.0f;
    float emission_stretch_x = 1.0f;
    float emission_stretch_z = 1.0f;
};