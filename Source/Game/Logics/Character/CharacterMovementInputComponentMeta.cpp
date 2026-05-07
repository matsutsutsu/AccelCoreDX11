#include "Game/Logics/Character/CharacterMovementInputComponent.h"
#include "Editor/Inspector/ComponentGuiRegistry.h"
#include "Engine/Serialization/ComponentRegistry.h"
#include "Engine/Serialization/Meta/ComponentMeta.h"
#include "Engine/Serialization/Meta/ComponentMetaImGui.h"
#include "Engine/Serialization/Meta/ComponentMetaJson.h"

#include <imgui.h>

// ===================================================================================
// 【 キャラクター入力モニター : CharacterMovementInputComponentMeta 】
//
// [ 役割 ]
// 毎フレーム上書きされる「入力（アクセルペダル）」の値をエディタ上で可視化する。
//
// [ アーキテクトからの注意点 ]
// 物理挙動（Jolt）が期待通りに動かない場合、ここに正しい速度ベクトルが
// 流れてきているかを確認するための「監視モニター」として機能します。
// 毎フレーム上書きされるため、ここで数値を手動で変更しても意味がありません（すぐに上書きされます）。
// そのため、ImGuiの入力欄は ReadOnly（読み取り専用）に設定しています。
// ===================================================================================

template <> struct ComponentMeta<CharacterMovementInputComponent> {
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "Character Movement Input";

    // リアルタイムに入力値をデバッグ表示するためにCustomGuiを有効化
    static constexpr bool        hasCustomGui = true;

    static bool CustomGui(CharacterMovementInputComponent& comp, unsigned long long entityID = 0, void* worldPtr = nullptr) {
        bool changed = false;

        ImGui::SeparatorText("Live Input Monitor (Read Only)");

        // SimpleMath::Vector3 を float 配列として ImGui に渡す
        float vel[3] = { comp.desiredVelocity.x, comp.desiredVelocity.y, comp.desiredVelocity.z };

        // ImGuiInputTextFlags_ReadOnly を付けることで、エディタ上からの手動書き換えを禁止する
        ImGui::InputFloat3("Desired Velocity", vel, "%.2f", ImGuiInputTextFlags_ReadOnly);

        float look[3] = { comp.desiredLookDir.x, comp.desiredLookDir.y, comp.desiredLookDir.z };
        ImGui::InputFloat3("Desired Look Dir", look, "%.2f", ImGuiInputTextFlags_ReadOnly);

        // チェックボックスも視覚化用としてDisabled（操作不可）状態で表示する
        ImGui::BeginDisabled();
        ImGui::Checkbox("Jump Requested", &comp.jumpRequested);
        ImGui::EndDisabled();

		ImGui::InputFloat("Custom Jump Velocity", &comp.customJumpVelocity, 0.1f, 1.0f, "%.2f", ImGuiInputTextFlags_ReadOnly);
		ImGui::InputFloat("Custom Gravity", &comp.customGravity, 0.1f, 1.0f, "%.2f", ImGuiInputTextFlags_ReadOnly);

        return changed; // ReadOnlyなので常にfalseを返す
    }

    static const std::vector<FieldDescriptor>& Fields() {
        static const std::vector<FieldDescriptor> fields = {
            // 注意: このコンポーネントは「そのフレームの入力状態」なので、
            // 本来はファイル（JSON）にセーブ/ロードする必要はありません。
            // しかし、シリアライザのシステム要件を満たすために空にしておくか、
            // エンジンの仕様に合わせてダミー登録しておきます。
        };
        return fields;
    }
};

REGISTER_COMPONENT(CharacterMovementInputComponent, "CharacterMovementInput")