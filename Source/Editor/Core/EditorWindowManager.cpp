#include "EditorWindowManager.h"

#include "Engine/Serialization/SceneSerializer.h"
#include "Engine/GamePlay/Core/Scene/SceneManager.h"
#include "Engine/Platform/Dialog.h"

#include "Editor/Core/EditorCommandHistory.h"

#include "Engine/GamePlay/Transform/TransformComponent.h"

// 各ウィンドウのヘッダをインクルード
#include "Editor/Windows/AssetBrowserWindow.h"
#include "Editor/Windows/ECSDebugWindow.h"
#include "Editor/Windows/GameViewWindow.h"
#include "Editor/Windows/HierarchyWindow.h"
#include "Editor/Windows/InspectorWindow.h"
#include "Editor/Windows/AnimationSequencerWindow.h"
#include "Editor/Windows/TextEditorWindow.h"
#include "Editor/Windows/InputEditorWindow.h"
#include "Editor/Windows/ConsoleWindow.h"
#include "Editor/Windows/AnimGraphWindow.h"
#include "Editor/Windows/UIEditorWindow.h"
#include "Editor/Windows/BehaviorTreeWindow.h"

#include "Engine/Serialization/Factory/Prefab.h"
#include <imgui.h>
#include <filesystem>

namespace fs = std::filesystem;

void EditorWindowManager::Initialize()
{
    // =========================================================
    // ★ 常に表示しておきたいメインウィンドウ (デフォルトで true)
    // =========================================================
    RegisterWindow<InspectorWindow>();
    RegisterWindow<HierarchyWindow>();
    RegisterWindow<AssetBrowserWindow>();

    // =========================================================
    // ★ デフォルトでは「非表示」にするサブウィンドウ
    // (->SetVisible(false) をつけるだけで、起動時は隠れます)
    // =========================================================
    RegisterWindow<ECSDebugWindow>();
    RegisterWindow<ConsoleWindow>();
    RegisterWindow<AnimationSequencerWindow>();
    RegisterWindow<AnimGraphWindow>();
    //RegisterWindow<UIEditorWindow>();
    RegisterWindow<BehaviorTreeWindow>();

    // ※まだ実装中で完全に読み込みたくない（クラッシュする等の）場合は、
    // 引き続きコメントアウトで対応してください。
    RegisterWindow<TextEditorWindow>()->SetVisible(false);
    RegisterWindow<InputEditorWindow>()->SetVisible(false);

    //RegisterWindow<SceneViewWindow>();
    //RegisterWindow<GameViewWindow>();
}


// プレハブフォルダをスキャンしてリストを更新する関数
void RefreshPrefabList(EditorContext &context)
{
    context.prefabFiles.clear();

    // スキャン対象のディレクトリ (必要に応じて変更してください)
    std::string targetPath = "Assets/Prefabs";

    if (fs::exists(targetPath)) {
        try {
            // 再帰的にディレクトリを探索
            for (const auto &entry : fs::recursive_directory_iterator(targetPath)) {
                // .json ファイルのみをリストに追加
                if (entry.is_regular_file() && entry.path().extension() == ".json") {
                    // パスを汎用形式 (スラッシュ区切り) に変換して保存
                    context.prefabFiles.push_back(entry.path().generic_string());
                }
            }
        }
        catch (const fs::filesystem_error &e) {
            OutputDebugStringA(e.what());
        }
    }
}


// Draw関数の実装
void EditorWindowManager::Draw(EditorContext &context)
{
    // EditorScene::OnDrawImGui もしくは Update の中で
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
        EditorCommandHistory::Instance().Undo();
    }
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
        EditorCommandHistory::Instance().Redo();
    }

    // 1. ショートカットキー (F1) で表示切替
    // ImGuiがアクティブでなくても効くようにします
    if (ImGui::IsKeyPressed(ImGuiKey_F1, false)) {
        _isVisible = !_isVisible;
    }

    // 初回のみリストを自動生成
    if (context.prefabFiles.empty()) {
        RefreshPrefabList(context);
    }

    // UI非表示モードなら、ここで処理を終了（=ウィンドウを描画しない）
    // これによりバックバッファに描画されているゲーム画面だけが見える状態になります
    if (!_isVisible) {
        // ヒントとして左上に小さく戻し方を表示しておくと親切です（不要なら削除可）
        ImGui::SetNextWindowPos(ImVec2(10, 30));
        ImGui::SetNextWindowBgAlpha(0.3f); // 半透明
        ImGui::Begin("HiddenOverlay",
            nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                ImGuiWindowFlags_NoInputs);
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "Press F1 to Show UI");
        ImGui::End();
        return;
    }

    // 3. 表示状態なら描画実行
    DrawMainMenuBar(context);

    for (auto &window : _windows) {
        window->Render(context);
    }
}



void EditorWindowManager::DrawMainMenuBar(EditorContext &context)
{
    auto &world = context.world;

    if (ImGui::BeginMainMenuBar()) {
        // --- File Menu ---
        if (ImGui::BeginMenu("File")) {

            if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) {
                char               filename[256] = {0};
                HWND               hWnd          = GetActiveWindow();
                static const char *filter = "Scene Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";

                if (Dialog::OpenFileName(filename, 256, filter, "Open Scene",
                    "Assets/Scene", hWnd) ==
                    DialogResult::OK) {

                    //  即時ロードせず、パスを保存して予約する
                    context.pendingLoadScenePath = std::string(filename);

                    // ★ アーキテクトの極意: 安全装置
                    // 前のシーンのエンティティはすべて破壊されたため、
                    // Inspectorが古いIDを参照してクラッシュするのを防ぐために選択を解除する
                    context.selectedEntity = CCL::ECS::InvalidEntityID;
                }
            }

            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
                char               filename[256] = {};
                HWND               hWnd          = GetActiveWindow();
                static const char *filter = "Scene Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";

                if (Dialog::SaveFileName(filename, 256, filter, "Save Scene", "json",
                    "Assets/Scene", hWnd) ==
                    DialogResult::OK) {

                    //  即時ロードせず、パスを保存して予約する
                    context.pendingSaveScenePath = std::string(filename);

                    // ★ アーキテクトの極意: 安全装置
                    // 前のシーンのエンティティはすべて破壊されたため、
                    // Inspectorが古いIDを参照してクラッシュするのを防ぐために選択を解除する
                    context.selectedEntity = CCL::ECS::InvalidEntityID;
                }
            }

            if (ImGui::MenuItem("Exit")) {
                PostQuitMessage(0);
            }
            ImGui::EndMenu();
        }

        // --- Entity Menu ---
        if (ImGui::BeginMenu("Entity")) {
            if (ImGui::MenuItem("Add Empty Entity")) {
                Archetype arch = ArchetypeHelper::Generate<TransformComponent>();
                world->RequestSpawnEntity(arch);
            }
            ImGui::EndMenu();
        }


        // --- Prefab Menu ---
        if (ImGui::BeginMenu("Prefab")) {

            // リスト更新ボタン
            if (ImGui::MenuItem("Refresh List")) {
                RefreshPrefabList(context);
            }
            ImGui::Separator();

            if (context.prefabFiles.empty()) {
                ImGui::TextDisabled("No prefabs found");
            }
            else {
                // ListBoxを廃止し、MenuItemの連続に変更する
                for (size_t i = 0; i < context.prefabFiles.size(); ++i) {
                    // フルパスだと長すぎるため、ファイル名だけを抽出して表示
                    std::filesystem::path p        = context.prefabFiles[i];
                    std::string           filename = p.filename().string();

                    // メニューアイテムがクリックされたら即座にSpawnする
                    if (ImGui::MenuItem(filename.c_str())) {
                        if (context.world) {
                            // TODO:
                            // 将来的にはカメラの注視点や、選択中のオブジェクトの場所に生成する
                            DirectX::XMFLOAT3 spawnPos = {0, 5, 0};
                            Prefab::SpawnPrefab(*context.world, context.prefabFiles[i], spawnPos);
                        }
                    }
                }
            }

            ImGui::EndMenu();
        }


        // シーン移行 メニュー
        if (ImGui::BeginMenu("Scene")) {

            if (ImGui::MenuItem("Title Scene")) SceneManager::Instance().ChangeScene("Title");
            if (ImGui::MenuItem("Game Scene")) SceneManager::Instance().ChangeScene("Game");

            if (ImGui::MenuItem("Particle Scene")) {
                SceneManager::Instance().ChangeScene("Particle");
            }
            if (ImGui::MenuItem("Editor Scene")) {
                SceneManager::Instance().ChangeScene("Editor");
            }
            if (ImGui::MenuItem("Shop Scene")) SceneManager::Instance().ChangeScene("Shop");


              ImGui::EndMenu();
        }

            // --- Window / View Menu (将来用) ---
            if (ImGui::BeginMenu("Window")) {

                // ここでも切り替えられるようにする (F1キーの案内も兼ねる)
                // MenuItemの第3引数にboolポインタを渡すと、チェック状態が連動します
                if (ImGui::MenuItem("Show UI", "F1", &_isVisible)) {
                    // クリックされたら自動で _isVisible が反転します
                }

                ImGui::Separator();

                for (auto &window : _windows) {
                    bool open = window->GetVisible();
                    if (ImGui::MenuItem(window->GetName().c_str(), nullptr, &open)) {
                        window->SetVisible(open);
                    }
                }
                ImGui::EndMenu();
            }

        
        ImGui::EndMainMenuBar();
    }
}