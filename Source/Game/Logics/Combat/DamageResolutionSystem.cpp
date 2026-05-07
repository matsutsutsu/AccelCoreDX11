/**
 * @file DamageResolutionSystem.cpp
 */
#include "DamageResolutionSystem.h"
#include "Game/Logic/Combat/CombatComponents.h"
#include "Game/Logic/Combat/HealthComponent.h"
#include "ECS/Core/CCL_World.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"
#include "Engine/Platform/Logger.h"

 // ヒットストップを付与するために時間をインクルード
#include "Engine/GamePlay/Core/Time/TimeState.h" 
#include <algorithm> // std::max 用
#include <imgui.h>

// 脊髄反射（被弾モーション）を直接叩くためのインクルード
#include "Engine/GamePlay/Animation/AnimParametersComponent.h"
#include "Game/Logic/AI/BehaviorTree/BossActionComponent.h" 
#include "Game/Logic/AI/BehaviorTree/Data/BehaviorTreeComponents.h" 
#include "Engine/Core/Math/StringHash.h"

using namespace CCL::ECS;

void DamageResolutionSystem::Update(float dt) {

    // 一時的に生成されたダメージイベントを全て走査
    ForEachWithID([this](EntityID eventEntity, const DamageEventComponent& event) {

        // ターゲットのHealthを取得 (ランダムアクセスだが、L1/L2キャッシュの恩恵は受けやすい)
        if (auto* health = _world->GetComponent<HealthComponent>(event.targetID)) {

            // 無敵時間中ならダメージを無効化（処理はスキップされるがイベントは破棄される）
            if (health->invincibilityTimer <= 0.0f) {

                // HPの減算
                health->currentHealth -= event.finalDamage;

                // ダメージを受けた瞬間に無敵時間をリセット（多段ヒット・ハメ防止）
                health->invincibilityTimer = health->invincibilityDuration;

                CCL_LOG_INFO(LogCategory::Game, "Entity %llu took %f damage. Remaining HP: %f",
                    event.targetID, event.finalDamage, health->currentHealth);


                // ========================================================
                // ★ 1.5 脊髄反射（被弾モーションとスーパーアーマー判定）
                // ========================================================
                auto* bossAction = _world->GetComponent<BossActionComponent>(event.targetID);
                auto* animParams = _world->GetComponent<AnimParametersComponent>(event.targetID);
				auto* btComp = _world->GetComponent<BehaviorTreeComponent>(event.targetID);

                // =========================================================
                // ダメージの蓄積（AIの記憶への書き込み）
                // スーパーアーマーで怯まなかった（Flinchしなかった）としても、
                // 受けたダメージ量そのものは、確実にAIの脳（Blackboard）に蓄積させる。
                // =========================================================
                if (btComp) {
                    btComp->blackboard.accumulatedDamage += event.finalDamage;
                }

                // ターゲットがボスであれば、物理ステートの上書きとトリガー発火を行う
                if (bossAction && animParams) {
                    // スーパーアーマー判定（待機中 か 歩行中 の時だけ怯む）
                    bool canFlinch = (bossAction->currentState == BossActionState::None ||
                        bossAction->currentState == BossActionState::Move);

                    if (canFlinch) {
                        // 脊髄反射: 強制的に怯み状態へ移行させ、物理的な慣性を殺す
                        bossAction->currentState = BossActionState::Flinch;
                        bossAction->actionTimer = BossTimings::Flinch_Duration;
                        bossAction->currentMoveSpeed = 0.0f;

                        // アニメーターへ「Damage」トリガーを送信
                        // （AnimControllerSystemの "Any State" がこれをキャッチし、即座に被弾モーションへ遷移させる）
                        animParams->SetTrigger(CCL::Utils::HashString("Damage"));

                        // =========================================================
                        // ★追加: 脳震盪（BTの短期記憶リセット）
                        // =========================================================
                        if (btComp) {
                            // すべてのノードの進行状態を未実行（-1.0f）にクリアする
                            std::fill(btComp->nodeTimers.begin(), btComp->nodeTimers.end(), -1.0f);
                            // 記憶（しおり）も全て白紙に戻す
                            std::fill(btComp->runningNodes.begin(), btComp->runningNodes.end(), -1);


                            // 実行中のノードIDをリセット（0xFFFFは未実行の番兵値）
                            btComp->runningNodeId = 0xFFFF;
                            btComp->previousRunningNodeId = 0xFFFF;

                            OutputDebugStringA("[DamageSystem] Boss got a concussion. BT Memory Reset.\n");
                        }

                        // =========================================================
                        // ★ 究極のハック: ボスが怯んだら、ドローンへの命令も強制的にリセット（退避）する
                        // これにより、古い命令のままドローンがプレイヤーを理不尽に襲うのを防ぐ！
                        // =========================================================
                        auto* bossCmd = _world->GetComponent<BossCommandComponent>(event.targetID);
                        if (bossCmd) {
                            // ドローンをボスの頭上高くに待機（HighOrbit）させ、一旦攻撃を仕切り直す
                            bossCmd->requestFormation = DroneFormationType::HighOrbit;
                        }
                    }
                    else {
                        // 大技の最中は怯まない（スーパーアーマー）
                        CCL_LOG_INFO(LogCategory::Game, "Boss Super Armored!");
                    }
                }


                // ========================================================
                // ★ 2. ヒットストップ（時間操作）の発火
                // ========================================================
                if (event.hitStopDuration > 0.0f) {

                    // 親時計（世界全体）へのヒットスロー命令
                    if (_world->HasResource<TimeContext>()) {
                        auto& ctx = _world->GetResource<TimeContext>();
                        // 全体を 0.05倍 (ほぼ停止に近い超スロー) にする
                        ctx.globalHitStopTimer = (std::max)(ctx.globalHitStopTimer, event.hitStopDuration);
                        ctx.globalFreezeScale = 0.05f;
                    }

                    // 【被弾側（敵）の処理】: 相手の時間を止める
                    _world->PatchComponent<TimeState>(event.targetID, [&event](TimeState& time) {
                        // 連続ヒット時に時間が短くならないよう、常に長い方を採用する
                        time.hitStopTimer = (std::max)(time.hitStopTimer, event.hitStopDuration);
                        time.freezeScale = event.hitStopFreezeScale;
                        });

                    // 【攻撃側（プレイヤー）の処理】: 自分の時間も止める（重い手応え）
                    if (_world->IsEntityValid(event.attackerID)) {
                        _world->PatchComponent<TimeState>(event.attackerID, [&event](TimeState& time) {
                            // 攻撃側は敵より少し短くするか、同じにするかはゲーム性次第
                            time.hitStopTimer = (std::max)(time.hitStopTimer, event.hitStopDuration);
                            time.freezeScale = event.hitStopFreezeScale;
                            });
                    }
                }
                // ========================================================

                // 【拡張ポイント】
                // ここでヒットエフェクトやカメラシェイクのRequest(一時エンティティ)を
                // PendingOps経由でSpawnさせると、完全に疎結合なVFXシステムが構築できる。
            }
        }

        // ★最重要: 処理が終わった「イベントエンティティ」は必ず破棄する
        _world->Destroy(eventEntity);
        });
}

// ========================================================================
// 修正: エディタ用のデバッグGUI描画処理
// ========================================================================
void DamageResolutionSystem::OnGui() {
    if (ImGui::CollapsingHeader("Damage & Flinch Debugger", ImGuiTreeNodeFlags_DefaultOpen)) {

        // ========================================================
        // ★新規追加: 手動ダメージテスト機能
        // ========================================================
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "Manual Damage Test");

        // HealthComponentを持つすべてのエンティティを取得
        auto view = _world->View<HealthComponent>();
        if (view.empty()) {
            ImGui::TextDisabled("No Health entities found.");
        }
        else {
            for (auto entity : view) {
                auto* health = _world->GetComponent<HealthComponent>(entity);
                auto* bossAction = _world->GetComponent<BossActionComponent>(entity);

                ImGui::PushID(static_cast<int>(entity));

                // エンティティIDと現在のHPを表示
                ImGui::Text("Entity [%llu] HP: %.1f/%.1f", entity, health->currentHealth, health->maxHealth);

                // ボスの場合は現在の物理ステート（筋肉の状態）もリアルタイム表示する
                if (bossAction) {
                    ImGui::SameLine();
                    const char* stateStr = "Other";
                    if (bossAction->currentState == BossActionState::None) stateStr = "None";
                    else if (bossAction->currentState == BossActionState::Move) stateStr = "Move";
                    else if (bossAction->currentState == BossActionState::Melee) stateStr = "Melee";
                    else if (bossAction->currentState == BossActionState::Charge) stateStr = "Charge";
                    else if (bossAction->currentState == BossActionState::JumpAttack) stateStr = "JumpAttack";
                    else if (bossAction->currentState == BossActionState::Evade) stateStr = "Evade";
                    else if (bossAction->currentState == BossActionState::Flinch) stateStr = "Flinch";

                    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "[State: %s]", stateStr);
                }

                // 右端にダメージボタンを配置
                ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 100.0f);
                if (ImGui::Button("Deal 10 DMG", ImVec2(90, 0))) {

                    // ECSのパイプラインに則り、擬似的なダメージイベントをSpawnする
                    static const CCL::ECS::Archetype damageArchetype = CCL::ECS::ArchetypeHelper::Generate<DamageEventComponent>();
                    auto entityRef = _world->Spawn(damageArchetype);

                    entityRef.Set(DamageEventComponent{
                        entity,                     // targetID (この行のエンティティ)
                        CCL::ECS::InvalidEntityID,  // attackerID (デバッグなので無効値)
                        10.0f,                      // finalDamage (10ダメージ)
                        0.0f,                       // hitStopDuration (デバッグ用なのでストップ無し)
                        0.0f                        // hitStopFreezeScale
                        });
                }
                ImGui::PopID();
            }
        }

        ImGui::Separator();
    }
}


// L05の判定システムの直後、かつHealthSystem(死亡判定など)の前に実行する
REGISTER_LOGIC_SYSTEM(DamageResolutionSystem, Priority::LogicStage::L06_DamageApply);