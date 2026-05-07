/**
 * @file TestEnemyActionComponentMeta.cpp
 * @brief テスト用敵アクションステータスのメタデータ登録
 */
#include "TestEnemyActionComponent.h"
#include "Editor/Inspector/ComponentGuiRegistry.h"
#include "Engine/Serialization/ComponentRegistry.h"
#include "Engine/Serialization/Meta/ComponentMeta.h"

template <> struct ComponentMeta<TestEnemyActionComponent> {
    static constexpr bool registered = true;
    static constexpr const char* displayName = "Test Enemy Action";
    static constexpr bool hasCustomGui = false;
    static constexpr bool isSerializable = true;

    static const std::vector<FieldDescriptor>& Fields() {
        static const std::vector<FieldDescriptor> fields = {
            { "歩行速度", "walkSpeed", FieldKind::Float, offsetof(TestEnemyActionComponent, walkSpeed), 0.1f, 0.0f, 100.0f, "Stats", nullptr, 0, true },
            { "突進速度", "chargeSpeed", FieldKind::Float, offsetof(TestEnemyActionComponent, chargeSpeed), 0.1f, 0.0f, 100.0f, "Stats", nullptr, 0, true },
            { "旋回速度", "turnSpeed", FieldKind::Float, offsetof(TestEnemyActionComponent, turnSpeed), 0.1f, 0.0f, 100.0f, "Stats", nullptr, 0, true },
            // ★ currentState を EnumU8 として登録
            { "現在の状態", "currentState", FieldKind::EnumU8, offsetof(TestEnemyActionComponent, currentState), 1.0f, 0.0f, 0.0f, "State", 
				new const char* [] { "None", "Melee", "Charge" }, 3, true },
            { "攻撃硬直タイマー", "attackTimer", FieldKind::Float, offsetof(TestEnemyActionComponent, attackTimer), 0.1f, 0.0f, 0.0f, "State", nullptr, 0, false },
            { "ウロウロ用タイマー", "moveTimer", FieldKind::Float, offsetof(TestEnemyActionComponent, moveTimer), 0.1f, 0.0f, 0.0f, "State", nullptr, 0, false }
        };
        return fields;
    }
};
REGISTER_COMPONENT(TestEnemyActionComponent, "TestEnemyActionComponent")