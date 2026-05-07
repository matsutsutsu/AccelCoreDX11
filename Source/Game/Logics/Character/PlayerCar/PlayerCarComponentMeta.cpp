#include "PlayerCarComponent.h"
#include "Editor/Inspector/ComponentGuiRegistry.h"
#include "Engine/Serialization/ComponentRegistry.h"
#include "Engine/Serialization/Meta/ComponentMeta.h"
#include "Engine/Serialization/Meta/ComponentMetaImGui.h"
#include "Engine/Serialization/Meta/ComponentMetaJson.h"
#include <imgui.h>

#include "Game/Logics/Character/Player/PlayerComponent.h"

// ============================================================================
// Player Car Controller
// ============================================================================
template <> struct ComponentMeta<PlayerCarComponent> {
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "Player Car Controller";
    static constexpr bool        hasCustomGui = true; // ★カスタムGUIを有効化

    // 1. 自動シリアライズ対象のフィールド定義
    static const std::vector<FieldDescriptor>& Fields()
    {
        static const std::vector<FieldDescriptor> fields = {
            META_FIELD_FLOAT(PlayerCarComponent, acceleration, "acceleration", "Acceleration", 50.0f, 0.0f, 20000.0f, "Settings"),
            META_FIELD_FLOAT(PlayerCarComponent, turnSpeed, "turnSpeed", "Turn Speed", 1.0f, 0.0f, 1000.0f, "Settings"),
            META_FIELD_FLOAT3(PlayerCarComponent, visualOffset, "visualOffset", "Visual Offset (位置ズレ)", 0.05f, "Visuals"),

            // サスペンションの設定
            META_FIELD_FLOAT(PlayerCarComponent, maxPitchAngle, "maxPitchAngle", "Max Pitch (前後傾き)", 0.1f, 0.0f, 45.0f, "Suspension"),
            META_FIELD_FLOAT(PlayerCarComponent, maxRollAngle, "maxRollAngle", "Max Roll (左右傾き)", 0.1f, 0.0f, 45.0f, "Suspension"),
            META_FIELD_FLOAT(PlayerCarComponent, suspensionSpeed, "suspensionSpeed", "Suspension Spring (バネの硬さ)", 0.1f, 1.0f, 50.0f, "Suspension"),

            // 回転調整用のスライダー（1度ずつ調整可能）
            META_FIELD_FLOAT3(PlayerCarComponent, visualRotation, "visualRotation", "Visual Rotation (回転ズレ)", 1.0f, "Visuals"),

            // デバッグ用に接地状態とレイキャストの長さをインスペクタに表示
            META_FIELD_FLOAT(PlayerCarComponent, raycastLength, "raycastLength", "Raycast Length (レーザーの長さ)", 0.05f, 0.1f, 5.0f, "Physics"),
            META_FIELD_BOOL(PlayerCarComponent, isGrounded, "isGrounded", "Is Grounded (接地状態)", "Physics")
        };
        return fields;
    }

    // 2. カスタムGUIによる「ドラッグ＆ドロップ」の実装
    static bool CustomGui(PlayerCarComponent& comp, unsigned long long entityID = 0, void* world = nullptr)
    {
        bool changed = false;

        // 標準フィールドの描画 (acceleration, turnSpeed)
        for (const auto& fd : Fields()) {
            if (ComponentMetaImGui::DrawField(fd, &comp)) changed = true;
        }

        ImGui::Separator();
        ImGui::Text("Physics Link");

        // --- EntityIDのドラッグ＆ドロップ受け入れ処理 ---
        // 現在リンクされているEntityの名前などを表示（デバッグ用）
        std::string label = (comp.physicsSphereID == 0) ? "(None)" : "Linked Sphere ID: " + std::to_string(comp.physicsSphereID);

        ImGui::Button(label.c_str(), ImVec2(-1, 0));

        // ★ドラッグ＆ドロップの受け入れ（他のEntityをここにドロップできる）
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_ENTITY_ID")) {
                unsigned long long droppedID = *(const unsigned long long*)payload->Data;
                comp.physicsSphereID = droppedID;
                changed = true;
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("ここに物理球体(Sphere)のEntityをドラッグしてください");
        }

        return changed;
    }
};

REGISTER_COMPONENT(PlayerCarComponent, "PlayerCar")



// ============================================================================
// Player  
// ============================================================================
template <> struct ComponentMeta<PlayerComponent> {
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "Player Component";
    static constexpr bool        hasCustomGui = false; // ★カスタムGUIを有効化

    // 1. 自動シリアライズ対象のフィールド定義
    static const std::vector<FieldDescriptor>& Fields()
    {
        static const std::vector<FieldDescriptor> fields = {
            META_FIELD_FLOAT(PlayerComponent, moveSpeed, "moveSpeed", "Move Speed", 10.0f, 0.0f, 20000.0f, "Settings"),
            META_FIELD_FLOAT(PlayerComponent, turnSpeed, "turnSpeed", "Turn Speed", 1.0f, 0.0f, 1000.0f, "Settings")

        };
        return fields;
    }
};

REGISTER_COMPONENT(PlayerComponent, "PlayerComponent")

