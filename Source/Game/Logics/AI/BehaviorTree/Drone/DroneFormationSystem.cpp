/**
 * @file DroneFormationSystem.cpp
 * @brief ドローンフォーメーションシステムの実装
 */
#include "DroneFormationSystem.h"
#include "ECS/Core/CCL_World.h"
#include "Game/Core/SystemPriority.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include <cmath>
#include <imgui.h>
#include "Engine/Serialization/Factory/Prefab.h"
#include "Engine/Serialization/Factory/EntityFactory.h"
#include "Game/Logics/AI/BehaviorTree/BossActionComponent.h"


using namespace CCL::ECS;
using namespace DirectX::SimpleMath;

// ============================================================================
// メインの更新処理
// ============================================================================
void DroneFormationSystem::Update(float rawDt) {

    // ★ 陣形全体の回転時間は、世界の親時計（TimeContext）に合わせて歪ませる
    float globalScale = 1.0f;
    if (_world->HasResource<TimeContext>()) {
        globalScale = _world->GetResource<TimeContext>().globalScale;
    }
    m_currentTime += rawDt * globalScale;
    float currentTime = m_currentTime;

    // ====================================================================
    // ★ 究極のハック：ドローンの自動ナンバリング（堅牢化版）
    // ====================================================================
    // まず、実際のドローンのエンティティIDをすべてリストアップする
    std::vector<EntityID> activeDrones;
    for (auto e : _world->View<DroneComponent>()) {
        activeDrones.push_back(e);
    }
    int actualDroneCount = static_cast<int>(activeDrones.size());

    if (actualDroneCount > 0) {
        // 先頭のドローンの記憶を確認
        auto* firstDrone = _world->GetComponent<DroneComponent>(activeDrones[0]);

        // ★修正: 総数が違っている場合【だけでなく】、念のため毎フレーム
        // 「インデックスが重複していないか（0番が複数いないか）」の厳密チェックを行うのは重いので、
        // 総数が変わった瞬間に、抽出したリスト(activeDrones)を使って確実にナンバリングする！
        if (firstDrone && firstDrone->totalDrones != actualDroneCount) {

            // リストの順番通りに、0から確実に番号を振り直す
            for (int i = 0; i < actualDroneCount; ++i) {
                auto* d = _world->GetComponent<DroneComponent>(activeDrones[i]);
                if (d) {
                    d->totalDrones = actualDroneCount;
                    d->localIndex = static_cast<uint16_t>(i);
                }
            }
            CCL_LOG_INFO(LogCategory::Game, "Drones re-numbered. Total: %d", actualDroneCount);
        }
    }

    // 超並列処理による各ドローンの更新
    ForEachParallel([this, currentTime](DroneComponent& drone, const TimeState& time) {

        // ★ ドローン各自の時計を使用（突撃待機のタイマー減算などに使う）
        float dt = time.localDt;

        // 1. 親と指示の取得（フェイルセーフ）
        const auto* bossTrans = _world->GetComponent<TransformComponent>(drone.ownerBossId);
        const auto* bossCmd = _world->GetComponent<BossCommandComponent>(drone.ownerBossId);
        if (!bossTrans || !bossCmd) return;

        // =================================================================
        // フェーズ 1: エッジ（瞬間）の処理
        // ボスからの命令が「切り替わった瞬間」のステート初期化を行う
        // =================================================================
        HandleStateTransition(drone, *bossTrans, *bossCmd);

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

        case DroneFormationType::HighOrbit:
            CalculateHighOrbit(drone, *bossTrans, currentTime);
				break;

        case DroneFormationType::LowOrbit:
            CalculateLowOrbit(drone, *bossTrans, currentTime);
                break;

        case DroneFormationType::SpreadLockOn:
            CalculateSpreadLockOn(drone, *bossTrans, currentTime);
				break;
        case DroneFormationType::CloseGuard:
            CalculateCloseGuard(drone, *bossTrans, currentTime);
            break;
        case DroneFormationType::BarrierBurst:
            CalculateBarrierBurst(drone, *bossTrans, dt, currentTime);
            break;

        case DroneFormationType::ChargeTunnel:
            CalculateChargeTunnel(drone, *bossTrans, *bossCmd);
            break;
			// 収納されている状態は目標座標を更新しない（ドローン移動システムが勝手にボスの近くに置いてくれる想定）
        case DroneFormationType::HoldPosition:
            // ★あえて「目標座標を一切更新しない」ことで、
            // ドローンが前回命令された時のワールド座標に永遠に固定され続けます。
            break;
        case DroneFormationType::CycloneBurst: 
            CalculateCycloneBurst(drone, *bossTrans, dt, currentTime);
            break;

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
inline void DroneFormationSystem::HandleStateTransition(DroneComponent& drone, const TransformComponent& bossTrans, const BossCommandComponent& bossCmd) {
    // 命令が切り替わった瞬間の処理：新しいフォーメーションに応じて状態を初期化する
    if (drone.currentFormation != bossCmd.requestFormation) {

        drone.currentFormation = bossCmd.requestFormation;

        const auto* playerTrans = _world->GetComponent<TransformComponent>(bossCmd.targetPlayerId);

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
                break;
            case DroneFormationType::HighOrbit: 
				// 高軌道：ボスの頭上高くで旋回させる場合も、フワフワと有機的に追従させる]
                // 遠くから素早く頭上に戻したい場合は、IdleではなくMoveToTargetがおすすめです。
                drone.currentState = DroneState::MoveToTarget;
                break;
            case DroneFormationType::LowOrbit:
                // フワフワと有機的に追従させる
                drone.currentState = DroneState::Idle;
                break;

            case DroneFormationType::SpreadLockOn:
                // 所定の位置へ素早く移動してピタッと止まるための機械的移動
                drone.currentState = DroneState::MoveToTarget;
                break;

            case DroneFormationType::AllCharge:
                //  一斉突撃。命令が下った「その瞬間」のプレイヤーの方向を計算して固定する
                drone.currentState = DroneState::FireCharge;

                if (playerTrans) {
                    Vector3 dir = playerTrans->position - drone.targetPosition;
                    if (dir.LengthSquared() > 0.001f) {
                        dir.Normalize();
                        drone.fireDirection = dir;
                    }
                    else {
                        drone.fireDirection = Vector3::UnitZ; // フェイルセーフ
                    }
                }
                break;
            case DroneFormationType::BarrierBurst:
                drone.currentState = DroneState::LockOn;
                drone.stateTimer = 1.5f; // ★ 1.5秒間、力を溜める（ブルブル震える）
                break;

            case DroneFormationType::HoldPosition:
                // ★修正: MoveToTarget ではなく、専用の「完全停止（Hold）」ステートを指定する
                drone.currentState = DroneState::Hold;
                break;
            case DroneFormationType::DirectionalCharge:
                drone.currentState = DroneState::FireCharge;

                // ============================================================
                // ★ 究極のハック: 突撃状態に入ると同時にタイマーをセットする
                // これにより、物理システムが「タイマーが切れるまではタメ期間だ」と認識します。
                // ============================================================
                drone.stateTimer = 0.8f; // 0.8秒間、弓を引き絞るようにタメる

                if (playerTrans) {
                    Vector3 dir = playerTrans->position - bossTrans.position;
                    dir.y = 0.0f; // 水平に飛ばす
                    if (dir.LengthSquared() > 0.001f) {
                        dir.Normalize();
                        drone.fireDirection = dir;
                    }
                    else {
                        drone.fireDirection = Vector3::UnitZ;
                    }
                }
                break;
            case DroneFormationType::CycloneBurst:
                drone.currentState = DroneState::LockOn;
                drone.stateTimer = 1.5f; // 1.5秒間、回転しながらボスの中心へ引き絞るタメ
                break;
            default:
                drone.currentState = DroneState::Idle;
                break;
        }
    }
}


// 1. 円陣（OrbitCircle）
inline void DroneFormationSystem::CalculateOrbitCircle(DroneComponent& drone, const TransformComponent& bossTrans, float currentTime) {
    // 【調整パラメーター】
    // 1.0f = 旋回スピード（大きいほど高速で回る。マイナスにすると逆回転）
    float speed = 1.0f;

    // 角度の計算： (時間 × スピード) + (360度 ÷ ドローン総数 × 自分の番号)
    float angle = currentTime * speed + (DirectX::XM_2PI / drone.totalDrones) * drone.localIndex;

    // 【調整パラメーター】
    // drone.orbitRadius = 旋回の広さ（現在はコンポーネントから取得）
    // 0.0f = ボスからの高さ（+2.0fなどにするとボスの少し上を回る）
    float heightOffset = 1.0f;

    Vector3 offset(cosf(angle) * drone.orbitRadius, heightOffset, sinf(angle) * drone.orbitRadius);
    drone.targetPosition = bossTrans.position + offset;
}


// 順番攻撃フォーメーションの目標座標と状態を計算する関数
inline void DroneFormationSystem::CalculateSequentialAttack(DroneComponent& drone, const BossCommandComponent& bossCmd, float dt) {
    // ロックオン（待機）中のみタイマーを減らし、0になったら突撃を開始する
    if (drone.currentState == DroneState::LockOn) {
        drone.stateTimer -= dt;

        // 時間が来たら発火
        if (drone.stateTimer <= 0.0f) {
            const auto* playerTrans = _world->GetComponent<TransformComponent>(bossCmd.targetPlayerId);
            if (playerTrans) {
                drone.currentState = DroneState::FireCharge;
                // ★修正: プレイヤーへの方向ベクトルを計算してセット
                Vector3 dir = playerTrans->position - drone.targetPosition;
                if (dir.LengthSquared() > 0.001f) {
                    dir.Normalize();
                    drone.fireDirection = dir;
                }
            }
        }
        else {
            // 発火待ちの間は、その場で高速回転してエネルギーをタメているように見せる
            const auto* bossTrans = _world->GetComponent<TransformComponent>(drone.ownerBossId);
            if (bossTrans) {
                // 【調整パラメーター】 5.0f = 待機中の「ブルブル震える・回る」演出スピード
                float angle = drone.stateTimer * 5.0f + (DirectX::XM_2PI / drone.totalDrones) * drone.localIndex;

                // ★修正：Y軸のオフセットを足元(0.0f)から、ボスの周囲の高さ(10.0fなど)に引き上げる
                float heightOffset = 10.0f;

                Vector3 offset(cosf(angle) * 10.0f, heightOffset, sinf(angle) * 10.0f);
                drone.targetPosition = bossTrans->position + offset;
            }
        }
    }
}

// 1. 処刑の輪（プレイヤーの周囲を囲む）
inline void DroneFormationSystem::CalculateDeathRing(DroneComponent& drone, const BossCommandComponent& bossCmd, float currentTime) {
    const auto* playerTrans = _world->GetComponent<TransformComponent>(bossCmd.targetPlayerId);
    if (!playerTrans) return;

    // プレイヤーの周囲を囲むように配置
    // 【調整パラメーター】 1.0f = 囲んだ後ゆっくりジリジリと回る威圧スピード
    float finalAngle = currentTime * 1.0f + (DirectX::XM_2PI / drone.totalDrones) * drone.localIndex;
    Vector3 offset;
    // 【調整パラメーター】 12.0f = プレイヤーを囲む時の「檻」の広さ
    float radius = 12.0f; // プレイヤーを囲む半径（広めにして逃げ道を見せる）
    offset.x = cosf(finalAngle) * radius;
    // 【調整パラメーター】 2.0f = プレイヤーの頭の高さくらいに浮かせる
    offset.y = 2.0f;
    offset.z = sinf(finalAngle) * radius;

    drone.targetPosition = playerTrans->position + offset;
}

// 2. 絶対防衛陣形（ボスの前面に壁を作る）
inline void DroneFormationSystem::CalculateAegisShield(DroneComponent& drone, const TransformComponent& bossTrans, const BossCommandComponent& bossCmd) {
    const auto* playerTrans = _world->GetComponent<TransformComponent>(bossCmd.targetPlayerId);
    if (!playerTrans) return;

    Vector3 toPlayer = playerTrans->position - bossTrans.position;
    toPlayer.y = 0.0f; // 水平面のみで考える
    if (toPlayer.LengthSquared() > 0.001f) {
        toPlayer.Normalize();
    }
    else {
        toPlayer = Vector3::UnitZ;
    }

    Vector3 right = toPlayer.Cross(Vector3::UnitY);
    float forwardOffset = 3.0f;
    Vector3 shieldCenter = bossTrans.position + toPlayer * forwardOffset;

    // ====================================================================
    // 【調整パラメーター】 グリッド設定
    // ====================================================================
    int maxColumns = 6;    // 横に並ぶ最大ドローン数（6体で折り返し）
    float spacingX = 1.8f; // 横の隙間 (広げると壁が横に広がる)
    float spacingY = 1.5f; // 縦の隙間 (上に積み上がる高さの間隔)

    // 割り算と余りで、自分が「何段目の、左から何番目か」を計算する魔法
    int row = drone.localIndex / maxColumns; // 段数 (0, 1, 2...)
    int col = drone.localIndex % maxColumns; // 横位置 (0, 1, 2, 3, 4, 5)

    // この段にいるドローンの数（最終段は数が半端かもしれないので安全に計算）
    int dronesInThisRow = (std::min)(maxColumns, drone.totalDrones - row * maxColumns);

    // その段をボスの正面に「中央揃え」するためのスタート位置
    float wallWidth = (dronesInThisRow - 1) * spacingX;
    float startOffset = -wallWidth * 0.5f;

    // 最終的な相対座標の決定
    float currentOffsetX = startOffset + col * spacingX;
    float currentOffsetY = row * spacingY; // 段が上がるごとにY座標が高くなる

    // 盾のベース高さ(1.0f)に段差(currentOffsetY)を足す
    drone.targetPosition = shieldCenter + right * currentOffsetX + Vector3(0, 1.0f + currentOffsetY, 0);
}


// 1. 上空待機 (HighOrbit)
inline void DroneFormationSystem::CalculateHighOrbit(DroneComponent& drone, const TransformComponent& bossTrans, float currentTime) {

    // 【調整パラメーター】 1.0f = 上空での旋回スピード
    float angle = currentTime * 1.0f + (DirectX::XM_2PI / drone.totalDrones) * drone.localIndex;
    // 【調整パラメーター】 8.0f = 上空待機時の高さ（Y座標のオフセット）
    Vector3 offset(cosf(angle) * drone.orbitRadius, 8.0f, sinf(angle) * drone.orbitRadius); // ★Y軸に +8.0f (頭上)
    drone.targetPosition = bossTrans.position + offset;
}

// 2. 降下展開 (LowOrbit)
inline void DroneFormationSystem::CalculateLowOrbit(DroneComponent& drone, const TransformComponent& bossTrans, float currentTime) {
    // 【調整パラメーター】 0.5f = 降下後の旋回スピード
    float angle = currentTime * 1.0f + (DirectX::XM_2PI / drone.totalDrones) * drone.localIndex;

    // ====================================================================
    // ★修正: ボスのスケールに合わせて、高さを 4.0f から引き上げる
    // 例: ボスの胸あたりにしたい場合は 10.0f や 12.0f などに調整してください
    // ====================================================================
    float heightOffset = 1.5f;

  
    Vector3 offset(cosf(angle) * drone.orbitRadius, heightOffset, sinf(angle) * drone.orbitRadius); // ★Y軸は heightOffset (ボスと同じ高さ)
    drone.targetPosition = bossTrans.position + offset;
}

// 3. 拡散静止 (SpreadLockOn)
inline void DroneFormationSystem::CalculateSpreadLockOn(DroneComponent& drone, const TransformComponent& bossTrans, float currentTime) {
    // 回転を止めたいので、currentTimeではなく固定値で角度を算出する（ボス基準で均等に配置）
    float angle = (DirectX::XM_2PI / drone.totalDrones) * drone.localIndex;
    // 攻撃の予兆として、半径を少し広げる (orbitRadius * 1.5f)
    // 【調整パラメーター】 1.5f = 攻撃の予兆として広がる大きさの倍率、1.0f = 展開時の高さ
    Vector3 offset(cosf(angle) * (drone.orbitRadius * 1.5f), 1.0f, sinf(angle) * (drone.orbitRadius * 1.5f));
    drone.targetPosition = bossTrans.position + offset;

}



// ============================================================================
// システム専用GUIの描画（エディタ実行時のみ呼ばれる）
// ============================================================================
void DroneFormationSystem::OnGui() {

    // システムのコントロールパネルを展開
    if (ImGui::CollapsingHeader("Drone Controller (Swarm Management)", ImGuiTreeNodeFlags_DefaultOpen)) {

        ImGui::Indent();

        // 1. 生成パラメータの設定
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Spawn Settings");
        ImGui::SliderInt("Spawn Count", &m_guiDroneCount, 1, 24);
        ImGui::SliderFloat("Spawn Radius", &m_guiOrbitRadius, 2.0f, 20.0f);
        ImGui::SliderFloat("Spawn Speed", &m_guiMoveSpeed, 10.0f, 100.0f);

        ImGui::Spacing();

        // 2. 現在のドローン数の取得（ECSのViewを使って数える）
        int currentDroneCount = 0;
        auto droneView = _world->View<DroneComponent>();
        for (auto e : droneView) {
            currentDroneCount++;
        }
        ImGui::Text("Current Active Drones: %d", currentDroneCount);

        ImGui::Spacing();

        // 3. ボスの検索（誰の周りにドローンを出すか）
        CCL::ECS::EntityID activeBossId = CCL::ECS::InvalidEntityID;
        auto bossView = _world->View<BossCommandComponent, TransformComponent>();
        for (auto b : bossView) {
            activeBossId = b;
            break; // 最初のボスを見つけたら終了
        }

        if (activeBossId == CCL::ECS::InvalidEntityID) {
            ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "Warning: No Boss found in scene!");
        }
        else {
            // =========================================================
             // 【アクション】ドローンの動的追加
             // =========================================================
            if (ImGui::Button("Spawn Drones", ImVec2(120, 30))) {

                auto* bossTrans = _world->GetComponent<TransformComponent>(activeBossId);
                Vector3 spawnBasePos = bossTrans ? Vector3(bossTrans->position) : Vector3::Zero;
                spawnBasePos.y += 5.0f;

                // 指定された数だけ生成
                for (int i = 0; i < m_guiDroneCount; ++i) {
                    EntityID droneId = Prefab::SpawnPrefab(*_world, "Assets/Prefabs/Enemy/Drone.json");
                    auto droneRef = CCL::ECS::Core::EntityRef(_world, droneId);

                    droneRef.Set<TransformComponent>({ .position = spawnBasePos });

                    // ★修正: 生成時は localIndex と totalDrones は初期値(0と1)のままでよい。
                    // 次のフレームのUpdateの先頭で、システムが一括で綺麗に振り直してくれるため。
                    droneRef.Set<DroneComponent>({
                        .ownerBossId = activeBossId,
                        .moveSpeed = m_guiMoveSpeed,
                        .orbitRadius = m_guiOrbitRadius
                        });
                }
            }

            ImGui::SameLine();

            // =========================================================
            // 【アクション】ドローンの全消去
            // =========================================================
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("Destroy All Drones", ImVec2(150, 30))) {

                // ECS上でコンポーネントを持っているEntityを列挙して破棄
                // ※ _world->DestroyEntity(e) が安全にキューイングされる設計である前提
                auto view = _world->View<DroneComponent>();
                for (auto e : view) {
                    _world->Destroy(e);
                }
            }
            ImGui::PopStyleColor();
        }

        ImGui::Unindent();
    }
}


// ============================================================================
// 全方位密着バリア (CloseGuard) : フィボナッチ半球（ドーム）アルゴリズム
// ============================================================================
inline void DroneFormationSystem::CalculateCloseGuard(DroneComponent& drone, const TransformComponent& bossTrans, float currentTime) {
    // 【調整パラメーター】
    float tightRadius = 3.0f;  // ボスをすっぽり覆うドームの半径（ボスのサイズに合わせて広げてください）
    float rotateSpeed = 1.5f;  // バリア全体の回転スピード

    // 1. 自分は何番目のドローンか、0.0(先頭) 〜 1.0(最後尾) の割合にする
    float indexRatio = 0.0f;
    if (drone.totalDrones > 1) {
        indexRatio = (float)drone.localIndex / (float)(drone.totalDrones - 1);
    }

    // ====================================================================
    // ★修正1: 完全な球 (1.0 to -1.0) ではなく、上半分の「半球 (1.0 to 0.0)」にする
    // indexRatio を 2倍 しないことで、高さが地面（0.0）でピッタリ止まります。
    // ====================================================================
    float y = 1.0f - indexRatio;

    // 3. その高さにおける、球の「水平方向の半径」を三平方の定理で計算
    float radiusAtY = std::sqrt(1.0f - y * y);

    // 4. 黄金角（約137.5度 = 2.39996ラジアン）を用いて螺旋配置
    float goldenAngle = 2.399963f;
    float theta = goldenAngle * drone.localIndex + (currentTime * rotateSpeed);

    // 5. XとZの座標を計算
    float x = std::cos(theta) * radiusAtY;
    float z = std::sin(theta) * radiusAtY;

    // ====================================================================
    // ★修正2: heightOffset を廃止。
    // ボスの原点（足元）を中心に、上半分のドームを展開します。
    // ====================================================================
    Vector3 offset(x * tightRadius, (y * tightRadius) + 0.5f, z * tightRadius);
    drone.targetPosition = bossTrans.position + offset;
}

// ============================================================================
// バリア爆発 (BarrierBurst) : 収縮 → 全方位拡散射出
// ============================================================================
inline void DroneFormationSystem::CalculateBarrierBurst(DroneComponent& drone, const TransformComponent& bossTrans, float dt, float currentTime) {
    if (drone.currentState == DroneState::LockOn) {
        // [フェーズ1] 溜め中：ボスの中心に向かってギュッと収縮する
        drone.stateTimer -= dt;

        if (drone.stateTimer <= 0.0f) {
            // [フェーズ2] 溜め完了：全方位へ一斉射出！
            drone.currentState = DroneState::FireCharge;

            // 射出方向は、「ボスの中心から、現在のドローンの配置位置」の延長線上
            Vector3 dir = drone.targetPosition - bossTrans.position;
            if (dir.LengthSquared() > 0.001f) {
                dir.Normalize();
                // ★修正: 無限遠方へのハックを削除し、純粋な方向ベクトルのみを渡す
                drone.fireDirection = dir;
            }
        }
        else {
            // ====================================================================
            // --- 収縮フォーメーションの維持（滑らかな溜め演出） ---
            // ====================================================================

            // 1. 溜めの進行度を 0.0(開始) 〜 1.0(爆発直前) で算出する
            float progress = 1.0f - (drone.stateTimer / 1.5f);
            progress = std::clamp(progress, 0.0f, 1.0f);

            // 2. 半径を CloseGuardの「3.0f」から、溜め完了時の「1.5f」へ滑らかに縮小させる
            float tightRadius = 3.0f - (1.5f * progress);

            // 3. 回転スピードも CloseGuard と完全に一致させる（ピタッと止めるとワープするため）
            float rotateSpeed = 1.5f;

            // --- 以下の計算式は CloseGuard と全く同じものを使用し、断絶を防ぐ ---
            float indexRatio = 0.0f;
            if (drone.totalDrones > 1) {
                indexRatio = (float)drone.localIndex / (float)(drone.totalDrones - 1);
            }

            float y = 1.0f - indexRatio;
            float radiusAtY = std::sqrt(1.0f - y * y);

            float goldenAngle = 2.399963f;
            float theta = goldenAngle * drone.localIndex + (currentTime * rotateSpeed);

            float x = std::cos(theta) * radiusAtY;
            float z = std::sin(theta) * radiusAtY;

            // Yのオフセットも CloseGuard と同じ「+0.5f」にする
            Vector3 offset(x * tightRadius, (y * tightRadius) + 0.5f, z * tightRadius);
            drone.targetPosition = bossTrans.position + offset;
        }
    }
}

// ============================================================================
// 決闘の回廊 (ChargeTunnel) : プレイヤーの左右を塞ぐ完全固定長の隊列
// ============================================================================
inline void DroneFormationSystem::CalculateChargeTunnel(DroneComponent& drone, const TransformComponent& bossTrans, const BossCommandComponent& bossCmd) {

    // ====================================================================
    // ボス突撃時の位置固定（ロックオン・パージ）
    // ====================================================================
    const auto* bossAction = _world->GetComponent<BossActionComponent>(drone.ownerBossId);
    if (bossAction && bossAction->currentState == BossActionState::Charge) {
        return;
    }

    const auto* playerTrans = _world->GetComponent<TransformComponent>(bossCmd.targetPlayerId);
    if (!playerTrans) return;

    // 1. ボスからプレイヤーへの方向ベクトルを算出（★距離は無視して方向だけ抽出）
    Vector3 toPlayer = playerTrans->position - bossTrans.position;
    toPlayer.y = 0.0f; // ★ トンネルが地面に刺さらないよう完全に水平にする

    if (toPlayer.LengthSquared() < 0.001f) {
        toPlayer = Vector3::UnitZ; // フェイルセーフ
    }
    else {
        toPlayer.Normalize();
    }

    // ====================================================================
    // ★ 究極の修正: プレイヤーの位置を無視し、ボスの突進仕様に合わせた固定長
    // ボスの突進スピード(15.0) × 突進時間(1.0s) ＋ 余裕 ＝ 約20.0m
    // ====================================================================
    float tunnelLength = 20.0f;

    // 3. 通路の横幅を決定（左右のオフセット方向）
    Vector3 rightDir = Vector3(toPlayer.z, 0, -toPlayer.x);
    float tunnelWidth = 3.5f;

    // 4. ドローンを左右に振り分ける（偶数は左、奇数は右）
    float sideSign = (drone.localIndex % 2 == 0) ? -1.0f : 1.0f;
    int dronesPerSide = drone.totalDrones / 2;
    int sideIndex = drone.localIndex / 2;

    // 5. 片側あたりのドローン数で、前後方向の配置位置を決める (0.0 ～ 1.0 に分布)
    float t = (dronesPerSide > 1) ? (float)sideIndex / (float)(dronesPerSide - 1) : 0.0f;

    // 6. 最終的な目標座標： ボスから固定長(tunnelLength)までの進捗(t) + 左右のオフセット
    Vector3 offset = (toPlayer * (tunnelLength * t)) + (rightDir * (tunnelWidth * sideSign));
    drone.targetPosition = bossTrans.position + offset;

    // ★ 高さもプレイヤーに依存せず、ボスの少し上で固定する
    drone.targetPosition.y = bossTrans.position.y + 1.5f;
}

// ============================================================================
// サイクロンバースト (CycloneBurst) : 回転しながら収縮 → 水平・全方位射出
// ============================================================================
inline void DroneFormationSystem::CalculateCycloneBurst(DroneComponent& drone, const TransformComponent& bossTrans, float dt, float currentTime) {
    if (drone.currentState == DroneState::LockOn) {
        drone.stateTimer -= dt;

        if (drone.stateTimer <= 0.0f) {
            drone.currentState = DroneState::FireCharge;

            Vector3 dir = drone.targetPosition - bossTrans.position;
            dir.y = 0.0f; // 地面と平行に飛ばす
            if (dir.LengthSquared() > 0.001f) {
                dir.Normalize();
                drone.fireDirection = dir;
            }
            else {
                drone.fireDirection = Vector3::UnitZ;
            }
        }
        else {
            // ====================================================================
            // 【調整パラメーター】 サイクロンバーストの演出設定
            // ====================================================================
            // ★重要: HandleStateTransitionでセットした時間(1.5f)と必ず一致させる！
            float totalChargeTime = 1.5f;

            float minRadius = 1.5f;           // どこまで小さく収縮するか
            float extraSpinTurns = 1.5f;      // タメ中に何周「追加で」回るか (1.5 = 1周半)

            // 加速カーブ (1.0=一定速度で吸い込まれる, 2.0=二次曲線で徐々に加速, 3.0=最後にギュイーンと急加速)
            float spinPower = 2.0f;

            // 1. 溜めの進行度 (0.0 〜 1.0) を正確に算出
            float progress = 1.0f - (drone.stateTimer / totalChargeTime);
            progress = std::clamp(progress, 0.0f, 1.0f);

            // 2. 半径の収縮
            float currentRadius = drone.orbitRadius - ((drone.orbitRadius - minRadius) * progress);

            // 3. ワープを防ぐ角度計算（スピンの追加）
            float baseAngle = currentTime * 1.0f + (DirectX::XM_2PI / drone.totalDrones) * drone.localIndex;
            // 指定したカーブ(spinPower)に従って、指定した周回数(extraSpinTurns)だけ余分に回す
            float spinAngle = std::pow(progress, spinPower) * (DirectX::XM_2PI * extraSpinTurns);
            float angle = baseAngle + spinAngle;

            // 4. 高さの持ち上げ
            float heightOffset = 1.0f + (0.5f * progress);

            Vector3 offset(cosf(angle) * currentRadius, heightOffset, sinf(angle) * currentRadius);
            drone.targetPosition = bossTrans.position + offset;
        }
    }
}


// 物理の更新より前に座標を計算するため、PrePhysicsを指定
REGISTER_LOGIC_SYSTEM(DroneFormationSystem, Priority::LogicStage::L03_PrePhysics);