#include "DistractionItemComponent.h"
#include "Editor/Inspector/ComponentGuiRegistry.h"
#include "Engine/Serialization/ComponentRegistry.h"
#include "Engine/Serialization/Meta/ComponentMeta.h"
#include "Engine/Serialization/Meta/ComponentMetaImGui.h"
#include "Engine/Serialization/Meta/ComponentMetaJson.h"
#include "Engine/Core/Math/StringHash.h" // プロジェクトのHashString関数

#include <imgui.h>

template <> struct ComponentMeta<DistractionItemComponent> {
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "Distraction Item (Sound)";
    static constexpr bool        hasCustomGui = true;

    static bool CustomGui(DistractionItemComponent& comp, unsigned long long entityID = 0, void* worldPtr = nullptr) {
        bool changed = false;

        ImGui::SeparatorText("Item Settings");

        changed |= ImGui::DragFloat("Volume Radius (m)", &comp.volumeRadius, 0.5f, 1.0f, 100.0f, "%.1f m");

        changed |= ImGui::DragFloat("BounceVelocityThreshold (m)", &comp.bounceVelocityThreshold, 0.5f, 1.0f, 100.0f, "%.1f m");

        ImGui::Spacing();
        ImGui::SeparatorText("FMOD Sound Event");

        // --- FMOD パス入力 UI ---
        static char pathBuffer[256] = "";
        ImGui::Text("FMOD Event Path");
        if (ImGui::InputText("##FMODPath", pathBuffer, sizeof(pathBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (strlen(pathBuffer) > 0) {
                // Enterキーが押されたらハッシュ化して保存
                comp.fmodEventHash = CCL::Utils::HashString(pathBuffer);
                changed = true;
                pathBuffer[0] = '\0'; // 入力欄をクリア
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Example: event:/SFX/ItemDrop\nType the FMOD path and press [Enter].");
        }

        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Hash ID: 0x%08X", comp.fmodEventHash);


        ImGui::Spacing();
        ImGui::SeparatorText("Runtime Status");

        if (comp.hasTriggered) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "[ SOUND TRIGGERED ]");
        }
        else {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[ READY TO RING ]");
        }

        changed |= ImGui::Checkbox("Has Triggered", &comp.hasTriggered);

        return changed;
    }

    static const std::vector<FieldDescriptor>& Fields() {
        static const std::vector<FieldDescriptor> fields = {
            META_FIELD_FLOAT(DistractionItemComponent, volumeRadius, "volumeRadius", "Volume Radius", 0.5f, 1.0f, 100.0f, "Settings"),

            // ★追加: ハッシュ値をセーブデータ(JSON)に保存する
            // UInt32 の保存マクロがない場合は INT を代用するか、エンジンの仕様に合わせます
            { "FMOD Hash", "fmodEventHash", FieldKind::Int, offsetof(DistractionItemComponent, fmodEventHash), 1.0f, 0.0f, 0.0f, "Audio", nullptr, 0, true },

            META_FIELD_FLOAT(DistractionItemComponent, bounceVelocityThreshold, "bounceVelocityThreshold", "BounceVelocityThreshold", 0.5f, 1.0f, 100.0f, "Settings"),
        };
        return fields;
    }
};

REGISTER_COMPONENT(DistractionItemComponent, "DistractionItem")