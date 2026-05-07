#include "ECS/Core/CCL_World.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

#include "TPSPlayerAttackSystem.h"
#include "TPSPlayerComponent.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "Engine/GamePlay/Animation/AnimParametersComponent.h"
#include "Engine/GamePlay/Animation/AnimatorComponent.h"
#include "Game/Logics/Character/Enemy/EnemyTag.h"
#include "Engine/Core/Math/StringHash.h"
#include "Engine/GamePlay/Camera/VirtualCameraComponents.h"
#include "Game/Logics/System/BlackboardComponent.h"
#include "Engine/GamePlay/Transform//Motion/MotionComponent.h"

#include "Engine/GamePlay/Physics/Character/JoltCharacterConfigComponent.h"
#include "Game/Logics/Character/CharacterMovementInputComponent.h"

#include <SimpleMath.h>
using namespace DirectX::SimpleMath;

using namespace DirectX;

void TPSPlayerAttackSystem::Update(float dt)
{
    ForEachWithID([&](CCL::ECS::EntityID id,
        TransformComponent& trans,
        TPSPlayerComponent& tps,
        TPSPlayerStateComponent& state,
        AnimParametersComponent& animPara,
        BlackboardComponent& bb,
        const AnimatorComponent& animator,
        const PlayerStateTag::IsAttackingTag& tag)
{
            auto* s = std::get_if<StateAttack>(&state.activeState);
            if (!s) return;

            auto* moveInput = _world->GetComponent<CharacterMovementInputComponent>(id);
            auto* motion = _world->GetComponent<MotionComponent>(id);

            state.stateTimer += dt;

            // --- 1. 特別なアタック（カウンター等）の判定 ---
            bool isSpecialDash = (s->comboCount == 99);

            if (moveInput && (s->isAirAttack || isSpecialDash))
            {
                moveInput->desiredVelocity.y = 0.0f; // 滞空
            }

            // --- 2. ターゲット検索と回転制御 ---
            CCL::ECS::EntityID targetEnemyID = CCL::ECS::InvalidEntityID;
            float minSearchDist = isSpecialDash ? s->config.lungerange : 5.0f; 

            // エネミー検索（もっとも近い敵を探す）
            auto enemies = _world->View<EnemyTag, TransformComponent>();
            for (auto enemyID : enemies)
            {
                auto* eTrans = _world->GetComponent<TransformComponent>(enemyID);
                if (!eTrans) continue;

                float dist = Vector3::Distance(trans.position, eTrans->position);
                if (dist < minSearchDist) 
                {
                    minSearchDist = dist;
                    targetEnemyID = enemyID;
                }
            }

            // 敵の方を向く処理
            if (targetEnemyID != CCL::ECS::InvalidEntityID)
            {
                auto* targetTrans = _world->GetComponent<TransformComponent>(targetEnemyID);
                if (targetTrans) {
                    XMVECTOR toTarget = XMVectorSubtract(XMLoadFloat3(&targetTrans->position), XMLoadFloat3(&trans.position));
                    XMVECTOR dir = XMVector3Normalize(XMVectorSetY(toTarget, 0.0f));
                    
                    float targetYaw = atan2f(XMVectorGetX(dir), XMVectorGetZ(dir));
                    XMVECTOR targetRot = XMQuaternionRotationRollPitchYaw(0, targetYaw, 0);
                    XMVECTOR currentRot = XMLoadFloat4(&trans.rotation);

                    // カウンター時は即座に、通常時は設定された速度で回転
                    float rotSpeed = isSpecialDash ? 1.0f : s->config.rotationSpeed * dt;
                    XMStoreFloat4(&trans.rotation, XMQuaternionSlerp(currentRot, targetRot, rotSpeed));
                }
            }

            // --- 3. 踏み込み（Lunge）ロジック ---
            float comboAttenuation = isSpecialDash ? 1.0f : std::pow(0.7f, static_cast<float>(s->comboCount));
            float maxLungeDist = s->config.lungerange * comboAttenuation;

            // 敵の手前で止まるための制限
            if (targetEnemyID != CCL::ECS::InvalidEntityID) 
            {
                float stopMargin = 1.2f; // 敵の1.2m手前
                maxLungeDist = (std::min)(maxLungeDist, (std::max)(0.0f, minSearchDist - stopMargin));
            }

            if (motion && s->currentLungeDist < maxLungeDist)
            {
                float remainingDist = maxLungeDist - s->currentLungeDist;
                
                // カウンター時は0.15秒で詰め切る超高速、通常時は指数減衰移動
                float moveStepZ = isSpecialDash 
                    ? (maxLungeDist / 0.15f) * dt 
                    : remainingDist * (1.0f - std::exp(-7.0f * dt));

                if (moveStepZ > remainingDist) moveStepZ = remainingDist;

                if (moveStepZ > 0.001f)
                {
                    XMVECTOR forward = XMVector3Rotate(XMVectorSet(0, 0, 1, 0), XMLoadFloat4(&trans.rotation));
                    XMVECTOR worldDelta = XMVectorScale(forward, moveStepZ);

                    XMVECTOR currentPending = XMLoadFloat3(&motion->pendingMovement);
                    XMStoreFloat3(&motion->pendingMovement, XMVectorAdd(currentPending, worldDelta));

                    s->currentLungeDist += moveStepZ;

                    // ★ MoveSystem 側の通常移動計算に上書きされないようフラグを立てる
                    tps.forceStopMovement = true; 
                }
            }


            // --- 4. 先行入力のバッファリング ---
            if(bb.GetBool("AcceptInput"))
            {
                if (tps.input.isAttackTriggered)
                {
                    s->hasNextComboBuffered = true;
                }
                // ★ 回避キャンセル：バッファリングせず、入力があった瞬間にステートを終了させる
                if (tps.input.isDodgeTriggered)
                {
                    state.isFinished = true;  // StateSystem側でDodgeへ遷移させるトリガー
                    tps.canMove = true;

                    bb.SetBool("AttackNext", false);
                    bb.SetBool("AcceptInput", false);
                    return;
                }
            }

            // --- 5. コンボ遷移判定 ---
            bool shouldTransition = animator.isFinished || bb.GetBool("AttackNext");

            if (shouldTransition)
            {

            // =========================================================
            // 5. コンボ遷移判定 と ステート終了判定の分離
            // =========================================================

            // ① コンボが成立した場合の処理
            // 「入力がバッファされている」かつ「AttackNext許可が出ている」場合のみ実行
                if (s->hasNextComboBuffered && bb.GetBool("AttackNext"))
                {
                    if (s->comboCount < s->config.maxComboCount)
                    {
                        s->comboCount++;
                        s->hasNextComboBuffered = false;
                        state.stateTimer = 0.0f; // タイマーリセットで次の攻撃の踏み込みが開始される
                        bb.SetBool("AttackNext", false);
                        bb.SetBool("AcceptInput", false);

                        std::string animTag = "Attack_" + std::to_string(s->comboCount);
                        animPara.SetTrigger(CCL::Utils::HashString(animTag.c_str()));
                        return;
                    }
                }

                // ② アニメーションが最後まで再生し終わった場合の処理
                // AttackNextがスルーされた（プレイヤーがクリックしなかった）場合は、
                // アニメーションが美しく最後まで再生されるのを待ってから終了する
                if (animator.isFinished)
                {
                    state.isFinished = true;
                    tps.canMove = true;
                    animPara.SetTrigger(CCL::Utils::HashString("AttackEnd"));
                    bb.SetBool("AttackNext", false);
                    bb.SetBool("AcceptInput", false);
                    return;
                }
            }
        });
}

REGISTER_LOGIC_SYSTEM(TPSPlayerAttackSystem, Priority::LogicStage::L02_Update); // 状態更新の直後に実行