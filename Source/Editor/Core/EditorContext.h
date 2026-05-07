#pragma once
#include "ECS/Core/CCL_World.h"
#include "ECS/System/CCL_SystemManager.h"
#include "Editor/Inspector/ComponentGuiRegistry.h"
#include <map>
#include <string>
#include <vector>
#include <ImGuizmo.h>

// エディタ内で共有されるコンテキストデータ
struct EditorContext {
    CCL::ECS::Core::World *world          = nullptr;
    CCL::ECS::SystemManager *systemManager  = nullptr;
    CCL::ECS::EntityID       selectedEntity = CCL::ECS::InvalidEntityID;

    // Prefabリスト (AssetBrowserなどが使う)
    std::vector<std::string> prefabFiles;

    // Hierarchy表示用の名前キャッシュ
    std::map<CCL::ECS::EntityID, std::string> entityNames;

    // シーンロード予約パス
    std::string pendingLoadScenePath;

    // シーンセーブ予約パス
    std::string pendingSaveScenePath;

    // アニメーションエディタとの連携用
     bool  isAnimEditMode = false; // これらもポインタではなく実体値にするのが安全です
     float animEditTime   = 0.0f;

     // ★ ImGuizmo用の状態管理
     ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::TRANSLATE;
     ImGuizmo::MODE      currentGizmoMode = ImGuizmo::LOCAL;
};