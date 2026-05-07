#include "TestAnimationPlayerComponent.h"
#include "Engine/Serialization/ComponentRegistry.h"
#include "Engine/Serialization/Meta/ComponentMeta.h"
#include "Engine/Serialization/Meta/ComponentMetaImGui.h"
#include "Engine/Serialization/Meta/ComponentMetaJson.h"
#include <imgui.h>

// ===================================================================
// TestAnimationPlayerComponent のインスペクタUIおよびシリアライズ定義
// ===================================================================
template <> struct ComponentMeta<TestAnimationPlayerComponent> {
    // コンポーネントをエンジンに登録するかどうか
    static constexpr bool        registered = true;
    // インスペクタ上での表示名
    static constexpr const char* displayName = "Test Animation Player";

    // カスタムGUIを使用するか（今回はシンプルな数値調整のみなので false で自動生成に任せることも可能ですが、
    // 将来的な拡張（テストボタン等）を見据えて true にしておきます）
    static constexpr bool        hasCustomGui = true;
    // シーン保存時にデータをJSONに書き出すかどうか
    static constexpr bool        isSerializable = true;

    // インスペクタでの描画処理
    static bool CustomGui(TestAnimationPlayerComponent& player, unsigned long long entityID, void* worldPtr)
    {
        bool changed = false;

        ImGui::TextDisabled("Player Movement Settings");
        ImGui::Separator();

        // 移動速度の調整スライダー
        if (ImGui::DragFloat("Move Speed", &player.moveSpeed, 0.1f, 0.0f, 100.0f)) {
            changed = true;
        }

        // 振り向き速度の調整スライダー
        if (ImGui::DragFloat("Turn Speed", &player.turnSpeed, 0.1f, 0.0f, 50.0f)) {
            changed = true;
        }

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "[Info] Drives 'Speed' & 'Trigger_Attack' Params");

        return changed;
    }

    // シリアライズ（JSON保存/読込）する変数の定義
    static const std::vector<FieldDescriptor>& Fields()
    {
        static const std::vector<FieldDescriptor> fields = {
            META_FIELD_FLOAT(TestAnimationPlayerComponent, moveSpeed, "moveSpeed", "Move Speed", 0.1f, 0.0f, 100.0f, "Movement"),
            META_FIELD_FLOAT(TestAnimationPlayerComponent, turnSpeed, "turnSpeed", "Turn Speed", 0.1f, 0.0f, 50.0f, "Movement")
        };
        return fields;
    }
};

// エンジンのファクトリにコンポーネントを登録
REGISTER_COMPONENT(TestAnimationPlayerComponent, "TestAnimationPlayer")