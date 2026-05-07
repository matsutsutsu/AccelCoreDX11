/**
 * @file BossActionComponentMeta.cpp
 */
#include "BossActionComponent.h"
#include "Editor/Inspector/ComponentGuiRegistry.h"
#include "Engine/Serialization/ComponentRegistry.h"
#include "Engine/Serialization/Meta/ComponentMeta.h"
#include <iterator> // std::size用

 // ============================================================================
 // ★修正: 状態のプルダウン表示用に「JumpAttack」「Evade」「Flinch」を追加
 // ============================================================================
static const char* BossStateNames[] = {
    "None", "Move", "Melee", "Charge", "JumpAttack", "Evade", "Flinch"
};

template <> struct ComponentMeta<BossActionComponent> {
    static constexpr bool registered = true;
    static constexpr const char* displayName = "Boss Action (Muscle)";
    static constexpr bool hasCustomGui = false;
    static constexpr bool isSerializable = true;

    static const std::vector<FieldDescriptor>& Fields() {
        static const std::vector<FieldDescriptor> fields = {
            { "歩行速度", "walkSpeed", FieldKind::Float, offsetof(BossActionComponent, walkSpeed), 0.1f, 0.0f, 100.0f, "Stats", nullptr, 0, true },
            { "突進速度", "chargeSpeed", FieldKind::Float, offsetof(BossActionComponent, chargeSpeed), 0.1f, 0.0f, 100.0f, "Stats", nullptr, 0, true },
            { "旋回速度", "turnSpeed", FieldKind::Float, offsetof(BossActionComponent, turnSpeed), 0.1f, 0.0f, 100.0f, "Stats", nullptr, 0, true },

            // ============================================================================
            // ★追加: 回避速度をインスペクタで調整できるようにする
            // ============================================================================
            { "回避速度", "evadeSpeed", FieldKind::Float, offsetof(BossActionComponent, evadeSpeed), 0.1f, 0.0f, 100.0f, "Stats", nullptr, 0, true },

            { "現在の移動速度", "currentMoveSpeed", FieldKind::Float, offsetof(BossActionComponent, currentMoveSpeed), 0.1f, 0.0f, 100.0f, "Inertia", nullptr, 0, false },
            { "加速度", "acceleration", FieldKind::Float, offsetof(BossActionComponent, acceleration), 0.1f, 0.0f, 100.0f, "Inertia", nullptr, 0, true },
            { "減速度", "deceleration", FieldKind::Float, offsetof(BossActionComponent, deceleration), 0.1f, 0.0f, 100.0f, "Inertia", nullptr, 0, true },

            { "ジャンプ攻撃の頂点の高さ", "jumpAttackApexHeight", FieldKind::Float, offsetof(BossActionComponent, jumpAttackApexHeight), 0.1f, 0.0f, 100.0f, "Visuals", nullptr, 0, true },
            { "チャージ前の浮遊初速", "chargeHoverVelocity", FieldKind::Float, offsetof(BossActionComponent, chargeHoverVelocity), 0.1f, 0.0f, 100.0f, "Visuals", nullptr, 0, true },

            // ============================================================================
            // ★修正: 配列の要素数を固定値(4)から、動的に取得するよう変更（これで今後増えても安心です）
            // ============================================================================
            { "現在の状態", "currentState", FieldKind::EnumU8, offsetof(BossActionComponent, currentState), 1.0f, 0.0f, 0.0f, "State", BossStateNames, std::size(BossStateNames), false },

            { "硬直タイマー", "actionTimer", FieldKind::Float, offsetof(BossActionComponent, actionTimer), 0.1f, 0.0f, 0.0f, "State", nullptr, 0, false },
        };
        return fields;
    }
};

REGISTER_COMPONENT(BossActionComponent, "BossActionComponent")