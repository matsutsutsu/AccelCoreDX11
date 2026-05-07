#include "Editor/Inspector/ComponentGuiRegistry.h"
#include "Engine/Serialization/ComponentRegistry.h"
#include "Engine/Serialization/Meta/ComponentMeta.h"
#include "Engine/Serialization/Meta/ComponentMetaImGui.h"
#include "Engine/Serialization/Meta/ComponentMetaJson.h"

// プレイヤーコンポーネントと入力API
#include "../PlayerStateComponent.h"
#include "Engine/Platform/Input/IInputAPI.h"
#include "Engine/Platform/Input/InputFacade.h" // GetAxisNames等を使用する場合
#include "ECS/Core/CCL_World.h"

#include "Engine/GamePlay/Transform/TransformComponent.h"

// ============================================================================
// PlayerStateComponent Meta
// ============================================================================
template <> struct ComponentMeta<TPSPlayerStateComponent>
{
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "TPS Player State System";
    static constexpr bool        hasCustomGui = true;


    static const std::vector<FieldDescriptor>& Fields()
    {
        static const std::vector<FieldDescriptor> fields = {
            // Dodge Config
             META_FIELD_FLOAT(TPSPlayerStateComponent, configs.dodge.dashSpeed, "dodgeDashSpeed", "Dash Speed", 0.1f, 0.0f, 100.0f, "Dodge Settings"),
             META_FIELD_FLOAT(TPSPlayerStateComponent, configs.dodge.dashFriction, "dodgeFriction", "Dash Friction", 0.1f, 0.0f, 50.0f, "Dodge Settings"),
             META_FIELD_FLOAT(TPSPlayerStateComponent, configs.dodge.duration, "dodgeDuration", "Dodge Duration", 0.01f, 0.0f, 2.0f, "Dodge Settings"),
             META_FIELD_FLOAT(TPSPlayerStateComponent, configs.dodge.usedStamina, "dodgeStaminaCost", "Stamina Cost", 0.5f, 0.0f, 100.0f, "Dodge Settings"),
             META_FIELD_FLOAT(TPSPlayerStateComponent, configs.dodge.maxTurnAngle, "dodgeMaxTurnAngle", "Max Turn Angle", 0.01f, 0.0f, 3.14f, "Dodge Settings"),

             // Attack Config (新規追加)
            META_FIELD_FLOAT(TPSPlayerStateComponent, configs.attack.lungeSpeed, "LungeSpeed", "lungeSpeed", 1.0f, 0.0f, 100.0f, "Attack Settings"),
            META_FIELD_FLOAT(TPSPlayerStateComponent, configs.attack.lungerange, "lungeRange", "LungeRange", 1.0f, 0.0f, 100.0f, "Attack Settings"),
            META_FIELD_FLOAT(TPSPlayerStateComponent, configs.attack.rotationSpeed, "rotationSpeed", "RotationSpeed", 1.0f, 0.0f, 100.0f, "Attack Settings"),
            META_FIELD_INT(TPSPlayerStateComponent, configs.attack.maxComboCount, "maxComboCount","maxComboCount", 0, 10, "Attack Settings"),
            META_FIELD_FLOAT(TPSPlayerStateComponent, configs.airAttack.lungeSpeed, "airAttacklungeSpeed", "AirAttackLungeSpeed", 1.0f, 0.0f, 100.0f, "Attack Settings"),
            META_FIELD_FLOAT(TPSPlayerStateComponent, configs.airAttack.lungerange, "airAttacklungeRange", "AirAttackLungeRange", 1.0f, 0.0f, 100.0f, "Attack Settings"),
            META_FIELD_FLOAT(TPSPlayerStateComponent, configs.airAttack.rotationSpeed, "airAttackrotationSpeed", "AirAttackRotationSpeed", 1.0f, 0.0f, 100.0f, "Attack Settings"),
            META_FIELD_FLOAT(TPSPlayerStateComponent, configs.counterAttack.lungeSpeed, "counterAttackLungeSpeed", "CounterAttackLungeSpeed", 1.0f, 0.0f, 100.0f, "Attack Settings"),
            META_FIELD_FLOAT(TPSPlayerStateComponent, configs.counterAttack.lungerange, "counterAttackLungeRange", "CounterAttackLungeRange", 1.0f, 0.0f, 100.0f, "Attack Settings"),
            META_FIELD_FLOAT(TPSPlayerStateComponent, configs.counterAttack.rotationSpeed, "counterAttackRotationSpeed", "CounterAttackRotationSpeed", 1.0f, 0.0f, 100.0f, "Attack Settings"),

             // LockOn Config (新規追加)
            META_FIELD_FLOAT(TPSPlayerStateComponent, configs.Targeting.maxRange, "lockOnMaxRange", "Max Target Range", 1.0f, 0.0f, 100.0f, "LockOn Settings"),
            META_FIELD_FLOAT(TPSPlayerStateComponent, configs.Targeting.staminaCostPerSec, "lockOnStaminaCost", "Stamina Cost/Sec", 0.1f, 0.0f, 50.0f, "LockOn Settings"),

            // ChainAttack Config (新規追加)
            META_FIELD_FLOAT(TPSPlayerStateComponent, configs.chain.warpInterval, "warpInterval", "Warp Interval", 0.01f, 0.0f, 1.0f, "ChainAttack Settings"),
            META_FIELD_FLOAT(TPSPlayerStateComponent, configs.chain.finishDuration, "finishDuration", "Finish Duration", 0.01f, 0.0f, 2.0f, "ChainAttack Settings"),
            META_FIELD_FLOAT(TPSPlayerStateComponent, configs.chain.pauseDuration, "pauseDuration", "Pause Duration", 0.01f, 0.0f, 2.0f, "ChainAttack Settings"),
        };
        return fields;
    }

    static bool CustomGui(TPSPlayerStateComponent& comp, unsigned long long entityID = 0, void* world = nullptr)
    {
        bool changed = false;

        // --- 1. 現在のステートの可視化 ---
        ImGui::SeparatorText("Active State Debug");

        std::string stateName = "Unknown";
        std::visit([&stateName](auto& s) {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, StateIdle>)         stateName = "IDLE";
            else if constexpr (std::is_same_v<T, StateMove>)         stateName = "MOVE";
            else if constexpr (std::is_same_v<T, StateDodge>)        stateName = "DODGE";
            else if constexpr (std::is_same_v<T, StateAttack>)       stateName = "ATTACK";
            else if constexpr (std::is_same_v<T, StateTargeting>)       stateName = "LOCK ON (AIM)";
            else if constexpr (std::is_same_v<T, StateChainAttack>)  stateName = "CHAIN ATTACK";
            }, comp.activeState);

        ImGui::Text("Current State: "); ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "%s", stateName.c_str());
        ImGui::Value("State Timer", comp.stateTimer);

        // --- 2. 各ステート固有の実行時データ (Debug) ---
        ImGui::BeginChild("StateExecutionData", ImVec2(0, 100), true);
        std::visit([&](auto& s) {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, StateDodge>) {
                ImGui::Checkbox("isDashing", &s.isDashing);
                ImGui::InputFloat3("Dash Dir", &s.dashDir.x, "%.2f", ImGuiInputTextFlags_ReadOnly);
            }
            else if constexpr (std::is_same_v<T, StateTargeting>) {
                ImGui::Text("Targets: %u / %d", s.targetCount, StateTargeting::MAX_TARGETS);
                ImGui::ProgressBar(s.remainingRange / s.config.maxRange, ImVec2(-1, 0), "Remaining Range");
                for (uint32_t i = 0; i < s.targetCount; ++i) {
                    ImGui::Text(" [%d] Entity ID: %llu", i, (unsigned long long)s.targets[i]);
                }
            }
            else if constexpr (std::is_same_v<T, StateChainAttack>) {
                ImGui::Text("Attacking: %u / %u", s.currentIndex + 1, s.targetCount);
                ImGui::SliderFloat("Warp Timer", &s.warpTimer, 0.0f, s.config.warpInterval);
            }
            else if constexpr (std::is_same_v<T, StateAttack>) {
                ImGui::InputInt("Combo Count", &s.comboCount);
            }
            }, comp.activeState);
        ImGui::EndChild();

        ImGui::Spacing();

        // --- 3. 設定値 (Configs) の描画 ---
        const char* categories[] = { "Dodge Settings", "Attack Settings", "LockOn Settings", "ChainAttack Settings" };
        for (const char* cat : categories) {
            if (ImGui::TreeNodeEx(cat, ImGuiTreeNodeFlags_DefaultOpen)) {
                for (const auto& fd : Fields()) {
                    if (std::string(fd.category) == cat) {
                        if (ComponentMetaImGui::DrawField(fd, &comp)) changed = true;
                    }
                }
                ImGui::TreePop();
            }
        }

        return changed;
    }

};

// コンポーネントの自動登録
REGISTER_COMPONENT(TPSPlayerStateComponent, "TPSPlayerStateComponent")