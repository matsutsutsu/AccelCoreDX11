#include "FPSPlayerViewSystem.h"
#include "ECS/Core/CCL_World.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

#include "FPSPlayerComponent.h"
#include "FPSPlayerViewComponent.h"
#include "Game/Logics/Combat/StaminaComponent.h"
#include "Engine/GamePlay/Camera/VirtualCameraComponents.h"
#include "Game/Logics/System/Modifier/ModifierComponent.h"

#include <vector>

using namespace CCL::ECS;
using namespace DirectX;

void FPSPlayerViewSystem::Update(float dt)
{
    auto cameraEntities = _world->View<VirtualCamera, CameraBodyFPS>();

    ForEachWithID([&](EntityID playerID,
        const FPSPlayerComponent& fps,
        const StaminaComponent& stam,
        const ModifierStatusComponent& modStatus, // キャッシュを参照
        FPSPlayerViewComponent& view)
        {
            // --- 1. 目標FOVの決定 ---
            float baseFOV = view.baseFOV;
            if (fps.currentState == PlayerState::Running && !stam.isFatigued) {
                baseFOV = view.runFOV;
            }

            // ModifierStatus に計算済みの View_BaseFOVAdd (加算) を適用
            // 疲労によるトンネルビジョンなどが status.viewBaseFOVAdd に含まれる
            float targetFOV = baseFOV + modStatus.viewBaseFOVAdd;

            // FOV補間
            view.currentFOV += (targetFOV - view.currentFOV) * view.fovInterpSpeed * dt;

            // FPSPlayerViewSystem.cpp 内の switch文付近
            float speedBase = 0.0f;
            float amountBase = 0.0f;
            float tiltBase = 0.0f;
            // --- 3. Modifier による演出の反映 ---
            float finalSpeedMult = 1.0f;
            float finalAmountMult = 1.0f;
            float stateTiltBase = 0.0f; // その状態が持つ本来の傾き
            float stateTiltMult = 1.0f; // その状態の傾きに対する倍率

            switch (fps.currentState) {
            case PlayerState::Idle:
                speedBase = view.idleBobSpeed;
                amountBase = view.idleBobAmount;
                finalSpeedMult = modStatus.viewIdleBobSpeedMult;
                finalAmountMult = modStatus.viewIdleBobAmountMult;
                break;
            case PlayerState::Walking:
                speedBase = view.walkBobSpeed;
                amountBase = view.walkBobAmount;
                finalSpeedMult = modStatus.viewWalkBobSpeedMult;
                finalAmountMult = modStatus.viewWalkBobAmountMult;
                break;
            case PlayerState::Running:
                speedBase = view.runBobSpeed;
                amountBase = view.runBobAmount;
                stateTiltBase = view.runTiltAmount; // 走行時のみ基本の傾きがある
                finalSpeedMult = modStatus.viewRunBobSpeedMult;
                finalAmountMult = modStatus.viewRunBobAmountMult;
                stateTiltMult = modStatus.viewRunTiltAmountMult;
                break;
            }

            float finalSpeed = speedBase * finalSpeedMult;
            float finalAmount = amountBase * finalAmountMult;

            view.bobTimer += finalSpeed * dt;

            // --- 4. 揺れ・傾きの計算 ---
            float bobY = std::sin(view.bobTimer) * finalAmount;
            float bobX = std::cos(view.bobTimer * 0.5f) * (finalAmount * 0.6f);

            // --- 疲労モードの演出（ここが肝） ---
            // 通常の走行傾き(stateTiltBase * stateTiltMult) に加えて、
            // 疲労などで強制される「フラフラ値」を合算する。
            // modStatus.viewFatigueTiltMult が 1.0 なら、停止中でも走行時並みの傾きが発生する
            float totalTilt = (stateTiltBase * stateTiltMult) + (view.runTiltAmount * modStatus.viewFatigueTiltMult);

            // ロール(Roll)の計算：totalTilt が 0 でなければ、呼吸や歩行の周期に合わせてカメラが傾く
            float rollTilt = std::cos(view.bobTimer * 0.5f) * totalTilt;

            // --- 5. 結果の反映 (カメラへの適用) ---
            for (EntityID camID : cameraEntities) {
                auto* fpsBody = _world->GetComponent<CameraBodyFPS>(camID);
                if (fpsBody && fpsBody->targetEntity == playerID) {
                    auto* vcam = _world->GetComponent<VirtualCamera>(camID);
                    if (vcam) {
                        vcam->fov = view.currentFOV;

                        // 座標オフセット
                        fpsBody->eyeOffset.x = view.baseEyeOffset.x + bobX;
                        fpsBody->eyeOffset.y = view.baseEyeOffset.y + bobY;
                        fpsBody->eyeOffset.z = view.baseEyeOffset.z;

                        // 回転オフセット (Roll)
                        XMVECTOR tiltQuat = XMQuaternionRotationRollPitchYaw(0, 0, rollTilt);
                        XMStoreFloat4(&fpsBody->rotationOffset, tiltQuat);
                    }
                    break;
                }
            }
        });
}

// 優先度は LogicStage の中盤に配置
REGISTER_LOGIC_SYSTEM(FPSPlayerViewSystem, Priority::LogicStage::L02_Update);