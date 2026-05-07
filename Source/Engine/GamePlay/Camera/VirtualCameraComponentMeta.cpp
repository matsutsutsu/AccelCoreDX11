#include "Editor/Inspector/ComponentGuiRegistry.h"
#include "Engine/Serialization/ComponentRegistry.h"
#include "Engine/Serialization/Meta/ComponentMeta.h"
#include "Engine/Serialization/Meta/ComponentMetaImGui.h"
#include "Engine/Serialization/Meta/ComponentMetaJson.h"
#include "Engine/GamePlay/Camera/VirtualCameraComponents.h"

#include "Engine/GamePlay/Transform/TransformComponent.h" // これが必要
#include "Engine/GamePlay/Core/NameComponent.h" // NameComponentの定義ヘッダー
#include "ECS/Core/CCL_World.h" // Worldの定義

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
            META_FIELD_BOOL(CameraBodyFollow, lockY, "lockY", "Lock Y Height", "Follow Settings"),
            META_FIELD_ENTITY_ID(CameraBodyFollow, target, "target", "Target Entity ID", "Follow Settings")
    };
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
template <> struct ComponentMeta<CameraBodyTPS> 
{
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
            META_FIELD_FLOAT3(CameraBodyTPS, targetOffset, "targetOffset", "Target Offset", 0.1f, "TPS Settings"),
            META_FIELD_ENTITY_ID(CameraBodyTPS, targetEntity, "targetEntity", "Target Entity ID", "TPS Settings")
        };
        return fields;
    }

    // ドラッグ＆ドロップの実装
    static bool CustomGui(CameraBodyTPS& comp, unsigned long long entityID = 0, void* world = nullptr)
    {
        CCL::ECS::Core::World* worldData = static_cast<CCL::ECS::Core::World*>(world);

        bool changed = false;
        for (const auto& fd : Fields()) {
            if (ComponentMetaImGui::DrawField(fd, &comp)) changed = true;
        }

        ImGui::Separator();
        ImGui::Text("Follow Target Link");

        std::string label = (comp.targetEntity == 0) ? "(None)" : "Linked Target ID: " + std::to_string(comp.targetEntity);
        ImGui::Button(label.c_str(), ImVec2(-1, 0));

        static bool s_wasOpened = false;
        //一旦ワールドから検索
        if (worldData)
        {
            // 1. 現在のターゲット名表示（これは1個だけ取得なので毎フレームでOK）
            std::string previewName = "(None)";
            if (comp.targetEntity != 0) {
                auto* nameComp = worldData->GetComponent<NameComponent>(comp.targetEntity);
                previewName = nameComp ? nameComp->name : "ID: " + std::to_string(comp.targetEntity);
            }

            // アイテムがクリックされた直後のフレーム、またはキャッシュが空の時だけ検索を実行
             // ※ImGui::IsItemClicked() は BeginCombo の直前で判定する必要があるため、
             //   もっとも確実なのは「初めて開いたフラグ」の管理です。
            static bool s_wasOpened = false;

            // 2. コンボボックスの開始
            // ここで static 変数を使って「開いた瞬間」を検知する
            static std::vector<std::pair<unsigned long long, std::string>> s_entityCache;

            if (ImGui::BeginCombo("Target Entity", previewName.c_str()))
            {

                if (!s_wasOpened)
                {
                    s_entityCache.clear();
                    // ここで重いサーチを実行
                    auto entities = worldData->View<TransformComponent>();
                    for (auto id : entities) {
                        if (id == entityID) continue;
                        auto* nameComp = worldData->GetComponent<NameComponent>(id);
                        std::string n = nameComp ? nameComp->name : "ID: " + std::to_string(id);
                        s_entityCache.push_back({ id, n });
                    }
                    s_wasOpened = true;
                }

                // --- 以降はキャッシュを使って描画するだけなので高速 ---
                if (ImGui::Selectable("(None)", comp.targetEntity == 0)) {
                    comp.targetEntity = 0;
                    changed = true;
                }

                for (const auto& item : s_entityCache) {
                    bool isSelected = (comp.targetEntity == item.first);
                    // IDを隠し名(##)にして表示
                    std::string label = item.second + " ##" + std::to_string(item.first);

                    if (ImGui::Selectable(label.c_str(), isSelected)) {
                        comp.targetEntity = item.first;
                        changed = true;
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            }
            else
            {
                // コンボボックスが閉じている時はフラグを下ろす
                s_wasOpened = false;
            }
        }

        //// プレイヤーEntityなどをドラッグしてここに落とす
        //if (ImGui::BeginDragDropTarget()) {
        //    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_ENTITY_ID")) {
        //        comp.targetEntity = *(const unsigned long long*)payload->Data;
        //        changed = true;
        //    }
        //    ImGui::EndDragDropTarget();
        //}
        //if (ImGui::IsItemHovered()) ImGui::SetTooltip("追従するEntityをここにドラッグ");

        return changed;
    }
};

//4/15追加　桃田
// ============================================================================
// Camera Body FPS
// ============================================================================
template <> struct ComponentMeta<CameraBodyFPS> 
{
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "Camera Body (FPS)";
    static constexpr bool        hasCustomGui = true;

    static const std::vector<FieldDescriptor>& Fields()
    {
        static const std::vector<FieldDescriptor> fields = {
            META_FIELD_FLOAT(CameraBodyFPS, currentPitch, "currentPitch", "Pitch", 1.0f, -89.0f, 89.0f, "FPS Settings"),
            META_FIELD_FLOAT(CameraBodyFPS, currentYaw, "currentYaw", "Yaw", 1.0f, -360.0f, 360.0f, "FPS Settings"),
            META_FIELD_FLOAT(CameraBodyFPS, mouseSensitivity, "mouseSensitivity", "Mouse Sensitivity", 0.01f, 0.0f, 10.0f, "FPS Settings"),
            META_FIELD_FLOAT3(CameraBodyFPS, eyeOffset, "eyeOffset", "Eye Offset", 0.05f, "FPS Settings"),
            META_FIELD_ENTITY_ID(CameraBodyFPS, targetEntity, "targetEntity", "Target Entity ID", "FPS Settings")
        };
        return fields;
    }

    // ドラッグ＆ドロップの実装
    static bool CustomGui(CameraBodyFPS& comp, unsigned long long entityID = 0, void* world = nullptr)
    {
        CCL::ECS::Core::World* worldData = static_cast<CCL::ECS::Core::World*>(world);
        bool changed = false;

        // --- 他のフィールド描画 ---
        for (const auto& fd : Fields()) {
            if (ComponentMetaImGui::DrawField(fd, &comp)) changed = true;
        }

        ImGui::Separator();
        ImGui::Text("Follow Target Link");
        //一旦ワールドから検索
        if (worldData)
        {
            // 1. 現在のターゲット名表示（これは1個だけ取得なので毎フレームでOK）
            std::string previewName = "(None)";
            if (comp.targetEntity != 0) {
                auto* nameComp = worldData->GetComponent<NameComponent>(comp.targetEntity);
                previewName = nameComp ? nameComp->name : "ID: " + std::to_string(comp.targetEntity);
            }

            // アイテムがクリックされた直後のフレーム、またはキャッシュが空の時だけ検索を実行
             // ※ImGui::IsItemClicked() は BeginCombo の直前で判定する必要があるため、
             //   もっとも確実なのは「初めて開いたフラグ」の管理です。
            static bool s_wasOpened = false;

            // 2. コンボボックスの開始
            // ここで static 変数を使って「開いた瞬間」を検知する
            static std::vector<std::pair<unsigned long long, std::string>> s_entityCache;

            if (ImGui::BeginCombo("Target Entity", previewName.c_str()))
            {

                if (!s_wasOpened)
                {
                    s_entityCache.clear();
                    // ここで重いサーチを実行
                    auto entities = worldData->View<TransformComponent>();
                    for (auto id : entities) {
                        if (id == entityID) continue;
                        auto* nameComp = worldData->GetComponent<NameComponent>(id);
                        std::string n = nameComp ? nameComp->name : "ID: " + std::to_string(id);
                        s_entityCache.push_back({ id, n });
                    }
                    s_wasOpened = true;
                }

                // --- 以降はキャッシュを使って描画するだけなので高速 ---
                if (ImGui::Selectable("(None)", comp.targetEntity == 0)) {
                    comp.targetEntity = 0;
                    changed = true;
                }

                for (const auto& item : s_entityCache) {
                    bool isSelected = (comp.targetEntity == item.first);
                    // IDを隠し名(##)にして表示
                    std::string label = item.second + " ##" + std::to_string(item.first);

                    if (ImGui::Selectable(label.c_str(), isSelected)) {
                        comp.targetEntity = item.first;
                        changed = true;
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            }
            else
            {
                // コンボボックスが閉じている時はフラグを下ろす
                s_wasOpened = false;
            }
        }

        return changed;

        }
};

template <> struct ComponentMeta<CameraLockOn> {
    static constexpr bool        registered = true;
    static constexpr const char* displayName = "Camera Lock On";
    static constexpr bool        hasCustomGui = true;

    static bool CustomGui(CameraLockOn& comp, unsigned long long entityID = 0, void* world = nullptr) {
        bool changed = false;

        // --- 基本設定 ---
        ImGui::TextDisabled("Base Settings");
        changed |= ImGui::DragFloat("Rotation Damping", &comp.rotationDamping, 0.1f, 0.0f, 100.0f);
        changed |= ImGui::DragFloat3("Target Offset", &comp.targetOffset.x, 0.01f);

        // --- レイアウト・画面収まり設定 ---
        ImGui::Separator();
        ImGui::TextDisabled("Layout & Framing");
        // focusWeight: 0.35辺りにすると自キャラが画面中央から外れにくくなります
        changed |= ImGui::SliderFloat("Focus Weight", &comp.focusWeight, 0.0f, 1.0f, "%.2f");
        // sideOffset: 肩越し視点の強さ
        changed |= ImGui::DragFloat("Side Offset", &comp.sideOffset, 0.01f, -5.0f, 5.0f);

        // --- 距離とFOVの連動設定 ---
        ImGui::Separator();
        ImGui::TextDisabled("Distance & FOV");
        changed |= ImGui::DragFloatRange2("Distance Min/Max", &comp.minDistance, &comp.maxDistance, 0.1f, 0.0f, 100.0f);
        changed |= ImGui::DragFloat("Min FOV", &comp.minFov, 0.1f, 10.0f, 120.0f);
        changed |= ImGui::DragFloat("Max FOV (Approach)", &comp.maxFov, 0.1f, 10.0f, 120.0f);

        CCL::ECS::Core::World* worldData = static_cast<CCL::ECS::Core::World*>(world);

         if (worldData) 
         {
            // 1. 現在の選択名を表示
            std::string previewName = "(None)";
            if (comp.targetEntity != 0) {
                auto* nameComp = worldData->GetComponent<NameComponent>(comp.targetEntity);
                previewName = nameComp ? nameComp->name : "ID: " + std::to_string(comp.targetEntity);
            }

            // 2. コンボボックスの制御（キャッシュ方式）
            static bool s_wasOpened = false;
            static std::vector<std::pair<unsigned long long, std::string>> s_entityCache;

            if (ImGui::BeginCombo("Enemy Target", previewName.c_str())) {
                if (!s_wasOpened) {
                    s_entityCache.clear();
                    // 全てのエンティティ（Transformを持つもの）を走査
                    auto entities = worldData->View<TransformComponent>();
                    for (auto id : entities) {
                        if (id == entityID) continue; // 自分自身は除外

                        auto* nameComp = worldData->GetComponent<NameComponent>(id);
                        std::string n = nameComp ? nameComp->name : "ID: " + std::to_string(id);
                        s_entityCache.push_back({ id, n });
                    }
                    s_wasOpened = true;
                }

                if (ImGui::Selectable("(None)", comp.targetEntity == 0)) {
                    comp.targetEntity = 0;
                    changed = true;
                }

                for (const auto& item : s_entityCache) {
                    bool isSelected = (comp.targetEntity == item.first);
                    std::string label = item.second + " ##" + std::to_string(item.first);

                    if (ImGui::Selectable(label.c_str(), isSelected)) {
                        comp.targetEntity = item.first;
                        changed = true;
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            else {
                s_wasOpened = false;
            }
        }
        return changed;
    }

    static const std::vector<FieldDescriptor>& Fields() {
        static const std::vector<FieldDescriptor> fields = {
            META_FIELD_ENTITY_ID(CameraLockOn, targetEntity, "targetEntity", "Target Entity ID", "Settings"),
        META_FIELD_ENTITY_ID(CameraLockOn, lastTargetEntity, "lastTargetEntity", "Last Target Entity ID", "Settings"),
        META_FIELD_FLOAT(CameraLockOn, rotationDamping, "rotationDamping", "Damping", 0.1f, 0.0f, 100.0f, "Settings"),
         META_FIELD_FLOAT(CameraLockOn, rotationDamping, "rotationDamping", "Damping", 0.1f, 0.0f, 100.0f, "Settings"),
            META_FIELD_FLOAT(CameraLockOn, minDistance, "minDistance", "Min Distance", 0.1f, 0.0f, 100.0f, "Settings"),
            META_FIELD_FLOAT(CameraLockOn, maxDistance, "maxDistance", "Max Distance", 0.1f, 0.0f, 100.0f, "Settings"),
            META_FIELD_FLOAT(CameraLockOn, sideOffset, "sideOffset", "Side Offset", 0.01f, -5.0f, 5.0f, "Layout"),
            META_FIELD_FLOAT(CameraLockOn, focusWeight, "focusWeight", "Focus Weight", 0.01f, 0.0f, 1.0f, "Layout"),
            META_FIELD_FLOAT(CameraLockOn, minFov, "minFov", "Min FOV", 0.1f, 1.0f, 170.0f, "FOV"),
            META_FIELD_FLOAT(CameraLockOn, maxFov, "maxFov", "Max FOV", 0.1f, 1.0f, 170.0f, "FOV"),
        };
        return fields;
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
//追加　桃田
REGISTER_COMPONENT(CameraBodyFPS, "CameraBodyFPS")
REGISTER_COMPONENT(CameraLockOn, "CameraLockOn")