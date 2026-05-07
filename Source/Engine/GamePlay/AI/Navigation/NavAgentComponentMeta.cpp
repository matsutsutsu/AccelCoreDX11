#include "Engine/GamePlay/AI/Navigation/NavAgentComponent.h"
#include "Editor/Inspector/ComponentGuiRegistry.h"
#include "Engine/Serialization/ComponentRegistry.h"
#include "Engine/Serialization/Meta/ComponentMeta.h"
#include "Engine/Serialization/Meta/ComponentMetaImGui.h"
#include "Engine/Serialization/Meta/ComponentMetaJson.h"



template <> struct ComponentMeta<NavAgentComponent> {
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "Nav Agent";

    // カスタムGUIを有効にして、デバッグ情報やボタンを描画する
    static constexpr bool        hasCustomGui = true;

    // ========================================================================
    // インスペクター（ImGui）上の描画ロジック
    // ========================================================================
    static bool CustomGui(NavAgentComponent& comp, unsigned long long entityID = 0, void* worldPtr = nullptr)
    {
        bool changed = false;

        ImGui::SeparatorText("Navigation Target");

        // 目的地の設定
        if (ImGui::DragFloat3("Target Position", &comp.targetPosition.x, 0.1f)) {
            changed = true;
        }

        ImGui::Spacing();

        // エディタ上で手動で経路計算を走らせるためのテストボタン
        if (ImGui::Button("Force Path Request", ImVec2(-1, 0))) {
            comp.isPathRequested = true;
            changed = true;
        }

        // ========================================================================
        // デバッグ情報の表示 (読み取り専用)
        // ========================================================================
        ImGui::Spacing();
        ImGui::SeparatorText("Debug Info");

        // 計算要求中かどうかを視覚的に表示
        if (comp.isPathRequested) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Status: Path Requested...");
        }
        else {
            ImGui::TextDisabled("Status: Idle / Moving");
        }

        // 現在のウェイポイントの数と、どこに向かっているかのインデックスを表示
        ImGui::Text("Waypoints Count: %zu", comp.waypoints.size());
        ImGui::Text("Current Index: %d", comp.currentWaypointIndex);

		ImGui::Text("currentDesiredSpeed: %.2f", comp.currentDesiredSpeed);
		ImGui::Text("stopDistance: %.2f", comp.stopDistance);

        return changed;
    }

    // ========================================================================
    // JSONセーブデータに保存する項目 (シリアライズ対象)
    // ========================================================================
    static const std::vector<FieldDescriptor>& Fields()
    {
        static const std::vector<FieldDescriptor> fields = {
            // targetPosition のみセーブデータに保存する。
            // (waypointsやindexなどの状態はランタイムで計算されるため保存しないのが正解)
            META_FIELD_FLOAT3(NavAgentComponent, targetPosition, "targetPosition", "Target Position", 0.1f, "AI")
        };
        return fields;
    }
};

// ============================================================================
// 究極の自動化：これだけでJSONとImGuiの両方に登録される
// ============================================================================
REGISTER_COMPONENT(NavAgentComponent, "NavAgent")