
#include "PlayerViewSystem.h"
#include "PlayerViewComponent.h"
#include "TPSPlayerComponent.h"

#include "../PlayerStateComponent.h"
#include "ECS/Core/CCL_World.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

#include "Game/Logic/Combat/StaminaComponent.h"
#include "Engine/GamePlay/Camera/VirtualCameraComponents.h"
#include "Game/Logic/System/Modifier/ModifierComponent.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "Game/Logic/Character/Enemy/EnemyTag.h"

#include <vector>

using namespace CCL::ECS;
using namespace DirectX;

void PlayerViewSystem::Update(float dt)
{
    // シーン内のすべてのカメラ候補を取得
    auto cameraEntities = _world->View<VirtualCamera, CameraBodyTPS>();

    ForEachWithID([&](EntityID playerID,
        PlayerViewComponent& view,
        TPSPlayerComponent& tps,
        const TPSPlayerStateComponent& state,
        const StaminaComponent& stam,
        const ModifierStatusComponent& modStatus)
        {
            // --- 1. FOV基本計算 (回避等の演出用) ---
            float baseFOV = view.baseFOV;
            bool isDodging = _world->HasComponent<PlayerStateTag::IsDashingTag>(playerID);
            float fovOffset = isDodging ? view.dodgeUpFov : 0.0f;
            float targetFOV = baseFOV + fovOffset + modStatus.viewBaseFOVAdd;

            float lerpFactor = 1.0f - std::exp(-view.fovInterpSpeed * dt);
            view.currentFOV = std::lerp(view.currentFOV, targetFOV, lerpFactor);

            // --- 2. カメラごとの更新処理 ---
            for (EntityID camID : cameraEntities)
            {
                auto* tpsBody = _world->GetComponent<CameraBodyTPS>(camID);

                // このカメラが操作中のプレイヤーを追跡している場合のみ
                if (tpsBody && tpsBody->targetEntity == playerID)
                {
                    // FOV反映
                    if (auto* vcam = _world->GetComponent<VirtualCamera>(camID)) {
                        vcam->fov = view.currentFOV;
                    }

                    // --- 3. ロックオン制御 (CameraLockOn) ---
                    if (auto* lockOn = _world->GetComponent<CameraLockOn>(camID))
                    {
                        // A. ロックオンの開始 / 解除
                        if (tps.input.isLockOnTriggered)
                        {
                            if (lockOn->targetEntity == CCL::ECS::InvalidEntityID) {
                                // 新規ターゲット検索

                                lockOn->targetEntity = FindClosestEnemy(playerID, lockOn->maxDistance);
                            }
                            else {
                                // 既にロックオン中なら解除
                                lockOn->targetEntity = CCL::ECS::InvalidEntityID;
                            }
                        }

                        // B. ロックオン対象の切り替え (isTargeting中にDecideなど、または別入力)
                        // ここでは「ロックオン中にもう一度トリガー」や「専用ボタン」を想定
                        // 例として、特定の入力（LockOnChenge相当）があれば次を探す
                        if (tps.input.isLockOnChengeTriggered && lockOn->targetEntity != CCL::ECS::InvalidEntityID)
                        {
                            lockOn->targetEntity = FindClosestEnemy(playerID, lockOn->maxDistance,lockOn->targetEntity);
                        }

                        // C. ターゲットの生存・有効性チェック
                        if (lockOn->targetEntity != CCL::ECS::InvalidEntityID)
                        {
                            if (!_world->IsEntityValid(lockOn->targetEntity)) {
                                lockOn->targetEntity = CCL::ECS::InvalidEntityID;
                            }
                        }
                    }
                    break; // 1プレイヤーにつき1メインカメラを想定
                }
            }
        });
}

CCL::ECS::EntityID PlayerViewSystem::FindClosestEnemy(CCL::ECS::EntityID playerID, float minSearchDist, CCL::ECS::EntityID currentTarget)
{
    auto* pTrans = _world->GetComponent<TransformComponent>(playerID);
    if (!pTrans) return CCL::ECS::InvalidEntityID;

    CCL::ECS::EntityID closestID = CCL::ECS::InvalidEntityID;
     // ロックオン可能な最大距離
    DirectX::XMVECTOR pPos = DirectX::XMLoadFloat3(&pTrans->position);

    // EnemyTagを持つエンティティを走査
    auto enemies = _world->View<EnemyTag, TransformComponent>();
    for (auto id : enemies)
    {
        // 切り替え時は現在のターゲットを除外
        if (id == currentTarget) continue;

        auto eTrans = _world->GetComponent<TransformComponent>(id);
        DirectX::XMVECTOR ePos = DirectX::XMLoadFloat3(&eTrans->position);
        float dist = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(ePos, pPos)));

        if (dist < minSearchDist)
        {
            minSearchDist = dist;
            closestID = id;
        }
    }
    return closestID;
}

// システムの登録
REGISTER_LOGIC_SYSTEM(PlayerViewSystem, Priority::LogicStage::L02_Update);