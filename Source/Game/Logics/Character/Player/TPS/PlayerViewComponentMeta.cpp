#include "Editor/Inspector/ComponentGuiRegistry.h"
#include "Engine/Serialization/ComponentRegistry.h"
#include "Engine/Serialization/Meta/ComponentMeta.h"
#include "Engine/Serialization/Meta/ComponentMetaImGui.h"
#include "Engine/Serialization/Meta/ComponentMetaJson.h"

// プレイヤーコンポーネントと入力API
#include "PlayerViewComponent.h"

// ============================================================================
// FPS Player View Component Meta
// ============================================================================
template <> struct ComponentMeta<PlayerViewComponent>
{
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "Player View Settings";
    static constexpr bool        hasCustomGui = true;

    // JSONシリアライズ対象（固定パラメータ）
    static const std::vector<FieldDescriptor>& Fields()
    {
        static const std::vector<FieldDescriptor> fields = {
            // FOV Settings
            META_FIELD_FLOAT(PlayerViewComponent, baseFOV, "baseFOV", "Base FOV", 0.5f, 30.0f, 120.0f, "Visuals"),
            META_FIELD_FLOAT(PlayerViewComponent, fovInterpSpeed, "fovInterpSpeed", "FOV Interp Speed", 0.1f, 0.0f, 30.0f, "Visuals"),

            // Idle Bobbing
            META_FIELD_FLOAT(PlayerViewComponent, idleBobSpeed, "idleBobSpeed", "Idle Speed", 0.1f, 0.0f, 10.0f, "Bobbing - Idle"),
            META_FIELD_FLOAT(PlayerViewComponent, idleBobAmount, "idleBobAmount", "Idle Amount", 0.001f, 0.0f, 0.5f, "Bobbing - Idle"),

            // Walk Bobbing
            META_FIELD_FLOAT(PlayerViewComponent, walkBobSpeed, "walkBobSpeed", "Walk Speed", 0.1f, 0.0f, 30.0f, "Bobbing - Walk"),
            META_FIELD_FLOAT(PlayerViewComponent, walkBobAmount, "walkBobAmount", "Walk Amount", 0.001f, 0.0f, 1.0f, "Bobbing - Walk"),

            // Run Bobbing
            META_FIELD_FLOAT(PlayerViewComponent, runBobSpeed, "runBobSpeed", "Run Speed", 0.1f, 0.0f, 40.0f, "Bobbing - Run"),
            META_FIELD_FLOAT(PlayerViewComponent, runBobAmount, "runBobAmount", "Run Amount", 0.001f, 0.0f, 1.0f, "Bobbing - Run"),
        };
        return fields;
    }

    // インスペクター上でのカスタム描画
    static bool CustomGui(PlayerViewComponent& comp, unsigned long long /*entityID*/ = 0, void* /*world*/ = nullptr)
    {
        bool changed = false;

        // --- 1. 実行時デバッグ情報の表示 (読み取り専用に近い形) ---
        ImGui::SeparatorText("Runtime Statistics");
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 1.0f, 1.0f)); // 少し色を変えてデバッグ値だと分かりやすく
        ImGui::Text("Current FOV: %.2f", comp.currentFOV);
        ImGui::Text("Bob Timer: %.2f", comp.bobTimer);
        ImGui::PopStyleColor();

        // --- 2. フィールドの描画 (カテゴリごとに自動描画) ---
        // カテゴリの順序を指定して描画
        const std::vector<std::string> categories = { "Visuals", "Camera Offset", "Bobbing - Idle", "Bobbing - Walk", "Bobbing - Run" };

        for (const auto& cat : categories) {
            ImGui::Spacing();
            ImGui::SeparatorText(cat.c_str());
            for (const auto& fd : Fields()) {
                if (fd.category == cat) {
                    if (ComponentMetaImGui::DrawField(fd, &comp)) changed = true;
                }
            }
        }

        return changed;
    }
};

// コンポーネントの自動登録
REGISTER_COMPONENT(PlayerViewComponent, "FPSPlayerViewComponent")

