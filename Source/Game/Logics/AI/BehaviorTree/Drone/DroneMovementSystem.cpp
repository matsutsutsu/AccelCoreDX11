/**
 * @file DroneMovementSystem.cpp
 * @brief ドローン移動システムの実装
 */
#include "DroneMovementSystem.h"
#include "Game/Core/SystemPriority.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include <cmath>
#include "Engine/Physics/IPhysicsAPI.h"

using namespace CCL::ECS;
using namespace DirectX::SimpleMath;

void DroneMovementSystem::Update(float rawDt) {
    // 1. Facade(IPhysicsAPI)がResourceに登録されているかチェック
    if (!_world->HasResource<std::shared_ptr<IPhysicsAPI>>()) return;

    // shared_ptrを取り出してローカルに保持
    auto physicsAPI = _world->GetResource<std::shared_ptr<IPhysicsAPI>>();


    // ★ ラムダの引数に const TimeState& time を追加
    ForEachWithIDParallel([this, physicsAPI, rawDt](EntityID id, TransformComponent& trans, DroneComponent& drone, const TimeState& time) {

        // ★ ドローン各自の時計を使用（ドローン自身が殴られた時などにピタッと止まる）
        float dt = time.localDt;

        drone.hoverTimeOffset += dt;

        switch (drone.currentState) {
        case DroneState::MoveToTarget: ProcessMoveToTarget(trans, drone, dt); break;
        case DroneState::FireCharge:   ProcessFireCharge(trans, drone, dt); break;
        case DroneState::Idle:         ProcessIdle(trans, drone, dt); break;
        case DroneState::LockOn:       ProcessLockOn(trans, drone, dt); break;
        case DroneState::Hold:         ProcessHold(trans, drone, dt); break; 
        }

        // =================================================================
        // ★ 4. Facadeを経由した物理の注入 (依存性逆転の実現)
        // AIはJoltを知らない。ただ「APIに速度と回転を渡す」だけ。
        // =================================================================

        // ★追加：Joltに渡す直前に、蓄積した微小な計算誤差（ドリフト）を浄化する
        DirectX::XMVECTOR quatVec = DirectX::XMLoadFloat4(&trans.rotation);

        // 正規化を実行（これで長さがピッタリ 1.0 に浄化される）
        quatVec = DirectX::XMQuaternionNormalize(quatVec);

        // 正規化した結果を trans.rotation に書き戻す
        DirectX::XMStoreFloat4(&trans.rotation, quatVec);

        physicsAPI->SetRotation(id, DirectX::XMFLOAT4(trans.rotation.x, trans.rotation.y, trans.rotation.z, trans.rotation.w));

        // -------------------------------------------------------------
        // ★ 最終的な物理速度の決定
        // -------------------------------------------------------------
        DirectX::SimpleMath::Vector3 finalVel = drone.currentVelocity;

        // 【修正】物理速度のタイムスケール・スケーリング
        // 自分の時計(dt)が、現実時間(rawDt)に対してどれくらい遅れているかの比率を出す
        // 例: 完全に止まっているなら 0.0倍、スローなら 0.1倍、通常なら 1.0倍
        float timeScale = (rawDt > 0.0001f) ? (dt / rawDt) : 0.0f;
        
        // 物理エンジンに渡す速度自体を、タイムスケールに合わせて割引きする
        finalVel *= timeScale;


        physicsAPI->SetLinearVelocity(id, finalVel);
        

        });
}


// ============================================================================
// 各ステートの具体的な物理演算ロジック
// ============================================================================

inline void DroneMovementSystem::ProcessMoveToTarget(TransformComponent& trans, DroneComponent& drone, float dt) {
    // 【調整パラメーター】
    float pGain = 5.0f;       // アクセルの強さ（目的地へ引っ張る力）
    float damping = 5.0f;     // ブレーキの強さ（空気抵抗・摩擦のような減衰力）
    float lerpSpeedRot = 10.0f;

    Vector3 displacement = drone.targetPosition - trans.position;

    // 1. 目標へ向かう力（P制御）を計算
    Vector3 targetVelocity = displacement * pGain;

    // ====================================================================
    // ★ 究極の修正：安全装置（スピードリミッターの導入）
    // 遠く離れた場所（爆散後など）から一気に戻る際、targetVelocityが青天井になり、
    // 猛スピードで陣形を通り過ぎてから戻る「オーバーシュート」を物理的に防ぐ。
    // ====================================================================
    float maxSpeed = drone.moveSpeed * 1.5f; // 陣形復帰を少し早めるため、基礎スピードの1.5倍まで許可
    if (targetVelocity.LengthSquared() > maxSpeed * maxSpeed) {
        targetVelocity.Normalize();
        targetVelocity *= maxSpeed;
    }

    // 2. 現在の速度に「目標速度」を滑らかに近づけつつ、急激な変化を減衰（ブレーキ）させる
    drone.currentVelocity = Vector3::Lerp(drone.currentVelocity, targetVelocity, damping * dt);

    // 3. 回転処理
    if (displacement.LengthSquared() > 0.001f) {
        float targetYaw = atan2f(displacement.x, displacement.z);
        Quaternion targetRot = Quaternion::CreateFromAxisAngle(Vector3::UnitY, targetYaw);
        trans.rotation = Quaternion::Slerp(trans.rotation, targetRot, lerpSpeedRot * dt);
        trans.isDirty = true;
    }
}

inline void DroneMovementSystem::ProcessFireCharge(TransformComponent& trans, DroneComponent& drone, float dt) {
    if (drone.fireDirection.LengthSquared() > 0.001f) {

        // ====================================================================
        // [フェーズ1] アンティシペーション（予備動作・タメ）
        // ====================================================================
        if (drone.stateTimer > 0.0f) {
            drone.stateTimer -= dt; // タイマーを消費

            // 突撃方向と「逆（マイナス）」へ、ゆっくりと引き絞る（バックステップ）
            float drawbackSpeed = 2.0f;
            drone.currentVelocity = -drone.fireDirection * drawbackSpeed;

            // 力を溜めている威圧感を出すための、激しいブルブル震え（Jitter）
            drone.currentVelocity.x += std::cos(drone.hoverTimeOffset * 50.0f) * 1.5f;
            drone.currentVelocity.y += std::sin(drone.hoverTimeOffset * 60.0f) * 1.5f;
            drone.currentVelocity.z += std::cos(drone.hoverTimeOffset * 40.0f) * 1.5f;

            // 下がっている間も、回転はプレイヤー（進行方向）をガン見し続ける
            float targetYaw = atan2f(drone.fireDirection.x, drone.fireDirection.z);
            Quaternion targetRot = Quaternion::CreateFromAxisAngle(Vector3::UnitY, targetYaw);
            trans.rotation = Quaternion::Slerp(trans.rotation, targetRot, 20.0f * dt);
            trans.isDirty = true;
        }
        // ====================================================================
        // [フェーズ2] 超高速突撃（リリース）
        // ====================================================================
        else {
            float chargeSpeedMultiplier = 1.5f;
            // タイマーが切れた瞬間、溜めた力を解放して一気に前進！
            drone.currentVelocity = drone.fireDirection * (drone.moveSpeed * chargeSpeedMultiplier);

            float targetYaw = atan2f(drone.fireDirection.x, drone.fireDirection.z);
            Quaternion targetRot = Quaternion::CreateFromAxisAngle(Vector3::UnitY, targetYaw);
            trans.rotation = Quaternion::Slerp(trans.rotation, targetRot, 25.0f * dt);
            trans.isDirty = true;
        }
    }
    else {
        // フェイルセーフ
        drone.currentVelocity = Vector3::Zero;
    }
}

// ============================================================================
// ProcessIdle の究極シンプル版（絶対に爆発しない・震えない）
// ============================================================================
inline void DroneMovementSystem::ProcessIdle(TransformComponent& trans, DroneComponent& drone, float dt) {

    // 1. 目標までのベクトルを計算
    Vector3 displacement = drone.targetPosition - trans.position;

    // 2. 目標へ向かう「理想の速度」を算出（P制御）
    float pGain = 4.0f;
    Vector3 targetVelocity = displacement * pGain;

    // ====================================================================
    // ★ 安全装置：最高速度の制限（Clamp）
    // どんなに遠くても、ドローンの基本スピード（moveSpeed）以上の速度を出さない。
    // これにより「遠いほど超加速してぶるぶる震える」バグを物理的に完全消滅させます。
    // ====================================================================
    float maxSpeed = drone.moveSpeed;
    if (targetVelocity.LengthSquared() > maxSpeed * maxSpeed) {
        targetVelocity.Normalize();
        targetVelocity *= maxSpeed;
    }

    // 3. 現在の速度を、目標速度に滑らかに近づける（Lerpによるブレーキ）
    float damping = 5.0f;
    drone.currentVelocity = Vector3::Lerp(drone.currentVelocity, targetVelocity, damping * dt);

    // 4. 陣形時の「フワフワ感（ホバー）」だけはY軸の速度に少し加算する
    float hoverFrequency = 1.0f;
    float hoverAmplitude = 3.0f;
    drone.currentVelocity.y += std::cos(drone.hoverTimeOffset * hoverFrequency) * hoverAmplitude * dt;

    // 5. 回転（シンプルに目標の方向を向く）
    if (displacement.LengthSquared() > 0.001f) {
        float targetYaw = atan2f(displacement.x, displacement.z);
        Quaternion targetRot = Quaternion::CreateFromAxisAngle(Vector3::UnitY, targetYaw);
        trans.rotation = Quaternion::Slerp(trans.rotation, targetRot, 10.0f * dt);
        trans.isDirty = true;
    }
}

// ============================================================================
// ★追加：発射前のエネルギー充填（ブルブル震える演出）
// ============================================================================
inline void DroneMovementSystem::ProcessLockOn(TransformComponent& trans, DroneComponent& drone, float dt) {
    float pGain = 15.0f;
    float damping = 10.0f;

    Vector3 displacement = drone.targetPosition - trans.position;
    Vector3 targetVelocity = displacement * pGain;

    // 発射直前のエネルギー充填を表現する、激しい微振動（Jitter）
    float jitterX = std::cos(drone.hoverTimeOffset * 50.0f) * 2.0f;
    float jitterY = std::sin(drone.hoverTimeOffset * 60.0f) * 2.0f;
    float jitterZ = std::cos(drone.hoverTimeOffset * 40.0f) * 2.0f;

    targetVelocity.x += jitterX;
    targetVelocity.y += jitterY;
    targetVelocity.z += jitterZ;

    drone.currentVelocity = Vector3::Lerp(drone.currentVelocity, targetVelocity, damping * dt);

    if (displacement.LengthSquared() > 0.001f) {
        float targetYaw = atan2f(displacement.x, displacement.z);
        Quaternion targetRot = Quaternion::CreateFromAxisAngle(Vector3::UnitY, targetYaw);
        trans.rotation = Quaternion::Slerp(trans.rotation, targetRot, 20.0f * dt);
        trans.isDirty = true;
    }
}


// ============================================================================
// ★修正：完全停止（急減速からのイーズアウト停止）
// ============================================================================
inline void DroneMovementSystem::ProcessHold(TransformComponent& trans, DroneComponent& drone, float dt) {

    // 【調整パラメーター】 急ブレーキの摩擦力
    // 以前の 20.0f（壁激突）から、5.0f（急ブレーキ）に引き下げました。
    // 数値を下げるほど、ツルッと氷の上を滑るように止まるようになります。
    float damping = 3.0f;

    // 現在の速度を、ゼロに向かって滑らかに減衰させる（徐々にブレーキが弱まる美しい減速曲線）
    drone.currentVelocity = Vector3::Lerp(drone.currentVelocity, Vector3::Zero, damping * dt);

    // ★重要: Lerpの計算上、永遠に完全なゼロにはならないため、
    // 「ほぼ止まった」と見なせる微小な速度になった瞬間に、速度をピタッと完全に0（空間固定）にする。
    if (drone.currentVelocity.LengthSquared() < 0.01f) {
        drone.currentVelocity = Vector3::Zero;
    }

    // 回転（向いている方向）はそのまま維持する

    // ====================================================================
    // ★ 究極のバグ修正: 描画の強制更新フラグ
    // 透明な物理肉体がブレーキをかけて滑っている間、3Dモデルのガワも
    // ちゃんとそれに追従するように毎フレーム描画を更新させます。
    // ====================================================================
    trans.isDirty = true;
}


// 実際の座標更新を行うため、L04_Physicsを指定
REGISTER_LOGIC_SYSTEM(DroneMovementSystem, Priority::LogicStage::L04_Physics);