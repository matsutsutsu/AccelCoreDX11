#pragma once
#include "ECS/System/CCL_System.h"

/**
 * @file HitboxCollisionSystem.h
 * @brief 物理衝突からダメージイベントを生成する「判定」システム
 *
 * =================================================================================
 * 【戦闘・ダメージ処理パイプライン 全体フロー】
 * * [Jolt Physics] (物理的な重なりを検知し、EventBusへ送信)
 * │
 * [ Phase 1: 判定 ] ★ HitboxCollisionSystem (L05_Collision) <--- [現在地]
 * │  - Hitbox と Hurtbox の交差を検証。
 * │  - 多段ヒットを防止しつつ、一時的な「DamageEventComponent」エンティティを生成。
 * ▼
 * [ Phase 2: 解決 ] DamageResolutionSystem (L06_Resolution)
 * │  - DamageEvent をシーケンシャルに収集。
 * │  - ターゲットの無敵時間を加味して Health の HP を減算し、イベントを破棄。
 * ▼
 * [ Phase 3: 管理 ] HealthSystem (L06_Resolution)
 * - Health の無敵タイマーを更新。
 * - HPが0以下の対象に DeadTag を付与。
 * =================================================================================
 *
 * @note
 * このシステムは対象の状態（HPや無敵）を一切直接変更しない。
 * 「誰が誰にどれだけのダメージを与えるか」という純粋な「出来事（Event）」を
 * メモリ上にデータとして並べることのみに特化している（DODの原則）。
 */

class HitboxCollisionSystem : public CCL::ECS::IfSystem<HitboxCollisionSystem> {
public:
    HitboxCollisionSystem() : IfSystem("HitboxCollisionSystem") {}

    void Initialize() override;
    void Update(float dt) override;

private:
    void CheckAndSpawnDamageEvent(CCL::ECS::EntityID entityA, CCL::ECS::EntityID entityB);
};