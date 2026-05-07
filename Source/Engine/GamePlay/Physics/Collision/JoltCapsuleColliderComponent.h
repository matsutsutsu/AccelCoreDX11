#pragma once
#include <DirectXMath.h>

// カプセルの形状（設計図）
// Joltの仕様では、「円柱部分の半分の高さ(halfHeight)」と「球部分の半径(radius)」で定義します。
// 全体の高さ = (halfHeight * 2) + (radius * 2) になります。
struct JoltCapsuleColliderComponent {
    float halfHeight = 0.5f; // 円柱部分の半分の高さ
    float radius     = 0.5f; // 半径

    // 基準点からのローカル位置ズレ
    DirectX::XMFLOAT3 localOffset = { 0.0f, 0.0f, 0.0f };

    // コライダー単体のローカル回転（横向きカプセル等に必須）
    DirectX::XMFLOAT3 localRotationEuler = { 0.0f, 0.0f, 0.0f };

    bool  isDirty    = false; // サイズを変更した時にtrueにする
};
