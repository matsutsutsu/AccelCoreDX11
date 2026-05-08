#pragma once
#include <DirectXMath.h>
#include <array>
#include <string>
#include "Engine/Graphics/Resource/MaterialData.h"

// トレイルの1フレーム分の履歴
struct TrailPoint {
    DirectX::XMFLOAT3 basePos; // 剣の根本のワールド座標
    DirectX::XMFLOAT3 tipPos;  // 剣の先端のワールド座標
    float age = 0.0f;          // このポイントが生成されてからの経過時間
};

// トレイルの設定とデータを保持するコンポーネント
struct TrailComponent {
    // === 制御パラメータ ===
    bool  isEmitting = false; // trueの間だけ軌跡を生成する
    float lifeTime = 0.3f;  // 軌跡が消滅するまでの時間（秒）
    float minVertexDistance = 0.05f; // 前回の点からこの距離以上離れないと点を作らない（頂点数の節約）
    DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f }; // トレイルの基本色

    // === データストレージ (リングバッファ) ===
    static constexpr int MAX_TRAIL_POINTS = 64; // 1振りの剣の軌跡なら64個で十分すぎる量です
    std::array<TrailPoint, MAX_TRAIL_POINTS> history;

    int headIndex = 0; // 最新のデータが入っているインデックス
    int count = 0;     // 現在有効なポイントの数

    // 剣のモデルにおける「根本」と「先端」のローカル座標
    DirectX::XMFLOAT3 localBasePos = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 localTipPos = { 0.0f, 1.0f, 0.0f }; // Y軸方向に1mの剣と仮定

    std::string texturePath = "Assets/Textures/VFX/VFX_Ramp_Magic_03.png"; // デフォルトのパス（適宜変更してください）
    TextureHandle textureHandle; // ロードしたテクスチャの実体へのチケット

    // --- リングバッファ用ヘルパー ---
    // 最新のポイントを追加する
    void AddPoint(const DirectX::XMFLOAT3& base, const DirectX::XMFLOAT3& tip) {
        headIndex = (headIndex + 1) % MAX_TRAIL_POINTS;
        history[headIndex] = { base, tip, 0.0f };
        if (count < MAX_TRAIL_POINTS) count++;
    }

    // 最新のポイントを取得する（距離計算用）
    const TrailPoint& GetLatestPoint() const {
        return history[headIndex];
    }

    // n個前の古いポイントを取得する (0 = 最新, 1 = 1つ前...)
    const TrailPoint& GetPointFromLatest(int n) const {
        int index = (headIndex - n + MAX_TRAIL_POINTS) % MAX_TRAIL_POINTS;
        return history[index];
    }
};