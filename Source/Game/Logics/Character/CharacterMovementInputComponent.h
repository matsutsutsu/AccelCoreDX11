#pragma once
#include "SimpleMath.h"

/**
 * @brief キャラクターの「動きたい方向と速度」を保持する純粋な入力データ。
 * @note プレイヤー操作システムやAI操舵システムがここに書き込み、
 * 物理システム（JoltCharacterUpdateSystem）がこれを読み取って実際の移動を行う。
 */
struct CharacterMovementInputComponent {
    DirectX::SimpleMath::Vector3 desiredVelocity = { 0.0f, 0.0f, 0.0f }; // 目標とする速度ベクトル
    DirectX::SimpleMath::Vector3 desiredLookDir = { 0.0f, 0.0f, 1.0f }; // 向けたい方向
    bool                         jumpRequested = false;              // (拡張用) ジャンプ要求
};