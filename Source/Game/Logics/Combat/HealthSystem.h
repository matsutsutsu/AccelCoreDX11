#pragma once
#include "ECS/System/CCL_System.h"
#include "Game/Logics/Combat/HealthComponent.h"

/**
 * @file HealthSystem.h
 * @brief HPの無敵時間更新と死亡判定を行う「管理」システム
 *
 * =================================================================================
 * 【戦闘・ダメージ処理パイプライン 全体フロー】
 * * [Jolt Physics] (物理的な重なりを検知し、EventBusへ送信)
 * │
 * [ Phase 1: 判定 ] HitboxCollisionSystem (L05_Collision)
 * │  - Hitbox と Hurtbox の交差を検証し、DamageEvent を生成。
 * ▼
 * [ Phase 2: 解決 ] DamageResolutionSystem (L06_Resolution)
 * │  - DamageEvent を消費し、対象の HP を減算してイベントを破棄。
 * ▼
 * [ Phase 3: 管理 ] ★ HealthSystem (L06_Resolution) <--- [現在地]
 * - 全エンティティの HealthComponent を走査。
 * - 減算された無敵タイマー（invincibilityTimer）の更新。
 * - HPが0以下の対象を発見次第、即座に DeadTag を付与する。
 * =================================================================================
 *
 * @note
 * このシステムは「誰がダメージを与えたか」を知る必要はない。
 * 非常に純粋なデータ変換（タイマーの減算とフラグ付け）のみを行うため、
 * 毒ダメージや落下ダメージなど、将来どのようなダメージソースが追加されても、
 * この HealthSystem のコードは一切変更せずに機能する。
 */

// 【解決】HPの増減と死亡判定（Tagの付与）のみを行うシステム
class HealthSystem : public CCL::ECS::IfSystem<HealthSystem,
                         CCL::ECS::Write<HealthComponent>> {
  public:
    HealthSystem() : IfSystem("HealthSystem") {}
    void Update(float dt) override;
};