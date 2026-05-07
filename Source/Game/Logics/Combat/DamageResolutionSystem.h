#pragma once
#include "ECS/System/CCL_System.h"
#include "Game/Logics/Combat/CombatComponents.h"

/**
 * @file DamageResolutionSystem.h
 * @brief ダメージイベントを評価し、HPの減算を行う「解決」システム
 *
 * =================================================================================
 * 【戦闘・ダメージ処理パイプライン 全体フロー】
 * * [Jolt Physics] (物理的な重なりを検知し、EventBusへ送信)
 * │
 * [ Phase 1: 判定 ] HitboxCollisionSystem (L05_Collision)
 * │  - Hitbox と Hurtbox の交差を検証し、DamageEvent を生成。
 * ▼
 * [ Phase 2: 解決 ] ★ DamageResolutionSystem (L06_Resolution) <--- [現在地]
 * │  - 生成された DamageEvent の Chunk をシーケンシャルに走査。
 * │  - ターゲットの無敵時間を加味して Health の HP を減算。
 * │  - 処理が完了した DamageEvent エンティティを即座に破棄（燃やして捨てる）。
 * ▼
 * [ Phase 3: 管理 ] HealthSystem (L06_Resolution)
 * - Health の無敵タイマーを更新。
 * - HPが0以下の対象に DeadTag を付与。
 * =================================================================================
 *
 * @warning
 * このシステム内で、ヒットエフェクトや音の再生等の「関数の直接呼び出し」を行ってはならない。
 * 視覚的なフィードバックが必要な場合は、ここからさらに VFXRequestComponent 等を生成し、
 * 次のパイプライン（描画フェーズ）へデータを流すこと。
 */

class DamageResolutionSystem : public CCL::ECS::IfSystem<DamageResolutionSystem,
                            CCL::ECS::Write<DamageEventComponent>> {
public:
    DamageResolutionSystem() : IfSystem("DamageResolutionSystem") 
    {
        hasGui = true;
    }

    void Update(float dt) override;

    void OnGui() override;
};

//: public CCL::ECS::IfSystem<PlayerInputSystem, CCL::ECS::Write<PlayerComponent>>{