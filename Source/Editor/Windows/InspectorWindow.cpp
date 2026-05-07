#include "Editor/Windows/InspectorWindow.h"
#include "Editor/Inspector/ComponentGuiRegistry.h"
#include <algorithm>
#include <cstring>
#include <string>
#include <windows.h>

#include "Engine/Platform/Dialog.h"
#include "Engine/Serialization/PrefabSerializer.h"
#include "Game/Core/AllComponents.h"

InspectorWindow::InspectorWindow() : EditorWindow("Inspector") {}

void InspectorWindow::DrawContents(EditorContext &context)
{
    // ★ 修正：ポインタではなく実体を直接取得
    CCL::ECS::EntityID     selected = context.selectedEntity;
    CCL::ECS::Core::World *world    = context.world;

    // 1. エンティティ未選択時のガード
    // InvalidEntityID または 0 なら描画しない
    if (selected == CCL::ECS::InvalidEntityID || selected == 0) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "Select an Entity to inspect.");
        return;
    }

    // =========================================================
    // Entity メタデータ編集エリア (名前・ID)
    // =========================================================
    auto *nameComp = world->GetComponent<NameComponent>(selected);

    char filenameBuf[256] = "NewPrefab";

    if (nameComp) {
        if (ImGui::InputText("Entity Name", nameComp->name, sizeof(nameComp->name))) {
            context.entityNames[selected] = nameComp->name;
        }
        if (strlen(nameComp->name) > 0) {
            strcpy_s(filenameBuf, sizeof(filenameBuf), nameComp->name);
        }
    }
    else {
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(No NameComponent)");

        ImGui::SameLine();
        if (ImGui::Button("+ Add Name")) {
            std::string defaultName = "Entity_" + std::to_string(selected);
            world->AddComponent<NameComponent>(selected, NameComponent(defaultName.c_str()));
            context.entityNames[selected] = defaultName;
        }

        auto it = context.entityNames.find(selected);
        if (it != context.entityNames.end()) {
            strcpy_s(filenameBuf, sizeof(filenameBuf), it->second.c_str());
        }
    }

    ImGui::SameLine();
    ImGui::TextDisabled("(ID: %llu)", selected);
    ImGui::Separator();

    // 削除ボタン
    ImGui::SameLine(ImGui::GetWindowWidth() - 120);
    if (ImGui::Button("Destroy Entity", ImVec2(110, 0))) {
        world->Destroy(selected);
        // ★ 修正：選択を無効化する
        context.selectedEntity = CCL::ECS::InvalidEntityID;
        return; // 破棄されたのでこれ以上の描画は中止
    }

    // =========================================================
    // Prefab保存ボタン
    // =========================================================
    ImGui::Spacing();
    if (ImGui::Button("Save as Prefab JSON", ImVec2(-1, 0))) {
        char filename[256] = {0};
        if (strlen(filenameBuf) > 0) {
            sprintf_s(filename, "%s.json", filenameBuf);
        }
        else {
            strcpy_s(filename, "NewPrefab.json");
        }

        HWND               hWnd   = GetActiveWindow();
        // 新しい DialogConfig を使って設定を構築する
        DialogConfig cfg;
        cfg.title = "Save Prefab";
        cfg.filter = "JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
        cfg.defaultDir = "Assets/Prefabs";
        cfg.ext = "json";
        cfg.historyKey = "PrefabSave"; // プレハブ保存用の履歴キー

        // 第3引数に cfg を渡す
        if (Dialog::SaveFileName(filename, 256, cfg, hWnd) == DialogResult::OK) {
            PrefabSerializer::Save(filename, world, selected, std::string(filenameBuf));
        }
    }

    ImGui::Separator();

    // =========================================================
    // コンポーネント一覧表示と編集
    // =========================================================
    const auto &archetype = world->GetEntityArchetype(selected);
    auto       &registry  = ComponentGuiRegistry::Instance();

    for (const auto &typeData : archetype) {
        CCL::ECS::TypeID typeID   = typeData.id;

        // 1. まずは Registry にカスタム名（表示名）が登録されているか確認
        std::string compName = registry.GetName(typeID);


        // ★ 診断ログを追加
 /*       char diagBuf[256];
        sprintf_s(diagBuf, "[CCL_DIAG] typeID=%llu, GetName='%s', typeData.name='%s'\n",
            (unsigned long long)typeID,
            compName.c_str(),
            typeData.name ? typeData.name : "(null)");
        OutputDebugStringA(diagBuf);*/


        // 2. 未登録（Unknown）の場合、C++が自動取得した型名を使う
        if (compName.find("Unknown") != std::string::npos &&
            typeData.name != nullptr) {
            compName = typeData.name; // 例: "struct JoltRigidbodyComponent"

            // MSVCの typeid().name() は先頭に "struct " や "class "
            // を付ける仕様があるため、それを削って綺麗にする
            const std::string prefixes[] = {"struct ", "class "};
            for (const auto &prefix : prefixes) {
                if (compName.find(prefix) == 0) {
                  compName.erase(0, prefix.length());
                  break;
                }
            }

          // GUIが未登録（パラメータ編集はできない状態）であることが分かるようにマークを付ける
          compName += " (No GUI)";
        }

        ImGui::PushID((int)typeID);
        bool headerOpen = ImGui::CollapsingHeader(
            compName.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);

        float buttonSize = 20.0f;
        ImGui::SameLine(ImGui::GetWindowWidth() - buttonSize - 20);
        if (ImGui::Button("x", ImVec2(buttonSize, 18))) {
            world->RequestRemoveComponent(selected, typeID);
        }

        if (headerOpen) {
            ImGui::Indent();
            registry.DrawInspector(typeID, world, selected);
            ImGui::Unindent();
            ImGui::Spacing();
        }
        ImGui::PopID();
    }

    ImGui::Separator();
    ImGui::Spacing();

    // --- 新規コンポーネント追加メニュー ---
    if (ImGui::Button("+ Add Component", ImVec2(-1, 30))) {
        ImGui::OpenPopup("AddComponentMenu");
        // メニューを開いた瞬間に検索ワードをリセットする
        _compSearchBuffer[0] = '\0';
    }

    // ポップアップの初期サイズを固定し、見栄えを整える
    ImGui::SetNextWindowSize(ImVec2(280, 400), ImGuiCond_Appearing);
    
    if (ImGui::BeginPopup("AddComponentMenu")) {
        // 1. 自動フォーカス付きの検索バー
        if (ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere(); // ポップアップが開いた瞬間、自動で文字入力状態にする
        }
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##CompSearch", "Search Component...", _compSearchBuffer, sizeof(_compSearchBuffer));
        
        ImGui::Separator();
        ImGui::Spacing();

        // 検索文字列の小文字化（大文字小文字を区別せず検索するため）
        std::string searchStr = _compSearchBuffer;
        std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);

        // 2. スクロール可能なリスト領域の開始
        ImGui::BeginChild("ComponentList", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

        auto allAvailableTypes = registry.GetAllTypeIDs();

        for (auto typeID : allAvailableTypes) {
            std::string compName = registry.GetName(typeID);

            // 検索フィルタリング処理
            if (!searchStr.empty()) {
                std::string compNameLower = compName;
                std::transform(compNameLower.begin(), compNameLower.end(), compNameLower.begin(), ::tolower);
                if (compNameLower.find(searchStr) == std::string::npos) {
                    continue; // 検索に引っかからなかったコンポーネントは非表示
                }
            }

            // 既に持っているコンポーネントはグレーアウトして押せなくする
            bool alreadyHas = world->HasComponent(selected, typeID);
            if (alreadyHas) ImGui::BeginDisabled();

            // コンポーネントの選択ボタン
            if (ImGui::Selectable(compName.c_str())) {
                auto &entry = registry.GetEntry(typeID);
                world->RequestAddComponent(selected,
                    typeID,
                    entry.size,
                    entry.ctor,
                    entry.dtor,
                    entry.constructor,
                    entry.assigner,
                    entry.mover,
                    entry.name.c_str());
                
                // 追加したら自動でポップアップを閉じる（UX向上）
                ImGui::CloseCurrentPopup();
            }

            if (alreadyHas) ImGui::EndDisabled();
        }
        ImGui::EndChild(); // スクロール領域終了
        ImGui::EndPopup();
    }

}