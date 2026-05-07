#include "Engine/Audio/AudioEmitterComponent.h"
#include "Editor/Inspector/ComponentGuiRegistry.h"
#include "Engine/Serialization/ComponentRegistry.h"
#include "Engine/Serialization/Meta/ComponentMeta.h"
#include "Engine/Serialization/Meta/ComponentMetaImGui.h"
#include "Engine/Serialization/Meta/ComponentMetaJson.h"
#include "Engine/Core/Math/StringHash.h" // ※プロジェクトのHashString関数
#include <imgui.h>

// =======================================================================
// メタデータ定義 (インスペクタ/シリアライズ用)
// =======================================================================
template <> struct ComponentMeta<AudioEmitterComponent> {
    static constexpr bool registered = true;
    static constexpr const char* displayName = "Audio Emitter";

    // ★最重要: デフォルトの描画を捨て、CustomGui() の呼び出しをエンジンに要求する
    static constexpr bool hasCustomGui = true;

    static constexpr bool isSerializable = true;

    // シリアライズ（JSON保存）の対象は設定値のみ
    static const std::vector<FieldDescriptor>& Fields() {
        static const std::vector<FieldDescriptor> fields = {
            { "Event Hash", "eventHash", FieldKind::Int, offsetof(AudioEmitterComponent, eventHash), 1.0f, 0.0f, 0.0f, "Audio", nullptr, 0, true },
            { "Auto Play", "autoPlay", FieldKind::Bool, offsetof(AudioEmitterComponent, autoPlay), 1.0f, 0.0f, 0.0f, "Audio", nullptr, 0, true }
        };
        return fields;
    }

    // =======================================================================
    // インスペクタ用 カスタムGUI描画処理
    // =======================================================================
    static bool CustomGui(AudioEmitterComponent& comp, unsigned long long entityID, void* world) {
        bool changed = false;

        ImGui::PushID("AudioEmitterComponent_UI");

        // 1. 現在のハッシュ値をデバッグ表示（読み取り専用）
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Hash ID: 0x%08X", comp.eventHash);

        // 2. 文字列からハッシュを生成するためのテキスト入力フィールド
        static char pathBuffer[256] = "";

        // UX向上: テキスト入力フィールド
        ImGui::Text("FMOD Event Path");
        if (ImGui::InputText("##FMODPath", pathBuffer, sizeof(pathBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
            // エンターキーが押された瞬間、文字列をハッシュ化してPODな数値として保存する
            if (strlen(pathBuffer) > 0) {
                comp.eventHash = CCL::Utils::HashString(pathBuffer);
                changed = true;

                // 次の入力のためにバッファをクリアしておく（好みに応じて残しても良い）
                pathBuffer[0] = '\0';
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Example: event:/SFX/Shoot\nType the FMOD path and press [Enter] to apply.");
        }

        ImGui::Spacing();

        // 3. その他のプロパティ (AutoPlay等) の描画
        if (ImGui::Checkbox("Auto Play on Spawn", &comp.autoPlay)) {
            changed = true;
        }

        // デバッグ用: 現在の再生状態（シリアライズはされないが、エディタで確認できると便利）
        ImGui::BeginDisabled();
        ImGui::Checkbox("Is Playing (Runtime)", &comp.isPlaying);
        ImGui::EndDisabled();

        ImGui::PopID();

        return changed;
    }
};

// マクロによるメタデータの自動登録
REGISTER_COMPONENT(AudioEmitterComponent, "AudioEmitterComponent")