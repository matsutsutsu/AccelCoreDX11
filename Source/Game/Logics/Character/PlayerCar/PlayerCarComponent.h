#pragma once
#include "ECS/Common/CCL_Common.h"
#include <DirectXMath.h>

struct PlayerCarComponent {
    // --- 抽象化された入力データ (Input State) ---
    struct InputData {
        float throttle = 0.0f;
        float steering = 0.0f;
        bool jump = false;
    } input;

    CCL::ECS::EntityID physicsSphereID = CCL::ECS::InvalidEntityID;

    float acceleration = 2000.0f;
    float turnSpeed = 150.0f;

    // =======================================================
    // 描画モデルのズレ補正 (Visual Offset & Rotation)
    // =======================================================
    DirectX::XMFLOAT3 visualOffset = { 0.0f, -1.0f, 0.0f };  // 位置のズレ補正（ローカル空間）
    DirectX::XMFLOAT3 visualRotation = { 0.0f, 0.0f, 0.0f }; // 回転のズレ補正（オイラー角/度数法）

    // =======================================================
    // フェイク・サスペンション (視覚的な慣性)
    // =======================================================
    float maxPitchAngle = 5.0f; // アクセル/ブレーキ時の前後の最大傾き（度）
    float maxRollAngle = 8.0f; // カーブ時の左右の最大傾き（度）
    float suspensionSpeed = 10.0f; // 傾きが適用・回復するスピード（バネの硬さ）

    // --- 内部状態（インスペクタには出さない） ---
    DirectX::XMFLOAT4 logicRotation = { 0.0f, 0.0f, 0.0f, 1.0f }; // 車の「論理的」な向き（進行方向）
    bool isLogicRotInitialized = false; // 初期化フラグ

    // 現在の動的な傾き（内部計算用）
    float currentPitch = 0.0f;
    float currentRoll = 0.0f;

    // =======================================================
    // 接地判定（レイキャストの結果を保存）
    // =======================================================
    bool isGrounded = false;
    float raycastLength = 1.2f; // 球の半径(1.0) + 地面の凹凸の許容範囲(0.2)

};