#include "Editor/Inspector/ComponentGuiRegistry.h"
#include "Engine/Serialization/ComponentRegistry.h"
#include "Engine/Serialization/Meta/ComponentMeta.h"
#include "Engine/Serialization/Meta/ComponentMetaImGui.h"
#include "Engine/Serialization/Meta/ComponentMetaJson.h"
#include "Engine/GamePlay/Camera/VirtualCameraComponents.h"

#include <algorithm> // std::max用

// ============================================================================
// Virtual Camera Base
// ============================================================================
template <> struct ComponentMeta<VirtualCamera> {
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "Virtual Camera";

    static const std::vector<FieldDescriptor>& Fields()
    {
        static const std::vector<FieldDescriptor> fields = {
            META_FIELD_INT(VirtualCamera, priority, "priority", "Priority", -100, 100, "Camera Settings"),
            META_FIELD_FLOAT(
                VirtualCamera, blendTime, "blendTime", "Blend Time (s)", 0.1f, 0.0f, 10.0f, "Camera Settings"),
            META_FIELD_FLOAT(VirtualCamera, fov, "fov", "Field of View (FOV)", 0.5f, 1.0f, 179.0f, "Lens"),
            META_FIELD_FLOAT(VirtualCamera, nearClip, "nearClip", "Near Clip", 0.01f, 0.001f, 1000.0f, "Lens"),
            META_FIELD_FLOAT(VirtualCamera, farClip, "farClip", "Far Clip", 10.0f, 1.0f, 100000.0f, "Lens") };
        return fields;
    }
};

// ============================================================================
// Camera Body Follow (D&D対応)
// ============================================================================
template <> struct ComponentMeta<CameraBodyFollow> {
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "Camera Body (Follow)";
    static constexpr bool        hasCustomGui = true; // カスタムGUIを有効化

    static bool CustomGui(CameraBodyFollow& comp, unsigned long long /*entityID*/ = 0, void* /*world*/ = nullptr)
    {
        bool changed = false;

        ImGui::SeparatorText("Target Link");

        // Target IDの直接入力
        int targetInt = static_cast<int>(comp.target);
        if (ImGui::InputInt("Target ID", &targetInt)) {
            comp.target = static_cast<unsigned long long>((std::max)(0, targetInt));
            changed = true;
        }

        //  ドラッグ＆ドロップの「受け皿」の魔法
        if (ImGui::BeginDragDropTarget()) {
            // HierarchyWindowから投げられた "HIERARCHY_ENTITY" というタグのデータを受け取る
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
                unsigned long long droppedID = *(const unsigned long long*)payload->Data;
                comp.target = droppedID;
                changed = true;
            }
            ImGui::EndDragDropTarget();
        }

        // ユーザーへの視覚的ヒント
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "[ Drop Entity Here from Hierarchy ]");

        ImGui::SeparatorText("Follow Settings");
        changed |= ImGui::DragFloat3("Follow Offset", &comp.offset.x, 0.1f);
        changed |= ImGui::DragFloat("Damping (遅延)", &comp.damping, 0.1f, 0.0f, 100.0f);
        changed |= ImGui::Checkbox("Lock Y Height", &comp.lockY);

        return changed;
    }

    static const std::vector<FieldDescriptor>& Fields()
    {
        static const std::vector<FieldDescriptor> fields = {
            META_FIELD_INT(CameraBodyFollow, target, "target", "Target Entity ID", 0, 999999, "Follow Settings"),
            META_FIELD_FLOAT3(CameraBodyFollow, offset, "offset", "Follow Offset", 0.1f, "Follow Settings"),
            META_FIELD_FLOAT(CameraBodyFollow, damping, "damping", "Damping (遅延)", 0.1f, 0.0f, 100.0f, "Follow Settings"),
            META_FIELD_BOOL(CameraBodyFollow, lockY, "lockY", "Lock Y Height", "Follow Settings") };
        return fields;
    }
};

// ============================================================================
// Camera Aim LookAt (D&D対応)
// ============================================================================
template <> struct ComponentMeta<CameraAimLookAt> {
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "Camera Aim (LookAt)";
    static constexpr bool        hasCustomGui = true; // カスタムGUIを有効化

    static bool CustomGui(CameraAimLookAt& comp, unsigned long long /*entityID*/ = 0, void* /*world*/ = nullptr)
    {
        bool changed = false;

        ImGui::SeparatorText("Target Link");

        // Target IDの直接入力
        int targetInt = static_cast<int>(comp.target);
        if (ImGui::InputInt("Target ID", &targetInt)) {
            comp.target = static_cast<unsigned long long>((std::max)(0, targetInt));
            changed = true;
        }

        //  ドラッグ＆ドロップの「受け皿」
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
                unsigned long long droppedID = *(const unsigned long long*)payload->Data;
                comp.target = droppedID;
                changed = true;
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "[ Drop Entity Here from Hierarchy ]");

        ImGui::SeparatorText("Aim Settings");
        changed |= ImGui::DragFloat3("LookAt Offset", &comp.offset.x, 0.1f);
        changed |= ImGui::DragFloat("Damping (遅延)", &comp.damping, 0.1f, 0.0f, 100.0f);

        return changed;
    }

    static const std::vector<FieldDescriptor>& Fields()
    {
        static const std::vector<FieldDescriptor> fields = {
            META_FIELD_INT(CameraAimLookAt, target, "target", "Target Entity ID", 0, 999999, "Aim Settings"),
            META_FIELD_FLOAT3(CameraAimLookAt, offset, "offset", "LookAt Offset", 0.1f, "Aim Settings"),
            META_FIELD_FLOAT(CameraAimLookAt, damping, "damping", "Damping (遅延)", 0.1f, 0.0f, 100.0f, "Aim Settings") };
        return fields;
    }
};

// ============================================================================
// Camera Body Free
// ============================================================================
template <> struct ComponentMeta<CameraBodyFree> {
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "Camera Body (Free)";

    static const std::vector<FieldDescriptor>& Fields()
    {
        static const std::vector<FieldDescriptor> fields = {
            META_FIELD_FLOAT(CameraBodyFree, moveSpeed, "moveSpeed", "Move Speed", 0.1f, 0.0f, 100.0f, "Settings"),
            META_FIELD_FLOAT(CameraBodyFree, lookSpeed, "lookSpeed", "Look Speed", 0.01f, 0.0f, 10.0f, "Settings"),
            META_FIELD_FLOAT(CameraBodyFree, currentPitch, "currentPitch", "Current Pitch", 0.1f, -89.0f, 89.0f, "State"),
            META_FIELD_FLOAT(CameraBodyFree, currentYaw, "currentYaw", "Current Yaw", 0.1f, -360.0f, 360.0f, "State")
        };
        return fields;
    }
};

// ============================================================================
// Camera Body TPS
// ============================================================================
template <> struct ComponentMeta<CameraBodyTPS> {
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "Camera Body (TPS)";
    static constexpr bool        hasCustomGui = true;

    static const std::vector<FieldDescriptor>& Fields()
    {
        static const std::vector<FieldDescriptor> fields = {
            META_FIELD_FLOAT(CameraBodyTPS, distance, "distance", "Distance", 0.1f, 1.0f, 100.0f, "TPS Settings"),
            META_FIELD_FLOAT(CameraBodyTPS, currentPitch, "currentPitch", "Pitch", 1.0f, -85.0f, 85.0f, "TPS Settings"),
            META_FIELD_FLOAT(CameraBodyTPS, currentYaw, "currentYaw", "Yaw", 1.0f, -360.0f, 360.0f, "TPS Settings"),
            META_FIELD_FLOAT(CameraBodyTPS, lookSpeedX, "lookSpeedX", "Look Speed X", 1.0f, 0.0f, 500.0f, "TPS Settings"),
            META_FIELD_FLOAT(CameraBodyTPS, lookSpeedY, "lookSpeedY", "Look Speed Y", 1.0f, 0.0f, 500.0f, "TPS Settings"),
            META_FIELD_FLOAT3(CameraBodyTPS, targetOffset, "targetOffset", "Target Offset", 0.1f, "TPS Settings")
        };
        return fields;
    }

    // ドラッグ＆ドロップの実装
    static bool CustomGui(CameraBodyTPS& comp, unsigned long long entityID = 0, void* world = nullptr)
    {
        bool changed = false;
        for (const auto& fd : Fields()) {
            if (ComponentMetaImGui::DrawField(fd, &comp)) changed = true;
        }

        ImGui::Separator();
        ImGui::Text("Follow Target Link");

        std::string label = (comp.targetEntity == 0) ? "(None)" : "Linked Target ID: " + std::to_string(comp.targetEntity);
        ImGui::Button(label.c_str(), ImVec2(-1, 0));

        // プレイヤーEntityなどをドラッグしてここに落とす
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_ENTITY_ID")) {
                comp.targetEntity = *(const unsigned long long*)payload->Data;
                changed = true;
            }
            ImGui::EndDragDropTarget();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("追従するEntityをここにドラッグ");

        return changed;
    }
};



// ============================================================================
// 究極の自動化：これだけでJSONとImGuiの両方に登録される
// ============================================================================
REGISTER_COMPONENT(VirtualCamera, "VirtualCamera")
REGISTER_COMPONENT(CameraBodyFollow, "CameraBodyFollow")
REGISTER_COMPONENT(CameraAimLookAt, "CameraAimLookAt")
REGISTER_COMPONENT(CameraBodyFree, "CameraBodyFree")
REGISTER_COMPONENT(CameraBodyTPS, "CameraBodyTPS")