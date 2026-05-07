#include "Editor/Inspector/ComponentGuiRegistry.h"
#include "Engine/Serialization/ComponentRegistry.h"
#include "Engine/Serialization/Meta/ComponentMeta.h"
#include "Engine/Serialization/Meta/ComponentMetaImGui.h"
#include "Engine/Serialization/Meta/ComponentMetaJson.h"
#include "Engine/GamePlay/Core/NameComponent.h"

// ============================================================================
// ★追加: NameComponent 専用の JSON シリアライズ処理（テンプレートの明示的特化）
// 汎用の Fields() リフレクションをバイパスし、char配列を安全に保存・復元します。
// ============================================================================
namespace ComponentMetaJson {
    // 保存（Save）時の処理
    template <>
    inline void Serialize<NameComponent>(json& j, const NameComponent& comp)
    {
        // char[32] を std::string に変換して JSON に書き込む
        j["name"] = std::string(comp.name);
    }

    // 読込（Load）時の処理
    template <>
    inline void Deserialize<NameComponent>(const json& j, NameComponent& comp)
    {
        // JSON に "name" キーがあれば読み込んで、char配列に安全にコピー（バッファオーバーラン防止）
        if (j.contains("name") && j["name"].is_string()) {
            std::string str = j["name"].get<std::string>();
            strncpy_s(comp.name, str.c_str(), sizeof(comp.name) - 1);
            comp.name[sizeof(comp.name) - 1] = '\0'; // 念のための終端保証
        }
    }
}

// ============================================================================
// Component Meta の定義
// ============================================================================
template <> struct ComponentMeta<NameComponent> {
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "Name";

    // char配列を安全に編集するため完全カスタムGUI
    static constexpr bool hasCustomGui = true;

    static bool CustomGui(NameComponent& comp, unsigned long long /*entityID*/ = 0, void* /*world*/ = nullptr)
    {
        bool changed = false;

        ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "Entity Name");

        // char[32] のバッファを直接ImGuiに渡して編集させる
        if (ImGui::InputText("##NameInput", comp.name, sizeof(comp.name))) {
            changed = true;
        }
        return changed;
    }

    // JSON変換は上の特化テンプレートで直接処理するため、ここは空でOK
    static const std::vector<FieldDescriptor>& Fields()
    {
        static const std::vector<FieldDescriptor> empty;
        return empty;
    }
};

// ============================================================================
// 究極の自動化：これだけでJSONとImGuiの両方に登録される
// ============================================================================
REGISTER_COMPONENT(NameComponent, "Name")