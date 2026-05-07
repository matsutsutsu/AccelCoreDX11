
#include "BlackboardComponent.h"
#include "Editor/Inspector/ComponentGuiRegistry.h"
#include "Engine/Serialization/ComponentRegistry.h"
#include "Engine/Serialization/Meta/ComponentMeta.h"
#include "Engine/Serialization/Meta/ComponentMetaImGui.h"
#include "Engine/Serialization/Meta/ComponentMetaJson.h"

#include <imgui.h>


// ===================================================================
// BlackboardComponent のインスペクタUIおよびシリアライズ定義
// ===================================================================
template <> struct ComponentMeta<BlackboardComponent> {
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "Blackboard Component";
    static constexpr bool        hasCustomGui = true;
    static constexpr bool        isSerializable = true;

    static bool CustomGui(BlackboardComponent& bb, unsigned long long entityID, void* worldPtr)
    {
        bool changed = false;

        // --- 1. Boolean Flags ---
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Boolean Flags");
        ImGui::Separator();

        // よく使うフラグは個別に名前付きで表示するとデバッグが捗ります
        // 例: "IsInvincible"_hash を使って直接アクセス
        auto drawFlag = [&](const char* label, uint32_t hash) {
            bool val = bb.bools.count(hash) ? bb.bools[hash] : false;
            if (ImGui::Checkbox(label, &val)) {
                bb.bools[hash] = val;
                return true;
            }
            return false;
            };

        // 開発中によく確認するフラグを明示
        changed |= drawFlag("IsInvincible (Test)", "IsInvincible"_hash);
        changed |= drawFlag("CanCancel (Test)", "CanCancel"_hash);

        ImGui::Spacing();
        ImGui::TextDisabled("Raw Bools (Count: %d)", (int)bb.bools.size());
        for (auto& [hash, value] : bb.bools) {
            ImGui::Text("  0x%08X: %s", hash, value ? "true" : "false");
        }

        ImGui::Separator();

        // --- 2. String Parameters ---
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "String Parameters");
        if (bb.strings.empty()) {
            ImGui::TextDisabled("No strings.");
        }
        else {
            for (auto& [hash, value] : bb.strings) {
                ImGui::Text("0x%08X: %s", hash, value.c_str());
            }
        }

        if (ImGui::Button("Reset All")) {
            bb.bools.clear();
            bb.strings.clear();
            changed = true;
        }

        return changed;
    }

    static const std::vector<FieldDescriptor>& Fields() {
        static const std::vector<FieldDescriptor> fields = {};
        return fields;
    }
};

// エンジンのファクトリにコンポーネントを登録
REGISTER_COMPONENT(BlackboardComponent, "Blackboard")