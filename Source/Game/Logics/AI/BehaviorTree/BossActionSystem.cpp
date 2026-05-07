/**
 * @file BossActionSystem.cpp
 */
#include "BossActionSystem.h"
#include "Engine/Core/Math/StringHash.h"
#include "Game/Core/SystemPriority.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Logic/AI/BehaviorTree/Data/ActionRegistry.h"
#include <cmath>

using namespace CCL::ECS;
using namespace DirectX::SimpleMath;

// =========================================================
// ★ 角度の差を -180度 ～ 180度 (-PI ~ PI) に丸めるヘルパー関数
// (ファイルの上のほう、あるいは無名名前空間に追加してください)
// =========================================================
namespace {
    float WrapAngle(float angle) {
        while (angle > DirectX::XM_PI) angle -= DirectX::XM_2PI;
        while (angle < -DirectX::XM_PI) angle += DirectX::XM_2PI;
        return angle;
    }
}

void BossActionSystem::Update(float rawDt) {
    static const uint32_t HASH_SPEED = CCL::Utils::HashString("Speed");
    static const uint32_t HASH_ATTACK = CCL::Utils::HashString("Attack");
    static const uint32_t HASH_CHARGE = CCL::Utils::HashString("Charge");

    // ジャンプ制御用のアニメーションパラメータ
    static const uint32_t HASH_JUMP_START = CCL::Utils::HashString("JumpStart");
    static const uint32_t HASH_JUMP_END = CCL::Utils::HashString("JumpEnd");
    static const uint32_t HASH_IS_AIRBORNE = CCL::Utils::HashString("IsAirborne");

    // ★追加: 回避用のアニメーションパラメータ
    static const uint32_t HASH_EVADE_START = CCL::Utils::HashString("EvadeStart");
    static const uint32_t HASH_EVADE_END = CCL::Utils::HashString("EvadeEnd");
    static const uint32_t HASH_IS_EVADING = CCL::Utils::HashString("IsEvading");

    ForEachParallel([this](TransformComponent& trans, AnimParametersComponent& animParams,
        BossActionComponent& bossDef, CharacterMovementInputComponent& moveInput, const BossCommandComponent& cmd, const TimeState& time) {

            // ★各自の時計から dt を算出（これでラムダ内の dt はすべて localDt になる）
            float dt = time.localDt;

            float currentSpeed = 0.0f;

            // 毎フレーム、まずはアクセルを離す（速度ゼロ）
            moveInput.desiredVelocity = Vector3::Zero;
            moveInput.jumpRequested = false;
            // カスタム物理も毎フレームリセットする
            moveInput.customJumpVelocity = 0.0f;
            moveInput.customGravity = 0.0f;
            moveInput.overrideVerticalVelocity = false;


            // [フェーズ1] コマンドの受付と状態更新
            AcceptNewCommand(bossDef, cmd, trans);

            // AIが今「出したい」目標速度
            float targetSpeed = 0.0f;

            // ---------------------------------------------------------
            // [フェーズ2] 状態に応じた物理移動の実行
            // ---------------------------------------------------------
            switch (bossDef.currentState) {
            case BossActionState::Move:
                ProcessMove(bossDef, moveInput, trans, cmd, dt, targetSpeed);
                break;

            case BossActionState::Charge:
                ProcessCharge(bossDef, moveInput, trans, cmd, dt, targetSpeed); // cmd と dt を渡す
                break;

            case BossActionState::JumpAttack:
                ProcessJumpAttack(bossDef, moveInput, trans, dt, targetSpeed);
                bossDef.currentMoveSpeed = 0.0f; // 大技中は慣性をリセット
                break;

            case BossActionState::Melee:
            case BossActionState::Evade:
                // 複雑な計算を関数に切り出し！
                ProcessEvade(bossDef, dt, targetSpeed);
                break;
            case BossActionState::Flinch:
                targetSpeed = 0.0f;
                bossDef.currentMoveSpeed = 0.0f; // 慣性を完全に殺し、その場でピタッと止める
                break;
            case BossActionState::None:
                ProcessIdle(bossDef, trans, cmd, dt);
                targetSpeed = 0.0f; // 速度はゼロのまま
                break;
            default:
                // 移動を伴わない状態は速度ゼロのまま
                break;
            }

            // =========================================================
            //  慣性（Inertia）の適用（足滑り防止の要！）
            // =========================================================
            if (bossDef.currentState == BossActionState::Move ||
                bossDef.currentState == BossActionState::Charge ||
                bossDef.currentState == BossActionState::None ||
                bossDef.currentState == BossActionState::Melee ||
                bossDef.currentState == BossActionState::Evade) 
            {
                // チャージ時は急加速・急停止させたいため、加速度を2倍にブーストする
                float currentAccel = (bossDef.currentState == BossActionState::Charge) ? bossDef.acceleration * 2.5f : bossDef.acceleration;
                float currentDecel = (bossDef.currentState == BossActionState::Charge) ? bossDef.deceleration * 2.0f : bossDef.deceleration;

                // 目標速度に向かって、実際の速度を滑らかに加減速させる
                if (targetSpeed > bossDef.currentMoveSpeed) {
                    bossDef.currentMoveSpeed += currentAccel * dt;
                    if (bossDef.currentMoveSpeed > targetSpeed) bossDef.currentMoveSpeed = targetSpeed;
                }
                else if (targetSpeed < bossDef.currentMoveSpeed) {
                    bossDef.currentMoveSpeed -= currentDecel * dt;
                    if (bossDef.currentMoveSpeed < targetSpeed) bossDef.currentMoveSpeed = targetSpeed;
                }

                // ★追加: 進行方向の決定（Charge時は一度決めたアクション方向、それ以外は動的な移動方向）
                Vector3 dir = (bossDef.currentState == BossActionState::Charge) ? bossDef.actionDirection : bossDef.moveDirection;

                // 加減速した「現実の速度」を物理エンジンに渡す
                if (bossDef.currentMoveSpeed > 0.01f) {
                    moveInput.desiredVelocity = dir * bossDef.currentMoveSpeed;
                }
                else {
                    moveInput.desiredVelocity = Vector3::Zero;
                }

                // アニメーションに渡す速度も、この「滑らかな物理速度」にする
                currentSpeed = bossDef.currentMoveSpeed;
            }

            // ---------------------------------------------------------
            // [フェーズ3] アクションタイマーの消化
            // ---------------------------------------------------------

            // 減算する前の時間を記憶しておく（トリガー検知用）
            float previousTimer = bossDef.actionTimer;

            // Flinch 中もタイマーを減算し、ゼロになったら None(待機) に戻るように追加
            if (bossDef.currentState == BossActionState::Melee ||
                bossDef.currentState == BossActionState::Charge ||
                bossDef.currentState == BossActionState::JumpAttack ||
                bossDef.currentState == BossActionState::Evade ||
                bossDef.currentState == BossActionState::Flinch) {
                bossDef.actionTimer -= dt;
                if (bossDef.actionTimer <= 0.0f) {
                    bossDef.currentState = BossActionState::None;
                }
            }

            // ---------------------------------------------------------
            // [フェーズ4] アニメーターへのデータ送信（定数を使用）
            // ---------------------------------------------------------
            animParams.SetFloat(HASH_SPEED, currentSpeed);

            if (bossDef.currentState == BossActionState::Melee) {
                if (previousTimer >= BossTimings::Melee_AnimTrigger && bossDef.actionTimer < BossTimings::Melee_AnimTrigger)
                    animParams.SetTrigger(HASH_ATTACK);
            }
            else if (bossDef.currentState == BossActionState::Charge) {
                if (previousTimer >= BossTimings::Charge_AnimTrigger && bossDef.actionTimer < BossTimings::Charge_AnimTrigger)
                    animParams.SetTrigger(HASH_CHARGE);
            }
            else if (bossDef.currentState == BossActionState::JumpAttack) {
                // 1. 開始トリガー（3.0sの瞬間）：ここで「しゃがみ（タメ）」のアニメーションを再生
                if (previousTimer >= BossTimings::JumpAttack_StartTrigger && bossDef.actionTimer < BossTimings::JumpAttack_StartTrigger) {
                    animParams.SetTrigger(HASH_JUMP_START);
                }

                // 2. 滞空フラグ（2.5s ～ 1.0sの間）：足が離れたら滞空モーションにする
                bool isAirborne = (bossDef.actionTimer <= BossTimings::JumpAttack_Liftoff && bossDef.actionTimer > BossTimings::JumpAttack_Land);
                animParams.SetBool(HASH_IS_AIRBORNE, isAirborne);

                // 3. 着地トリガー（1.0sの瞬間）：ここで「ドスーン！（着地）」のアニメーションを再生
                if (previousTimer >= BossTimings::JumpAttack_Land && bossDef.actionTimer < BossTimings::JumpAttack_Land) {
                    animParams.SetTrigger(HASH_JUMP_END);
                }
            }
            else if (bossDef.currentState == BossActionState::Evade) {

                // 1. 開始トリガー（1.2sの瞬間）：地面を蹴る Start アニメーション
                if (previousTimer >= BossTimings::Evade_Total && bossDef.actionTimer < BossTimings::Evade_Total) {
                    animParams.SetTrigger(HASH_EVADE_START);
                }

                // 2. 滞空フラグ（1.0s 〜 0.2sの間）：空中で下がっている Loop アニメーション
                bool isEvading = (bossDef.actionTimer <= BossTimings::Evade_Airborne && bossDef.actionTimer > BossTimings::Evade_Land);
                animParams.SetBool(HASH_IS_EVADING, isEvading);

                // 3. 着地トリガー（0.2sの瞬間）：着地してブレーキをかける End アニメーション
                if (previousTimer >= BossTimings::Evade_Land && bossDef.actionTimer < BossTimings::Evade_Land) {
                    animParams.SetTrigger(HASH_EVADE_END);
                }
            }
        });
}


// ============================================================================
// 各アクションの具体的な処理（ヘルパー関数）
// ============================================================================

void BossActionSystem::AcceptNewCommand(BossActionComponent& bossDef, const BossCommandComponent& cmd, TransformComponent& trans) {
    // =======================================================
    // ★修正: 大技の硬直中、および「怯み(Flinch)中」はAIからの新しい命令を完全に無視する
    // =======================================================
    if (bossDef.currentState != BossActionState::None && bossDef.currentState != BossActionState::Move) {
        return; // 硬直中・怯み中ならAIの命令は届かない（脊髄反射優先）
    }

    bossDef.currentState = BossActionState::None;

    // 【修正後】AIから渡されたActionIDをそのままダイレクトに評価する
    switch (cmd.currentActionId) {

    case AI::A_MeleeAttack: {
        bossDef.currentState = BossActionState::Melee;
        // =======================================================
        // ★修正: 1.0f ではなく、BossTimings 定数を代入！
        // =======================================================
        bossDef.actionTimer = BossTimings::Melee_Duration;

        const auto* playerTrans = _world->GetComponent<TransformComponent>(cmd.targetPlayerId);
        if (playerTrans) {
            Vector3 dir = playerTrans->position - trans.position;
            dir.y = 0.0f;
            if (dir.LengthSquared() > 0.01f) {
                dir.Normalize();
                float targetYaw = atan2f(dir.x, dir.z);
                trans.rotation = Quaternion::CreateFromAxisAngle(Vector3::UnitY, targetYaw);
                trans.isDirty = true;
            }
        }
        break;
    }
    case AI::A_ChargeAttack: {
        bossDef.currentState = BossActionState::Charge;
        bossDef.actionTimer = BossTimings::Charge_Total;
        bossDef.jumpRequested = true;
        // ★ここでは方向計算をしない（ProcessCharge内でタメ中に毎フレーム計算するため）
        break;
    }
    case AI::A_Move: {
        bossDef.currentState = BossActionState::Move;
        break;
    }
    case AI::A_JumpAttack: {
        bossDef.currentState = BossActionState::JumpAttack;
        bossDef.actionTimer = BossTimings::JumpAttack_Total; // (ここは既に定数になっていましたわね)
        // ★修正: ここではまだジャンプフラグを立てない！（タメ中に飛んでしまうため）
        bossDef.jumpRequested = true; // ← このフラグは ProcessJumpAttack でタイミングを見て消費します

        const auto* playerTrans = _world->GetComponent<TransformComponent>(cmd.targetPlayerId);
        if (playerTrans) {
            Vector3 dir = playerTrans->position - trans.position;
            dir.y = 0.0f;
            float distance = dir.Length();

            if (distance > 0.01f) {
                dir.Normalize();
                bossDef.actionDirection = dir;
                bossDef.dynamicJumpSpeed = distance / 1.5f;

                float targetYaw = atan2f(dir.x, dir.z);
                trans.rotation = Quaternion::CreateFromAxisAngle(Vector3::UnitY, targetYaw);
                trans.isDirty = true;
            }
        }
        break;
    }
    case AI::A_EvadeBackward: {
        bossDef.currentState = BossActionState::Evade;
        // 新しい Total タイマーを使う
        bossDef.actionTimer = BossTimings::Evade_Total;

        const auto* playerTrans = _world->GetComponent<TransformComponent>(cmd.targetPlayerId);
        if (playerTrans) {
            // =========================================================
            // ★ 逆ベクトルの計算: 「自分の位置 - プレイヤーの位置」
            // =========================================================
            Vector3 dir = trans.position - playerTrans->position;
            dir.y = 0.0f;
            if (dir.LengthSquared() > 0.01f) {
                dir.Normalize();
                bossDef.actionDirection = dir; // 逃げる方向を記憶

                // ※ボスの体（Rotation）はプレイヤーの方を向けたまま（ガン見しながら）後ろに下がる
                float targetYaw = atan2f(-dir.x, -dir.z);
                trans.rotation = Quaternion::CreateFromAxisAngle(Vector3::UnitY, targetYaw);
                trans.isDirty = true;
            }
        }
        break;
    }
    default:
        break;
    }
}

void BossActionSystem::ProcessMove(BossActionComponent& bossDef, CharacterMovementInputComponent& moveInput, TransformComponent& trans, const BossCommandComponent& cmd, float dt, float& outTargetSpeed) {
    const auto* playerTrans = _world->GetComponent<TransformComponent>(cmd.targetPlayerId);
    if (!playerTrans) return;

    Vector3 moveDir = playerTrans->position - trans.position;
    moveDir.y = 0.0f;

    if (moveDir.LengthSquared() > 0.01f) {
        moveDir.Normalize();
        outTargetSpeed = bossDef.walkSpeed; // ★目標速度(TargetSpeed)を設定

        bossDef.moveDirection = moveDir;

        // 1. 目標の角度 (Target Yaw)
        float targetYaw = atan2f(moveDir.x, moveDir.z);

        // 2. 現在の角度 (Current Yaw) を取得
        Vector3 currentForward = Vector3::Transform(Vector3::UnitZ, trans.rotation);
        float currentYaw = atan2f(currentForward.x, currentForward.z);

        // 3. 最短の回転方向を計算 (-180度 ～ 180度 に丸める)
        float diff = WrapAngle(targetYaw - currentYaw);

        // 4. 一定速度で角度を近づける
        float speed = (bossDef.turnSpeed <= 0.0f) ? 10.0f : bossDef.turnSpeed;
        float newYaw = currentYaw + diff * (speed * dt);

        // 5. 計算した新しい角度から、純粋なY軸回転のクォータニオンを再構築
        trans.rotation = Quaternion::CreateFromAxisAngle(Vector3::UnitY, newYaw);
        trans.isDirty = true;
    }
}

void BossActionSystem::ProcessCharge(BossActionComponent& bossDef, CharacterMovementInputComponent& moveInput, TransformComponent& trans, const BossCommandComponent& cmd, float dt, float& outTargetSpeed) {

    // [フェーズ1] 浮遊・タメ (2.0s ～ 1.5s)
    if (bossDef.actionTimer > BossTimings::Charge_DashStart) {
        if (bossDef.jumpRequested) {
            moveInput.jumpRequested = true;
            moveInput.customJumpVelocity = bossDef.chargeHoverVelocity;
            bossDef.jumpRequested = false;
        }
        moveInput.customGravity = 15.0f;
        outTargetSpeed = 0.0f;

        // ★ タメ中は毎フレーム、プレイヤーへ向けて突進ベクトル(3D)を更新し続ける
        const auto* playerTrans = _world->GetComponent<TransformComponent>(cmd.targetPlayerId);
        if (playerTrans) {
            Vector3 targetPos = playerTrans->position;

            // ★修正1: 胸元(1.0f)ではなく、足元より少し下を狙って鋭く突き刺さるようにする！
            targetPos.y -= 0.5f;

            Vector3 dir = targetPos - trans.position;

            // ★修正2: 究極の安全装置（斜め上への飛行禁止）
            // ボスがプレイヤーより低い位置にいたとしても、絶対に空へは飛ばさない！
            // Y方向のベクトルが上向き(プラス)になったら、強制的に「わずかに下向き」に矯正する
            if (dir.y > 0.0f) {
                dir.y = -0.1f;
            }

            if (dir.LengthSquared() > 0.01f) {
                dir.Normalize();
                bossDef.actionDirection = dir; // Y軸込みの斜め下ベクトルをロックオン！

                // 振り向きは水平面のみで行う（体が縦に回転しないように）
                float targetYaw = atan2f(dir.x, dir.z);
                Vector3 currentForward = Vector3::Transform(Vector3::UnitZ, trans.rotation);
                float currentYaw = atan2f(currentForward.x, currentForward.z);

                float diff = WrapAngle(targetYaw - currentYaw);
                float speed = (bossDef.turnSpeed <= 0.0f) ? 10.0f : bossDef.turnSpeed;
                float newYaw = currentYaw + diff * (speed * dt);
                trans.rotation = Quaternion::CreateFromAxisAngle(Vector3::UnitY, newYaw);
                trans.isDirty = true;
            }
        }
    }
    // [フェーズ2] 空中ダッシュ突進 (1.5s ～ 0.5s)
    else if (bossDef.actionTimer > BossTimings::Charge_BrakeTime) {
        // 浮遊が終わった瞬間に方向の更新が止まり、記憶した方向へ重力無視で一直線に突進する！
        moveInput.overrideVerticalVelocity = true; // Y軸も完全に速度で上書き

        // ====================================================================
        // ★ バックステップ(Evade)の知見を活かした、重みのある突進カーブ
        // ====================================================================
        float dashDuration = BossTimings::Charge_DashStart - BossTimings::Charge_BrakeTime;
        // 進行度 (1.0: 突進開始直後 -> 0.0: ブレーキ寸前)
        float progress = (bossDef.actionTimer - BossTimings::Charge_BrakeTime) / dashDuration;

        // 突進は完全に停止させず、最高速から70%程度の速度までしか減衰させない（貫通力を残す）
        // 最初は100%のスピード、後半は70%のスピードになるカーブ
        float minSpeedRatio = 0.7f;
        outTargetSpeed = bossDef.chargeSpeed * (minSpeedRatio + (1.0f - minSpeedRatio) * progress);

        // 空間を蹴り飛ばす爆発的な初動（Lerp）
        float kickPower = 15.0f; // 踏み込みの力（大きいほど瞬時に最高速になる）
        if (bossDef.currentMoveSpeed < outTargetSpeed) {
            bossDef.currentMoveSpeed = std::lerp(bossDef.currentMoveSpeed, outTargetSpeed, kickPower * dt);
        }
    }
    // [フェーズ3] ブレーキ・硬直 (0.5s ～ 0.0s)
    else {
        outTargetSpeed = 0.0f;

        // ★ ブレーキ時もLerpを使うと「ズザザッ」と滑る重厚な停止になります
        float brakePower = 8.0f;
        bossDef.currentMoveSpeed = std::lerp(bossDef.currentMoveSpeed, 0.0f, brakePower * dt);
    }
}


void BossActionSystem::ProcessJumpAttack(BossActionComponent& bossDef, CharacterMovementInputComponent& moveInput, TransformComponent& trans, float dt, float& outTargetSpeed) {
    // [フェーズ1] 地上でのタメ・予兆 (3.0s ～ 2.5s)
    if (bossDef.actionTimer > BossTimings::JumpAttack_Liftoff) {
        outTargetSpeed = 0.0f;
    }
    // [フェーズ2] 飛翔・空中移動 (2.5s ～ 1.0s)
    else if (bossDef.actionTimer > BossTimings::JumpAttack_Land) {

        // 滞空時間 (T) = 1.5秒
        float hangTime = BossTimings::JumpAttack_Liftoff - BossTimings::JumpAttack_Land;
        float H = bossDef.jumpAttackApexHeight; // 目標とする山の高さ(m)

        // ★ アクションゲーム特有の「高さを指定して重力と初速を逆算する」計算式
        float requiredGravity = (8.0f * H) / (hangTime * hangTime);
        float requiredJumpVel = (4.0f * H) / hangTime;

        if (bossDef.jumpRequested) {
            moveInput.jumpRequested = true;
            moveInput.customJumpVelocity = requiredJumpVel;
            bossDef.jumpRequested = false;
        }

        // 飛んでいる間はずっと専用の重力を物理エンジンに要求する
        moveInput.customGravity = requiredGravity;

        outTargetSpeed = bossDef.dynamicJumpSpeed;
        moveInput.desiredVelocity = bossDef.actionDirection * outTargetSpeed;
    }
    // [フェーズ3] 着地・硬直 (1.0s ～ 0.0s)
    else {
        outTargetSpeed = 0.0f;
    }
}


void BossActionSystem::ProcessEvade(BossActionComponent& bossDef, float dt, float& outTargetSpeed) {
    // Start(タメ)とEnd(着地)の最中は動かず、Loop(空中)の期間だけ物理的に後ろへ下がる
    if (bossDef.actionTimer <= BossTimings::Evade_Airborne && bossDef.actionTimer > BossTimings::Evade_Land) {

        float airborneDuration = BossTimings::Evade_Airborne - BossTimings::Evade_Land;
        float progress = (bossDef.actionTimer - BossTimings::Evade_Land) / airborneDuration;

        // 2次関数でカーブを作る (1.0 -> 0.0)
        outTargetSpeed = bossDef.evadeSpeed * (progress * progress);

        // ====================================================================
        // ★ 究極の自然な回避カーブ (Lerpによる初速のブレンド)
        // ====================================================================
        // いきなりトップギアに入れるとワープして見えてしまうため、
        // 「現在の速度」と「目標の最高速度」を非常に短い時間（Lerp）で滑らかに繋ぎます。
        // これにより、地面を「グッ、ダン！」と蹴る重みのある初動が生まれます。

        float kickPower = 15.0f; // 地面を蹴る力（大きいほど素早く最高速に到達する）

        if (bossDef.currentMoveSpeed < outTargetSpeed) {
            // 現在速度を、最高速に向かって急速に引っ張り上げる
            bossDef.currentMoveSpeed = std::lerp(bossDef.currentMoveSpeed, outTargetSpeed, kickPower * dt);
        }

        bossDef.moveDirection = bossDef.actionDirection;
    }
    else {
        outTargetSpeed = 0.0f; // 地面に足がついている時は急ブレーキ
        bossDef.moveDirection = bossDef.actionDirection;
    }
}


void BossActionSystem::ProcessIdle(BossActionComponent& bossDef, TransformComponent& trans, const BossCommandComponent& cmd, float dt) {
    const auto* playerTrans = _world->GetComponent<TransformComponent>(cmd.targetPlayerId);
    if (!playerTrans) return;

    Vector3 dirToPlayer = playerTrans->position - trans.position;
    dirToPlayer.y = 0.0f; // 水平方向のみで計算

    // プレイヤーと完全に重なっていない場合のみ計算
    if (dirToPlayer.LengthSquared() > 0.01f) {

        // 1. プレイヤーの方向の角度 (Target Yaw) を計算
        float targetYaw = atan2f(dirToPlayer.x, dirToPlayer.z);

        // 2. 現在の自分の角度 (Current Yaw) を取得
        Vector3 currentForward = Vector3::Transform(Vector3::UnitZ, trans.rotation);
        float currentYaw = atan2f(currentForward.x, currentForward.z);

        // 3. 最短の回転方向を計算 (-180度 ～ 180度 に丸める)
        float diff = WrapAngle(targetYaw - currentYaw);

        // ====================================================================
        // ★演出の極意：待機中の旋回は、歩行中(turnSpeed)の半分にする
        // すぐにビュッと向くのではなく、重たい体でジリジリと獲物を睨みつける演出
        // ====================================================================
        float idleTurnSpeed = (bossDef.turnSpeed <= 0.0f) ? 10.0f : (bossDef.turnSpeed * 0.5f);

        // 4. 一定速度で角度を近づける
        float newYaw = currentYaw + diff * (idleTurnSpeed * dt);

        // 5. 新しい角度をクォータニオンとして保存
        trans.rotation = Quaternion::CreateFromAxisAngle(Vector3::UnitY, newYaw);
        trans.isDirty = true;
    }
}


REGISTER_LOGIC_SYSTEM(BossActionSystem, Priority::LogicStage::L04_Physics);