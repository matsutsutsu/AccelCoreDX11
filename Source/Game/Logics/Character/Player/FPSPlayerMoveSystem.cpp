//4/15作製　桃田
// --- FPSPlayerMoveSystem.cpp ---
#include "FPSPlayerComponent.h"
#include "FPSPlayerMoveSystem.h"
#include "ECS/Core/CCL_World.h"
#include <DirectXMath.h>
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

#include "Engine/Graphics/Core/Camera.h"
#include "Engine/GamePlay/Camera/VirtualCameraComponents.h"
#include "PlayerTag.h"

#include "Game/Logics/Combat/StaminaComponent.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "Game/Logics/System/Modifier/ModifierComponent.h"


// ★ Joltのインクルードは一切不要！純粋なAPI窓口だけを呼ぶ
#include "Engine/Physics/IPhysicsAPI.h" 
#include <memory>

using namespace DirectX;
using namespace CCL::ECS;


void FPSPlayerMoveSystem::Update(float dt)
{
    // 1. メインカメラを取得 (PlayerInputSystem と同じ方法)
    Camera* mainCamera = nullptr;
    if (_world->HasResource<Camera*>()) {
        mainCamera = _world->GetResource<Camera*>();
    }

    // カメラが取得できなかった場合のデフォルト方向（バックアップ）
    XMVECTOR camForward = XMVectorSet(0, 0, 1, 0);
    XMVECTOR camRight = XMVectorSet(1, 0, 0, 0);

    if (mainCamera) {
        // 1. カメラのビュー行列（XMFLOAT4X4）を取得し、SIMDレジスタ（XMMATRIX）にロード（Load）する
        XMMATRIX viewMat = XMLoadFloat4x4(&mainCamera->GetView());

        // 2. SIMDレジスタ上で逆行列計算を行う
        XMMATRIX invView = XMMatrixInverse(nullptr, viewMat);
        camForward = invView.r[2]; // Forward (Z軸)
        camRight = invView.r[0]; // Right (X軸)

        // 地面移動用にY成分を抜いて正規化
        camForward = XMVector3Normalize(XMVectorSetY(camForward, 0.0f));
        camRight = XMVector3Normalize(XMVectorSetY(camRight, 0.0f));
    }

    // PlayerModifierStatusComponent をクエリに追加
    ForEachWithID([&](EntityID entityID,
        TransformComponent& trans,
        FPSPlayerComponent& fps,
        StaminaComponent& stam,
        const ModifierStatusComponent& modStatus) // キャッシュされたステータスを参照
        {
            if (_world->HasComponent<PlayerTag::HideTag>(entityID)) return;

            // --- A. 移動入力の判定 ---
            bool hasInput = (std::abs(fps.input.moveForward) > 0.001f || std::abs(fps.input.moveRight) > 0.001f);

            // --- B. 状態の決定 ---
            bool canRun = hasInput && fps.input.isRunPressed && !stam.isFatigued && stam.current > 0.0f;
            if (canRun) {
                fps.currentState = PlayerState::Running;
                stam.isConsuming = true;
            }
            else if (hasInput) {
                fps.currentState = PlayerState::Walking;
                stam.isConsuming = false;
            }
            else {
                fps.currentState = PlayerState::Idle;
                stam.isConsuming = false;
            }

            // --- C. 目標速度の決定（キャッシュされた Mult を使用） ---
            float baseTargetSpeed = 0.0f;
            stam.isMoving = false;

            switch (fps.currentState) {
            case PlayerState::Idle:
                baseTargetSpeed = 0.0f;
                break;
            case PlayerState::Walking:
                stam.isMoving = true;
                // 走査なし：直接計算済み倍率を掛ける
                baseTargetSpeed = fps.walkSpeed * modStatus.moveWalkSpeedMult;
                break;
            case PlayerState::Running:
                stam.isMoving = true;
                baseTargetSpeed = fps.runSpeed * modStatus.moveRunSpeedMult;
                break;
            }

            // --- C-2. 方向による補正 ---
            float directionModifier = 1.0f;
            if (hasInput) {
                float forwardWeight = (fps.input.moveForward > 0.0f) ? fps.input.moveForward : std::abs(fps.input.moveForward) * fps.backSpeedModifier;
                float rightWeight = std::abs(fps.input.moveRight) * fps.sideSpeedModifier;
                directionModifier = (forwardWeight + rightWeight) / (std::abs(fps.input.moveForward) + std::abs(fps.input.moveRight));
            }

            // 最終目標速度の算出。OverallSpeedMult は PlayerModifierSystem 側で計算済み
            fps.currenTargetSpeed = baseTargetSpeed * directionModifier;

            // --- D. 速度の補間 ---
            // 加速度もキャッシュを参照
            float finalAcc = fps.acceleration * modStatus.moveAccelMult;

            if (fps.currentSpeed < fps.currenTargetSpeed) {
                fps.currentSpeed = (std::min)(fps.currentSpeed + finalAcc * dt, fps.currenTargetSpeed);
            }
            else if (fps.currentSpeed > fps.currenTargetSpeed) {
                // 減速（定数 20.0f も必要ならコンポーネント化を推奨）
                fps.currentSpeed = (std::max)(fps.currentSpeed - 20.0f * dt, fps.currenTargetSpeed);
            }

            // --- E. 移動ベクトルの算出と座標更新 ---
            XMVECTOR moveDir = XMVectorAdd(XMVectorScale(camForward, fps.input.moveForward), XMVectorScale(camRight, fps.input.moveRight));
            if (XMVectorGetX(XMVector3LengthSq(moveDir)) > 0.001f)
            {
                moveDir = XMVector3Normalize(moveDir);
                XMVECTOR currentPos = XMLoadFloat3(&trans.position);
                XMStoreFloat3(&trans.position, XMVectorAdd(currentPos, XMVectorScale(moveDir, fps.currentSpeed * dt)));
            }
        });
}


REGISTER_LOGIC_SYSTEM(FPSPlayerMoveSystem, Priority::LogicStage::L02_Update); // 状態更新の直後に実行