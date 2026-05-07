#include "AIPerceptionSystem.h"
#include "ECS/Core/CCL_World.h"
#include "Engine/Physics/IPhysicsAPI.h"
#include "Game/Logics/Character/Player/PlayerComponent.h"
#include "Engine/Platform/Logger.h"

#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

#include "SimpleMath.h"
using namespace DirectX::SimpleMath;
using namespace CCL::ECS;

// ===================================================================================
// 【 AI感覚器 (目) : AIPerceptionSystem 】
//
// [ 役割 ]
// 物理世界を観測し、視界内にプレイヤーがいるかを判定してAIの「記憶」と「感情」を上書きする。
//
// [ ECSデータパイプライン ]
// 📥 READ  : TransformComponent      (AI自身の位置・向き、プレイヤーの位置)
// 📥 READ  : AIPerceptionComponent   (AIの視力：距離・角度)
// 📥 READ  : PlayerCarComponent      (※追跡対象のタグとして利用)
// 📤 WRITE : AIMemoryComponent       (最後にプレイヤーを見た座標、怒りレベル)
// 📤 WRITE : AIStateComponent        (発見時に Chase へ状態遷移)
// 📤 WRITE : NavAgentComponent       (発見時に 目標座標をプレイヤー位置へ更新)
//
// [ 内部挙動の直感的な解説 ]
// 毎フレーム、AIの正面ベクトル(Forward)とプレイヤーへの方向ベクトルの内積(Dot)を取り、視界の扇形(Cone)に
// 入っているかを判定します。さらに、Jolt PhysicsのCastRay(光線)を飛ばし、間に壁や障害物がないかを確認します。
// 全ての条件をクリアした場合、「見えた！」と判断して脳のステートを即座に書き換えます。
// ===================================================================================

void AIPerceptionSystem::Update(float dt) {
    if (!_world || !_world->HasResource<std::shared_ptr<IPhysicsAPI>>()) return;
    auto physics = _world->GetResource<std::shared_ptr<IPhysicsAPI>>();
    if (!physics) return;

    // 1. プレイヤーの座標取得
    EntityID playerEntity = InvalidEntityID;
    Vector3 playerPos = Vector3::Zero;

    auto playerView = _world->View<TransformComponent, PlayerComponent>();
    for (auto p : playerView) {
        playerEntity = p;
        playerPos = _world->GetComponent<TransformComponent>(p)->position;
        break;
    }

    if (playerEntity == InvalidEntityID) return;

    // 2. 視覚判定ループ
    ForEachWithID([&](EntityID aiEntity,
        const TransformComponent& transform,
        const AIPerceptionComponent& perception,
        AIMemoryComponent& memory,
        AIStateComponent& state,
        NavAgentComponent& navAgent) {

            if (perception.visionRange <= 0.0f) return;

            Vector3 aiPos = transform.position;
            Vector3 toPlayer = playerPos - aiPos;
            float distToPlayer = toPlayer.Length();
            bool canSeePlayer = false;


             // ① 距離チェック 
             if (distToPlayer <= perception.visionRange && distToPlayer > 0.001f) {
                 // ② 角度チェック (SimpleMath::Matrix を活用) 
                 Matrix worldMat = transform.worldMatrix;
                 Vector3 forward = worldMat.Forward();
                 // 行列から正面方向を直感的に取得 
                 forward.Normalize();
                 Vector3 dirToPlayer = toPlayer;
                 dirToPlayer.Normalize();
                 float dot = forward.Dot(dirToPlayer);
                 float angleDeg = DirectX::XMConvertToDegrees(acosf(std::clamp(dot, -1.0f, 1.0f)));
                 if (angleDeg <= perception.visionAngle) {
                     // ③ 遮蔽物チェック (Jolt Raycast) 
                     // 視点を足元から少し上げる（1.5m）
                     Vector3 rayStart = aiPos + Vector3(0, 1.5f, 0);
                     PhysicsHitInfo hit = physics->RayCast(rayStart, dirToPlayer, perception.visionRange, aiEntity);
                     // ヒットしない、またはプレイヤーに当たった場合 
                     if (!hit.hit || hit.entity == playerEntity || hit.distance >= distToPlayer) {
                         canSeePlayer = true; 
                     }
                 } 
             }


            // ---------------------------------------------------------
            // 🧠 状態遷移と記憶の更新
            // ---------------------------------------------------------
            if (canSeePlayer) {
                // ★修正: 新しい変数名へ変更
                memory.lastKnownPos = playerPos;
                memory.currentAngerLevel = 100.0f; // プレイヤーを直接見たので怒りMAX

                if (state.currentState != AIState::Chase) {
                    state.currentState = AIState::Chase;
                    state.timeInState = 0.0f;
                    CCL_LOG_INFO(LogCategory::Core, "AI[%llu]: Target Spotted!", aiEntity);
                }

                // ターゲットが1m以上動いたら経路を再計算
                float moveDelta = Vector3::Distance(playerPos, navAgent.targetPosition);
                if (moveDelta > 1.0f || navAgent.waypoints.empty()) {
                    navAgent.targetPosition = playerPos;
                    navAgent.isPathRequested = true;
                }
            }
            else {
                // 見失った瞬間、最後に見た場所を調査するモードへ
                if (state.currentState == AIState::Chase) {
                    state.currentState = AIState::Investigate;
                    state.timeInState = 0.0f;

                    // ★修正: 新しい変数名へ変更
                    navAgent.targetPosition = memory.lastKnownPos;
                    navAgent.isPathRequested = true;
                    CCL_LOG_INFO(LogCategory::Core, "AI[%llu]: Target Lost. Investigating...", aiEntity);
                }
            }
        });
}


REGISTER_LOGIC_SYSTEM(AIPerceptionSystem, Priority::LogicStage::L02_AI_Sense)