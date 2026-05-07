#include "TestEnemyActionSystem.h"
#include "Engine/Core/Math/StringHash.h"
#include "Game/Core/SystemPriority.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include <cmath>

using namespace CCL::ECS;
using namespace DirectX::SimpleMath;

void TestEnemyActionSystem::Update(float dt) {
    static const uint32_t HASH_SPEED = CCL::Utils::HashString("Speed");
    static const uint32_t HASH_ATTACK = CCL::Utils::HashString("Attack");
    static const uint32_t HASH_CHARGE = CCL::Utils::HashString("Charge");

    ForEachParallel([dt](TransformComponent& trans, AnimParametersComponent& animParams, TestEnemyActionComponent& enemyDef, const BossCommandComponent& cmd) {
        float currentSpeed = 0.0f;
        bool triggerAttack = false;
        bool triggerCharge = false;

        // ---------------------------------------------------------
        // 1. コマンドの受付（アクションをしていない時だけAIの言うことを聞く）
        // ---------------------------------------------------------
        if (enemyDef.currentState == EnemyActionState::None) {
            if (cmd.requestMeleeAttack) {
                enemyDef.currentState = EnemyActionState::Melee;
                enemyDef.attackTimer = 1.0f; // 1秒硬直
                triggerAttack = true;
            }
            else if (cmd.requestCharge) {
                enemyDef.currentState = EnemyActionState::Charge;
                enemyDef.attackTimer = 1.5f; // 1.5秒突進
                triggerCharge = true;
            }
        }

        // ---------------------------------------------------------
        // 2. 状態の継続処理（タイマーが減っている間、毎フレーム呼ばれる）
        // ---------------------------------------------------------
        if (enemyDef.currentState == EnemyActionState::Charge) {
            // ★ 突進中は毎フレーム前進させ、速度を最大に保つ
            Vector3 forward = Vector3::Transform(Vector3::UnitZ, trans.rotation);
            trans.position = trans.position + forward * enemyDef.chargeSpeed * dt;
            currentSpeed = enemyDef.chargeSpeed;
        }
        else if (enemyDef.currentState == EnemyActionState::Melee) {
            // ★ 攻撃中は立ち止まる
            currentSpeed = 0.0f;
        }
        else if (enemyDef.currentState == EnemyActionState::None && cmd.requestMove) {
            // ★ ウロウロ移動処理
            enemyDef.moveTimer += dt;
            Vector3 moveDir = Vector3(sinf(enemyDef.moveTimer * 2.0f), 0.0f, cosf(enemyDef.moveTimer * 1.0f));

            if (moveDir.LengthSquared() > 0.01f) {
                moveDir.Normalize();
                currentSpeed = enemyDef.walkSpeed;
                trans.position = trans.position + moveDir * currentSpeed * dt;

                float targetYaw = atan2f(moveDir.x, moveDir.z);
                Quaternion targetRot = Quaternion::CreateFromAxisAngle(Vector3::UnitY, targetYaw);
                trans.rotation = Quaternion::Slerp(trans.rotation, targetRot, enemyDef.turnSpeed * dt);
            }
        }

        // ---------------------------------------------------------
        // 3. タイマーの減算と状態の終了
        // ---------------------------------------------------------
        if (enemyDef.currentState != EnemyActionState::None) {
            enemyDef.attackTimer -= dt;
            if (enemyDef.attackTimer <= 0.0f) {
                enemyDef.currentState = EnemyActionState::None; // アクション終了、待機へ
            }
        }

        // ---------------------------------------------------------
        // 4. アニメーターへの送信
        // ---------------------------------------------------------
        animParams.SetFloat(HASH_SPEED, currentSpeed);
        if (triggerAttack) animParams.SetTrigger(HASH_ATTACK);
        if (triggerCharge) animParams.SetTrigger(HASH_CHARGE);
        });
}

// ★ 規約違反を修正：L02_Updateの確実に後に実行されるステージを指定
REGISTER_LOGIC_SYSTEM(TestEnemyActionSystem, Priority::LogicStage::L02_Update);