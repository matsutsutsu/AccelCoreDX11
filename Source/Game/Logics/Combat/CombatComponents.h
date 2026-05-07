/**
 * @file CombatComponents.h
 * @brief 戦闘に関する判定（Hitbox/Hurtbox）とダメージイベントのデータ構造
 */
#pragma once
#include "ECS/Common/CCL_Common.h"

 // --- 1. 攻撃側の判定データ ---
struct HitboxComponent {
    CCL::ECS::EntityID ownerID;     // 誰の攻撃か（剣ならプレイヤーのID）
    float damageAmount;   // 基礎ダメージ量
    bool isActive = false;        // 判定が生きているか（アニメーションからON/OFFする）

    // ★多段ヒット防止用の固定長配列
    static constexpr uint8_t MAX_HIT_TARGETS = 8;
    CCL::ECS::EntityID hitTargets[MAX_HIT_TARGETS]; // ヒットした対象のIDリスト
    uint8_t hitCount; // 現在ヒットした数

    // ※アニメーションが開始し、isActiveがtrueになる瞬間に
    // hitCount = 0 にリセットすることで、次の攻撃で再度当たるようになる。
};

// --- 2. 被弾側の判定データ ---
struct HurtboxComponent {
    float damageMultiplier = 1.0f; // 弱点なら2.0など
};

// --- 3. ダメージイベント・エンティティ用コンポーネント（★重要） ---
struct DamageEventComponent {
    CCL::ECS::EntityID targetID;      // 誰がダメージを受けるか
    CCL::ECS::EntityID attackerID;    // 誰が攻撃したか（キルログや吸収などに使用）
    float finalDamage;      // 計算済みの最終ダメージ量
    // ※必要に応じてヒット位置(Vector3)などを追加可能
};
