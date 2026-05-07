#include "HitboxCollisionSystem.h"
#include "Engine/GamePlay/Physics/Collision/JoltCollisionEvent.h"
#include "CombatComponents.h"
#include "ECS/Core/CCL_World.h"
#include "ECS/Core/CCL_PendingOps.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Engine/Platform/Logger.h"

#include "Game/Logic/Character/Player/PlayerStateComponent.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"

/**
 * @file HitboxCollisionSystem.cpp（※各システム名に読み替える）
 * @brief パイプラインにおける本システムの役割とフローはヘッダーファイル (.h) を参照。
 * * @note 【実装の意図（Why）】
 * （例）多段ヒット防止の走査は要素数が最大8であるため、ヒープ確保を避けて
 * 固定長配列の線形探索を行っている。キャッシュに乗るため分岐予測ペナルティは無視できる。
 */



using namespace CCL::ECS;

void HitboxCollisionSystem::Initialize() {
    // EventBusから物理衝突の報告を購読する
    ListenEvent<JoltCollisionEvent>([this](const JoltCollisionEvent& ev) {
        // Aが攻撃、Bが被弾のパターン
        CheckAndSpawnDamageEvent(ev.entityA, ev.entityB);
        // Bが攻撃、Aが被弾のパターン
        CheckAndSpawnDamageEvent(ev.entityB, ev.entityA);
        });

    CCL_LOG_INFO(LogCategory::Game, "HitboxCollisionSystem Initialized.");
}

void HitboxCollisionSystem::Update(float dt) {
    // イベント駆動で処理されるため、Updateでは何もしない
    // （※もしEventBusがキューイング方式なら、ここでキューを消化する処理を書く）
}

void HitboxCollisionSystem::CheckAndSpawnDamageEvent(EntityID attackerCandidate, EntityID victimCandidate) {
    // 1. エンティティが Hitbox と Hurtbox を持っているか確認
    auto* hitbox = _world->GetComponent<HitboxComponent>(attackerCandidate);
    auto* hurtbox = _world->GetComponent<HurtboxComponent>(victimCandidate);

    // 2. 条件を満たしているかチェック
    if (hitbox && hitbox->isActive && hurtbox) {
        // プレイヤーの現在のステート情報を取得
        auto* state = _world->GetComponent<TPSPlayerStateComponent>(victimCandidate);
        // --- 回避判定の追加 ---
        // 被害者がダッシュ（回避）タグを持っているか確認
        if (_world->HasComponent<PlayerStateTag::IsDashingTag>(victimCandidate)) {

            // 【ジャスト回避判定】ダッシュ開始直後の特定のフレーム（例: 0.15秒以内）か
                // ※ stateTimerはステート遷移時に0リセットされる想定
            if (state && state->stateTimer <= 0.2f)
            {

                // ジャスト回避イベントエンティティの生成
                static const Archetype evadeArchetype = ArchetypeHelper::Generate<JustEvadeEventComponent>();
                auto eventRef = _world->Spawn(evadeArchetype);

                eventRef.Set(JustEvadeEventComponent{
                    victimCandidate,
                    hitbox->ownerID,
                    2.5f,  // slowDuration
                    0.1f   // slowScale (ウィッチタイム)
                    });

                CCL_LOG_INFO(LogCategory::Game, "Just Evade Triggered! Player[%llu]", victimCandidate);

                // ジャスト回避成立時はダメージ計算をスキップして終了
                return;
            }
            return;
        }

        // =========================================================================
        // ownerIDの遅延初期化 (Lazy Initialization)
        // 剣などの武器に ownerID が設定されていない場合、Transform階層を遡って「真の親」を自動検知してキャッシュする
        // =========================================================================
        if (hitbox->ownerID == 0 || hitbox->ownerID == CCL::ECS::InvalidEntityID) {

            CCL::ECS::EntityID currentRoot = attackerCandidate;
            auto* trans = _world->GetComponent<TransformComponent>(currentRoot);

            // 親がいなくなる（ルートに到達する）まで while ループで遡り続ける
            while (trans && trans->parentID != 0 && trans->parentID != CCL::ECS::InvalidEntityID) {
                currentRoot = trans->parentID;
                trans = _world->GetComponent<TransformComponent>(currentRoot);
            }

            // 見つけたルート(ボスやプレイヤー本体)のIDを、次回から遡らなくていいようにキャッシュ(焼き付け)する
            hitbox->ownerID = currentRoot;

            CCL_LOG_INFO(LogCategory::Game, "Hitbox ownerID auto-resolved: Weapon[%llu] -> Owner[%llu]", attackerCandidate, currentRoot);
        }
        // =========================================================================

        // 3. ダメージ計算（防御力やバフの計算もここで行うとよい）
        float actualDamage = hitbox->damageAmount * hurtbox->damageMultiplier;

        // 1. ダメージイベント専用のArchetypeを定義
        // ※ staticにすることで、毎フレームの生成コストをゼロ（キャッシュ）にします。
        static const Archetype damageArchetype = ArchetypeHelper::Generate<DamageEventComponent>();

        // 2. Worldの機能を使ってエンティティを生成
        // ※ CCL_World.h の仕様により、EntityRef という便利なラッパーが返ってきます。
        auto entityRef = _world->Spawn(damageArchetype);

        // 3. Set() を使って、生成したエンティティに直接データを流し込む
        entityRef.Set(DamageEventComponent{
            victimCandidate,     // targetID
            hitbox->ownerID,     // attackerID
            actualDamage,        // finalDamage
            // 判定箱(Hitbox)が持っているヒットストップの性能を、付箋(DamageEvent)に転写する
            hitbox->hitStopDuration,    // hitStopDuration
            hitbox->hitStopFreezeScale  // hitStopFreezeScale
            });

        // （オプション）多段ヒットを防ぐために、一度当たった相手のIDを
        // Hitbox側のリスト（固定長配列やビットマスク等）に記録する処理を入れることも可能
    }
}

// 物理の衝突報告システム(L05_Collision)と同じか、その直後に実行されるように登録
REGISTER_LOGIC_SYSTEM(HitboxCollisionSystem, Priority::LogicStage::L05_Collision);