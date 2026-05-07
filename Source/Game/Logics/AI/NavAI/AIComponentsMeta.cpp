#include "Game/Logics/AI/NavAI/AIComponents.h"
#include "Editor/Inspector/ComponentGuiRegistry.h"
#include "Engine/Serialization/ComponentRegistry.h"
#include "Engine/Serialization/Meta/ComponentMeta.h"
#include "Engine/Serialization/Meta/ComponentMetaImGui.h"
#include "Engine/Serialization/Meta/ComponentMetaJson.h"

#include <imgui.h>

// ============================================================================
// 1. 脳の状態 (AI State)
// ============================================================================
template <> struct ComponentMeta<AIStateComponent> {
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "AI State / Brain";
    static constexpr bool        hasCustomGui = true;

    static bool CustomGui(AIStateComponent& comp, unsigned long long entityID = 0, void* worldPtr = nullptr) {
        bool changed = false;

        // --- ランタイム状態の表示 (Read Only) ---
        ImGui::SeparatorText("Runtime Status");
        const char* stateNames[] = { "Idle", "Patrol", "Investigate", "Chase", "AttackDoor", "AmbushDuct" };
        int stateIdx = static_cast<int>(comp.currentState);
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "Current State: %s", stateNames[stateIdx]);
        ImGui::Text("Time in State: %.2f s", comp.timeInState);

        ImGui::Spacing();

        // --- パラメータ調整 (Editable) ---
        ImGui::SeparatorText("Movement Speed Settings");

        // DragFloat を使うことで、細かい単位での調整が可能になります
        // 速度がマイナスになると物理エンジンが破綻するため、0.0f を最小値に設定します
        changed |= ImGui::DragFloat("Patrol Speed", &comp.patrolSpeed, 0.05f, 0.0f, 20.0f, "%.2f m/s");
        changed |= ImGui::DragFloat("Investigate Speed", &comp.investigateSpeed, 0.05f, 0.0f, 20.0f, "%.2f m/s");
        changed |= ImGui::DragFloat("Chase Speed", &comp.chaseSpeed, 0.05f, 0.0f, 30.0f, "%.2f m/s");

        ImGui::Spacing();
        ImGui::SeparatorText("Patrol Parameters");
        changed |= ImGui::DragFloat("Patrol Radius", &comp.patrolRadius, 0.5f, 1.0f, 100.0f);
        changed |= ImGui::DragFloat("Wait Time", &comp.patrolWaitTime, 0.1f, 0.0f, 60.0f);

        return changed;
    }

    static const std::vector<FieldDescriptor>& Fields() {
        static const std::vector<FieldDescriptor> fields = {
            // META_FIELD_FLOAT(ClassType, FieldName, JsonName, DisplayName, Speed, Min, Max, Category)

            // --- 移動速度 (Speedを0.05fにして微調整しやすくする) ---
            META_FIELD_FLOAT(AIStateComponent, patrolSpeed,      "patrolSpeed",      "Patrol Speed",      0.05f, 0.0f, 20.0f, "Movement"),
            META_FIELD_FLOAT(AIStateComponent, investigateSpeed, "investigateSpeed", "Investigate Speed", 0.05f, 0.0f, 20.0f, "Movement"),
            META_FIELD_FLOAT(AIStateComponent, chaseSpeed,       "chaseSpeed",       "Chase Speed",       0.05f, 0.0f, 30.0f, "Movement"),

            // --- 巡回ロジック (Radiusは0.5f刻み、時間は0.1秒刻み) ---
            META_FIELD_FLOAT(AIStateComponent, patrolRadius,     "patrolRadius",     "Patrol Radius",     0.5f,  1.0f, 100.0f, "Logic"),
            META_FIELD_FLOAT(AIStateComponent, patrolWaitTime,   "patrolWaitTime",   "Wait Time",         0.1f,  0.0f, 60.0f,  "Logic")
        };
        return fields;
    }
};

REGISTER_COMPONENT(AIStateComponent, "AIState")


// ============================================================================
// 2. 知覚能力 (AI Perception)
// ============================================================================
template <> struct ComponentMeta<AIPerceptionComponent> {
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "AI Perception";

    // 特殊なデバッグ表示が不要なため、メタシステムの自動描画に任せる
    static constexpr bool        hasCustomGui = false;

    static const std::vector<FieldDescriptor>& Fields() {
        static const std::vector<FieldDescriptor> fields = {
            META_FIELD_FLOAT(AIPerceptionComponent, visionRange, "visionRange", "Vision Range (m)", 0.5f, 0.0f, 100.0f, "Vision"),
            META_FIELD_FLOAT(AIPerceptionComponent, visionAngle, "visionAngle", "Vision Angle (deg)", 1.0f, 0.0f, 360.0f, "Vision"),

        };
        return fields;
    }
};

REGISTER_COMPONENT(AIPerceptionComponent, "AIPerception")


// ============================================================================
// 3. 記憶と適応 (AI Memory / Meta AI)
// ============================================================================
template <> struct ComponentMeta<AIMemoryComponent> {
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "AI Memory (Meta)";
    static constexpr bool        hasCustomGui = true;

    static bool CustomGui(AIMemoryComponent& comp, unsigned long long entityID = 0, void* worldPtr = nullptr) {
        bool changed = false;

        ImGui::SeparatorText("Live Memory");

        // 怒りレベルをプログレスバーとして視覚化
        ImGui::Text("Anger Level:");
        ImGui::ProgressBar(comp.currentAngerLevel / 100.0f, ImVec2(-1, 0), "");

        // 最後に確認した座標の表示 (lastKnownPlayerPos -> lastKnownPos へ修正)
        ImGui::TextDisabled("Last Known Pos: (%.1f, %.1f, %.1f)",
            comp.lastKnownPos.x, comp.lastKnownPos.y, comp.lastKnownPos.z);

        ImGui::Spacing();
        ImGui::SeparatorText("Adaptation Stats (Learned)");

        // デバッグ目的で適応レベルを手動でいじれるようにしておく
        // ImGui::InputInt にはラベル名と変数のポインタの2つの引数が必要
        changed |= ImGui::InputInt("Tricked By Sound", &comp.trickedBySoundCount);
        changed |= ImGui::InputInt("Duct Usage Observed", &comp.ductUsageObserved);

        return changed;
    }

    static const std::vector<FieldDescriptor>& Fields() {
        static const std::vector<FieldDescriptor> fields = {
            // 新企画の変数名に合わせたJSONセーブデータのマッピング
            META_FIELD_INT(AIMemoryComponent, trickedBySoundCount, "trickedBySoundCount", "Tricked By Sound", 0, 1000, "Adaptation"),
            META_FIELD_INT(AIMemoryComponent, ductUsageObserved, "ductUsageObserved", "Duct Usage Observed", 0, 1000, "Adaptation")
        };
        return fields;
    }
};

REGISTER_COMPONENT(AIMemoryComponent, "AIMemory")