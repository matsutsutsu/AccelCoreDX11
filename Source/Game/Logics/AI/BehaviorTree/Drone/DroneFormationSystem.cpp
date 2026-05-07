/**
 * @file DroneFormationSystem.cpp
 * @brief ドローンフォーメーションシステムの実装
 */
#include "DroneFormationSystem.h"
#include "ECS/Core/CCL_World.h"
#include "Game/Core/SystemPriority.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include <cmath>

using namespace CCL::ECS;
using namespace DirectX::SimpleMath;

// ============================================================================
// メインの更新処理
// ============================================================================
void DroneFormationSystem::Update(float dt) {

    // システム全体での経過時間を蓄積
    m_currentTime += dt;
    float currentTime = m_currentTime;

    // 超並列処理による各ドローンの更新
    ForEachParallel([this, dt, currentTime](DroneComponent& drone) {

        // 1. 親と指示の取得（フェイルセーフ）
        const auto* bossTrans = _world->GetComponent<TransformComponent>(drone.ownerBossId);
        const auto* bossCmd = _world->GetComponent<BossCommandComponent>(drone.ownerBossId);
        if (!bossTrans || !bossCmd) return;

        // =================================================================
        // フェーズ 1: エッジ（瞬間）の処理
        // ボスからの命令が「切り替わった瞬間」のステート初期化を行う
        // =================================================================
        HandleStateTransition(drone, *bossCmd);

        // =================================================================
        // フェーズ 2: レベル（継続）の処理
        // 現在のステートに基づいて、毎フレーム継続的に目標座標を再計算する
        // =================================================================
        switch (drone.currentFormation) {
            // 円陣を描く動き
        case DroneFormationType::OrbitCircle:
            CalculateOrbitCircle(drone, *bossTrans, currentTime);
            break;

			// 順番にプレイヤーへ突撃する動き
        case DroneFormationType::SequentialAttack:
            CalculateSequentialAttack(drone, *bossCmd, dt);
            break;

			// プレイヤーを囲む「処刑の輪」の動き
        case DroneFormationType::DeathRing:   
            CalculateDeathRing(drone, *bossCmd, currentTime);
            break;

			// ボスの前面に壁を作る「絶対防衛陣形」の動き
        case DroneFormationType::AegisShield: 
            CalculateAegisShield(drone, *bossTrans, *bossCmd);
			break;

			// 収納されている状態は目標座標を更新しない（ドローン移動システムが勝手にボスの近くに置いてくれる想定）
        case DroneFormationType::Hidden:
        default:
            break;
        }
        });
}

// ============================================================================
// ヘルパー関数の実装 (inlineにより関数呼び出しのコストを排除)
// ============================================================================

// ボスの命令が変更された瞬間の状態遷移処理　（たった１フレームだけ、ロックオン状態になるのがポイント！）
inline void DroneFormationSystem::HandleStateTransition(DroneComponent& drone, const BossCommandComponent& bossCmd) {
	// 命令が切り替わった瞬間の処理：新しいフォーメーションに応じて状態を初期化する
    if (drone.currentFormation != bossCmd.requestFormation) {

        drone.currentFormation = bossCmd.requestFormation;

       // 新しい陣形（演出意図）に応じて、最適な「物理挙動（DroneState）」をセットする
        switch (drone.currentFormation) {
            
            case DroneFormationType::SequentialAttack:
                // 順番突撃：まずはその場で静止（LockOn）し、個体ごとに異なる待機時間をセットする
                drone.currentState = DroneState::LockOn;
                drone.stateTimer = drone.localIndex * 0.3f; // ドミノ倒しのように発射タイミングをズラす
                break;
            
            case DroneFormationType::AegisShield:
                // 絶対防衛（盾）：フワフワしていると壁の隙間を抜けられてしまうため、
                // 素早くビシッと定位置に整列する「機械的移動」をセットする
                drone.currentState = DroneState::MoveToTarget;
                break;

            case DroneFormationType::OrbitCircle:
				// 円陣：フワフワしていると全体の形が崩れてしまうため、素早くビシッと定位置に整列する「機械的移動」をセットする
                drone.currentState = DroneState::MoveToTarget;
                break;

            case DroneFormationType::DeathRing:
                // 円陣・処刑の輪：生物的な不気味さ・威圧感を出すため、
                // スプリング制御による「有機的なフワフワ追従」をセットする
                drone.currentState = DroneState::Idle;
                break;

            case DroneFormationType::Hidden:
            default:
                drone.currentState = DroneState::Idle;
                break;
        }
    }
}


// 円形フォーメーションの目標座標を計算する関数
inline void DroneFormationSystem::CalculateOrbitCircle(DroneComponent& drone, const TransformComponent& bossTrans, float currentTime) {
    // 【数式】 \theta_i = \text{base\_angle} + \left( \frac{2\pi}{N} \right) \times i
    float baseAngle = currentTime * 2.0f;
    float angleOffset = (DirectX::XM_2PI / (float)drone.totalDrones) * (float)drone.localIndex;
    float finalAngle = baseAngle + angleOffset;

    Vector3 offset;
    offset.x = cosf(finalAngle) * drone.orbitRadius;
    offset.y = 2.0f + sinf(currentTime * 3.0f + drone.localIndex) * 0.5f; // Y軸の微細な波打ち
    offset.z = sinf(finalAngle) * drone.orbitRadius;

    // ボスの相対座標から絶対座標へ変換して目標を決定
    drone.targetPosition = bossTrans.position + offset;
}


// 順番攻撃フォーメーションの目標座標と状態を計算する関数
inline void DroneFormationSystem::CalculateSequentialAttack(DroneComponent& drone, const BossCommandComponent& bossCmd, float dt) {
    // ロックオン（待機）中のみタイマーを減らし、0になったら突撃を開始する
    if (drone.currentState == DroneState::LockOn) {
        drone.stateTimer -= dt;
        if (drone.stateTimer <= 0.0f) {
            const auto* playerTrans = _world->GetComponent<TransformComponent>(bossCmd.targetPlayerId);
            if (playerTrans) {
                drone.targetPosition = playerTrans->position;
                drone.currentState = DroneState::FireCharge;
            }
        }
    }
}

// 1. 処刑の輪（プレイヤーの周囲を囲む）
inline void DroneFormationSystem::CalculateDeathRing(DroneComponent& drone, const BossCommandComponent& bossCmd, float currentTime) {
    const auto* playerTrans = _world->GetComponent<TransformComponent>(bossCmd.targetPlayerId);
    if (!playerTrans) return;

    // プレイヤーを中心に円陣を組む
    float baseAngle = currentTime * 1.0f; // ボス中心より少しゆっくり回る
    float angleOffset = (DirectX::XM_2PI / (float)drone.totalDrones) * (float)drone.localIndex;
    float finalAngle = baseAngle + angleOffset;

    Vector3 offset;
    float radius = 10.0f; // プレイヤーを囲む半径（広めにして逃げ道を見せる）
    offset.x = cosf(finalAngle) * radius;
    offset.y = 2.0f;
    offset.z = sinf(finalAngle) * radius;

    drone.targetPosition = playerTrans->position + offset;
}

// 2. 絶対防衛陣形（ボスの前面に壁を作る）
inline void DroneFormationSystem::CalculateAegisShield(DroneComponent& drone, const TransformComponent& bossTrans, const BossCommandComponent& bossCmd) {
    const auto* playerTrans = _world->GetComponent<TransformComponent>(bossCmd.targetPlayerId);
    if (!playerTrans) return;

    // ボスからプレイヤーへの方向ベクトルを算出
    Vector3 toPlayer = playerTrans->position - bossTrans.position;
    toPlayer.y = 0.0f; // 水平面のみで考える
    if (toPlayer.LengthSquared() > 0.001f) {
        toPlayer.Normalize();
    }
    else {
        toPlayer = Vector3::UnitZ; // プレイヤーが完全重なっている場合のフェイルセーフ
    }

    // 進行方向に対する「右方向」のベクトル（外積）
    Vector3 right = toPlayer.Cross(Vector3::UnitY);

    // ボスの少し前に壁（盾の中心）を展開
    float forwardOffset = 3.0f;
    Vector3 shieldCenter = bossTrans.position + toPlayer * forwardOffset;
    shieldCenter.y += 2.0f;

    // 横一列に並べるためのオフセット計算（ローカルインデックスを利用）
    float spacing = 2.5f; // ドローン同士の間隔
    float startOffset = -((drone.totalDrones - 1) * spacing) * 0.5f; // 中心を0とするための計算
    float localOffset = startOffset + (drone.localIndex * spacing);

    drone.targetPosition = shieldCenter + right * localOffset;
}

// 物理の更新より前に座標を計算するため、PrePhysicsを指定
REGISTER_LOGIC_SYSTEM(DroneFormationSystem, Priority::LogicStage::L03_PrePhysics);