#include "Editor/Inspector/ComponentGuiRegistry.h"
#include "Engine/Serialization/ComponentRegistry.h"
#include "Engine/Serialization/Meta/ComponentMeta.h"
#include "Engine/Serialization/Meta/ComponentMetaImGui.h"
#include "Engine/Serialization/Meta/ComponentMetaJson.h"
#include "TimeState.h"


// ============================================================================
// TimeState 専用の JSON シリアライズ処理
// ============================================================================
namespace ComponentMetaJson {
    template <>
    inline void Serialize<TimeState>(json& j, const TimeState& comp)
    {
        j["localDt"] = comp.localDt;
        j["hitStopTimer"] = comp.hitStopTimer;
        j["freezeScale"] = comp.freezeScale;
        j["group"] = static_cast<uint8_t>(comp.group);
    }

    template <>
    inline void Deserialize<TimeState>(const json& j, TimeState& comp)
    {
        if (j.contains("localDt"))      comp.localDt = j["localDt"].get<float>();
        if (j.contains("hitStopTimer")) comp.hitStopTimer = j["hitStopTimer"].get<float>();
        if (j.contains("freezeScale"))  comp.freezeScale = j["freezeScale"].get<float>();
        if (j.contains("group"))        comp.group = static_cast<TimeGroup>(j["group"].get<uint8_t>());
    }
}

// ============================================================================
// TimeState の Inspector GUI 定義
// ============================================================================
template <> struct ComponentMeta<TimeState> {
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "Time State";

    // enum のプルダウン等を使うためカスタムGUIを有効化
    static constexpr bool hasCustomGui = true;

    static bool CustomGui(TimeState& comp, unsigned long long /*entityID*/ = 0, void* /*world*/ = nullptr)
    {
        bool changed = false;

        // 計算済みの実効dt（リードオンリー表示）
        ImGui::Text("Local Dt: %.4f", comp.localDt);
        ImGui::Spacing();

        // TimeGroup のプルダウン選択
        const char* groupNames[] = { "None", "Player", "Enemy", "Environment", "UI" };
        int currentGroup = static_cast<int>(comp.group);
        if (ImGui::Combo("Time Group", &currentGroup, groupNames, IM_ARRAYSIZE(groupNames))) {
            comp.group = static_cast<TimeGroup>(currentGroup);
            changed = true;
        }

        // デバッグ・テスト用のヒットストップ手動設定
        if (ImGui::DragFloat("HitStop Timer", &comp.hitStopTimer, 0.01f, 0.0f, 5.0f)) changed = true;
        if (ImGui::SliderFloat("Freeze Scale", &comp.freezeScale, 0.0f, 1.0f)) changed = true;

        return changed;
    }

    static const std::vector<FieldDescriptor>& Fields()
    {
        static const std::vector<FieldDescriptor> empty_fields;
        return empty_fields; // カスタムGUIとJSONシリアライズを全自作したためリフレクションは不要
    }
};

// コンポーネント登録マクロ（システムが起動時に認識するために必須）
REGISTER_COMPONENT(TimeState,"Time State");