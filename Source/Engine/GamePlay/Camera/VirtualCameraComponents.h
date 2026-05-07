#pragma once
#include "ECS/Common/CCL_Common.h"
#include <DirectXMath.h>
#include <algorithm> // for max

// 1. 仮想カメラの基本情報 (vCam)
// これがついているエンティティは「カメラ候補」になります。
struct VirtualCamera {
    int priority = 0; // 優先度（高いものが採用される）

    // このカメラに切り替わるときにかける時間 (秒)
    float blendTime = 1.0f;

    // カメラ設定
    float fov      = 45.0f;   // 視野角 (度数法)
    float nearClip = 0.1f;    // ← これが 0.0f になっているとクラッシュ
    float farClip  = 1000.0f; // ← これも 0.0f だとクラッシュ

    // 計算結果格納用（システムがここに書き込みます）
    // ※ TransformComponentは使わず、あえてここを使います（計算途中を汚さないため）
    DirectX::XMFLOAT3 resultPos    = {0, 0, 0};
    DirectX::XMFLOAT3 resultLookAt = {0, 0, 1};
    DirectX::XMFLOAT3 resultUp     = {0, 1, 0};

    // 状態管理（ブレンド用）
    bool isValid = false; // 計算結果が有効か（最初のフレーム対策）
};

// 2. 「移動」戦略: ターゲット追従 (Body)
struct CameraBodyFollow {
    CCL::ECS::EntityID target  = 0;
    DirectX::XMFLOAT3  offset  = {0, 5, -10}; // ターゲット基準のオフセット
    float              damping = 5.0f;  // 追従の遅延（大きいほどキビキビ、小さいほどフワフワ）
    bool               lockY   = false; // 高さを固定するかどうか
};

// 3. 「視線」戦略: ターゲット注視 (Aim)
struct CameraAimLookAt {
    CCL::ECS::EntityID target  = 0;
    DirectX::XMFLOAT3  offset  = {0, 0, 0}; // ターゲット中心からのズレ（頭、胸など）
    float              damping = 10.0f;     // 視線移動の遅延
};

// 4. 「揺れ」演出用コンポーネント (Shake)
struct CameraShake {
    float amplitude   = 0.0f; // 揺れの強さ (現在の強さ)
    float duration    = 0.0f; // 残り時間
    float maxDuration = 1.0f; // 開始時の時間（減衰計算用: 1.0 -> 0.0）

    // 設定ヘルパー
    void SetShake(float strength, float time)
    {
        amplitude   = strength;
        maxDuration = time;
        duration    = time;
    }
};

// ---------------------------------------------------------
// 新しい拡張: 自由移動用ボディコンポーネント
// ---------------------------------------------------------
// 1. 自由移動用の振る舞いデータ
struct CameraBodyFree {
    float moveSpeed    = 10.0f; // 移動速度
    float lookSpeed    = 0.2f;  // 回転感度 (Degree)
    float currentYaw   = 0.0f;  // 現在のY軸回転 (Deg)
    float currentPitch = 0.0f;  // 現在のX軸回転 (Deg)
};


// ===================================================================================
// TPS（三人称視点）カメラ用コンポーネント
// ===================================================================================
struct CameraBodyTPS {
    CCL::ECS::EntityID targetEntity = CCL::ECS::InvalidEntityID; // 追従するプレイヤーのID

    float distance = 6.0f;  // プレイヤーからカメラまでの距離
    float currentPitch = 20.0f; // 縦の角度（上下）
    float currentYaw = 0.0f;  // 横の角度（左右）

    float lookSpeedX = 300.0f; // マウス・右スティックでの横回転スピード
    float lookSpeedY = 200.0f; // マウス・右スティックでの縦回転スピード
    float speedFactor = 1.0f; 

    DirectX::XMFLOAT3 targetOffset = { 0.0f, 1.5f, 0.0f }; // プレイヤーの足元ではなく「頭や肩」を注視するためのズレ
};

// ---------------------------------------------------------
// FPS（一人称視点）カメラ用コンポーネント
// ---------------------------------------------------------
struct CameraBodyFPS {
    CCL::ECS::EntityID targetEntity = CCL::ECS::InvalidEntityID; // 追従するプレイヤーのID

    float mouseSensitivity = 0.1f;
    float minPitch = -89.0f;
    float maxPitch = 89.0f;

    // 内部状態
    float currentYaw = 0.0f;
    float currentPitch = 0.0f;

    // オフセット（目線の高さなど）
    DirectX::XMFLOAT3 eyeOffset = { 0.0f, 1.7f, 0.0f };
    DirectX::XMFLOAT4 rotationOffset = { 0, 0, 0, 1 };       // 走行時などの揺れ回転
};

// ===================================================================================
// TPSロックオン用コンポーネント（POD）
// ===================================================================================
struct CameraLockOn
{
    CCL::ECS::EntityID targetEntity = CCL::ECS::InvalidEntityID; // ロックオン対象

    float rotationDamping = 10.0f;
    float minDistance = 4.0f;  // 最短カメラ距離
    float maxDistance = 28.0f; // 最長カメラ距離

    // --- 肩越し視点・レイアウト制御 ---
    float sideOffset = 1.5f;     // 左右へのズレ量
    float currentSide = 1.0f;    // 1.0(右出し) or -1.0(左出し)
    float sideDamping = 5.0f;    // 左右移動の滑らかさ

    // --- 画面内維持・FOV制御 ---
    float minFov = 45.0f;        // 通常時のFOV
    float maxFov = 65.0f;        // 接近時に広角にする最大値
    float originalFov = -1.0f;   // FOV退避用

    // --- ロックオン中の制約 ---
    float speedReductionFactor = 0.01f;
    float originalLookSpeedX = -1.0f;
    float originalLookSpeedY = -1.0f;
    float originalDistance = -1.0f;

    // --- 内部計算用 ---
    float focusWeight = 0.4f;
    DirectX::XMFLOAT3 targetOffset = { 0.0f, 0.1f, 0.0f };

    // ==========================================
    // --- 補間移動・切り替え制御用（追加分） ---
    // ==========================================

    // 現在補間計算中の値（この値を最終的なカメラに適用する）
    DirectX::XMFLOAT3 currentInterpolatedPos = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 currentInterpolatedLookAt = { 0.0f, 0.0f, 0.0f };

    // 補間速度 (大きいほど素早く、小さいほどゆったり)
    float interpolationSpeed = 12.5f;

    // 切り替え判定用
    CCL::ECS::EntityID lastTargetEntity = CCL::ECS::InvalidEntityID;
    bool isInitialized = false; // 補間の初期化が完了しているか
};