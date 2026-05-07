#include "CCL_SystemManager.h"
#include "ECS/Core/CCL_Chunk.h"
#include <imgui.h>


namespace CCL::ECS
{
    using namespace CCL::ECS::Core;

    void SystemManager::ResolveTargetChunks(Core::ChunkManager* cm)
    {
        // チャンク振り分け負荷を計測
        ZoneScopedN("SystemManager::ResolveTargetChunks");


        if (!cm) return;

         // 読み取りロックを取得（Chunk構成を読むため）
        std::shared_lock<std::shared_mutex> lock(cm->GetStructureMutex());


        // =========================================================
        // ★ 厳格なキャッシュ判定（ポインタ配列の完全一致チェック）
        // =========================================================
        auto& chunks = cm->GetChunks(); // vector<unique_ptr<Chunk>>&
        bool isStructureDirty = false;

        // 1. まず「数」が違うなら確実に構造が変化している
        if (chunks.size() != _cachedChunkPointers.size()) {
            isStructureDirty = true;
        }
        else {
            // 2. 数が同じでも、中身のチャンク（メモリアドレス）が入れ替わっていないか確認する
            for (size_t i = 0; i < chunks.size(); ++i) {
                if (chunks[i].get() != _cachedChunkPointers[i]) {
                    isStructureDirty = true; // 1つでも違えばDirty
                    break;
                }
            }
        }

        // 構造に変化がなければ、総当たりのマッチング計算をスキップして即終了！ (0.0ms)
        if (!isStructureDirty) {
            return;
        }

        // --- ここから下は「構造が変わった（エンティティ構成が変化した）時」だけ走る ---

        // キャッシュを最新の状態に更新しておく
        _cachedChunkPointers.clear();
        _cachedChunkPointers.reserve(chunks.size());
        for (auto& chunkPtr : chunks) {
            _cachedChunkPointers.push_back(chunkPtr.get());
        }

        // =========================================================
        // 1. 全Systemのターゲットチャンクをリセット (Logic & Render)
        // =========================================================
        
        // ヘルパーラムダ: システムリストを受け取ってクリアする
        auto clearTargets = [](auto& systems) {
            for (auto& sysPtr : systems) {
                sysPtr->ClearTargetChunks();
            }
            };

        clearTargets(_logicSystems);
        clearTargets(_renderSystems);


        // =========================================================
        // 2. 全チャンクを走査し、適合するシステムに配る
        // =========================================================
        for (auto& upChunk : chunks)
        {
            if (!upChunk) continue;
            Chunk* chunk = upChunk.get();
            const Archetype& chunkArch = chunk->GetArchetype();

            // ヘルパーラムダ: システムリストを受け取って適合判定を行う
            auto matchSystems = [&](auto& systems) {
                for (auto& sysPtr : systems)
                {
                    const Archetype& filter = sysPtr->GetFilter();

                    // フィルタ無し → 無条件で対象にする
                    if (filter.empty())
                    {
                        sysPtr->AddTargetChunk(chunk);
                        continue;
                    }

                    // Archetype一致判定 (ChunkがFilterの要件を満たしているか)
                    if (ArchetypeHelper::LhasR(chunkArch, filter))
                    {
                        sysPtr->AddTargetChunk(chunk);
                    }
                }
                };

            // Logic と Render 両方のシステムに対してマッチングを行う
            matchSystems(_logicSystems);
            matchSystems(_renderSystems);
        }
    }


   void SystemManager::OnGui()
    {
        if (ImGui::Begin("System Manager")) {

            // ----------------------------------------------------
            // マスタースイッチ (上部に大きく配置)
            // ----------------------------------------------------
            // 少し目立つように装飾しても良いでしょう
            if (ImGui::Checkbox("Global Multi-Thread Enable", &globalMultiThreadEnabled)) {
                for (auto &sys : _logicSystems) sys->enableMultiThread = globalMultiThreadEnabled;
                for (auto &sys : _renderSystems) sys->enableMultiThread = globalMultiThreadEnabled;
                
            }

            // 補足情報をツールチップで出すと親切
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Force enable/disable multi-threading for ALL systems.");
            }

            ImGui::Separator();

            if (ImGui::BeginTabBar("SystemManagerTabs")) {

                // --------------------------------------------------------
                // Tab 1: Monitor (テーブル化して見やすく整理)
                // --------------------------------------------------------
                if (ImGui::BeginTabItem("Monitor")) {

                    // テーブル開始
                    // フラグ解説:
                    // - ImGuiTableFlags_Borders: 枠線を表示
                    // - ImGuiTableFlags_RowBg: 1行ごとに背景色を交互に変える（見やすい）
                    // - ImGuiTableFlags_Resizable: 列幅を変えられるようにする
                    // - ImGuiTableFlags_Sortable: (任意) ソート機能をつけたい場合
                    static ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                                   ImGuiTableFlags_Resizable |
                                                   ImGuiTableFlags_ScrollY;

                    // 4カラム: Name, Time, MT, Debug
                    // 第2引数はカラム数
                    // 縦スクロールのために外側のウィンドウサイズに合わせる (-FLT_MIN)
                    if (ImGui::BeginTable("SystemMonitorTable", 4, flags, ImVec2(0, -FLT_MIN))) {

                        // ヘッダー設定 (幅の比率を指定)
                        ImGui::TableSetupColumn(
                            "System Name", ImGuiTableColumnFlags_WidthStretch); // 残りの幅全部
                        ImGui::TableSetupColumn(
                            "Time (ms)", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                        ImGui::TableSetupColumn(
                            "MT", ImGuiTableColumnFlags_WidthFixed, 40.0f); // Checkbox用
                        ImGui::TableSetupColumn(
                            "Debug", ImGuiTableColumnFlags_WidthFixed, 40.0f); // Checkbox用

                        // ヘッダー行を表示
                        ImGui::TableHeadersRow();

                        // 行描画用ラムダ
                        auto drawSystemRow = [](const std::unique_ptr<SystemBase> &sys) {
                            ImGui::TableNextRow();

                            // --- Column 0: Name ---
                            ImGui::TableSetColumnIndex(0);
                            ImGui::TextUnformatted(sys->GetName().c_str());

                            // --- Column 1: Time ---
                            ImGui::TableSetColumnIndex(1);
                            float time = sys->GetDisplayUpdateTime();
                            if (time > 2.0f)
                                ImGui::TextColored(ImVec4(1, 0.2f, 0.2f, 1), "%.3f", time);
                            else if (time > 0.5f)
                                ImGui::TextColored(ImVec4(1, 0.8f, 0.2f, 1), "%.3f", time);
                            else
                                ImGui::Text("%.3f", time);

                            // --- Column 2: MT Checkbox ---
                            ImGui::TableSetColumnIndex(2);
                            ImGui::PushID(sys->GetSystemID()); // ID重複防止
                            // 中央揃えにするテクニック
                            float cw = ImGui::GetColumnWidth();
                            float iw = ImGui::GetFrameHeight(); // Checkboxの幅
                            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (cw - iw) * 0.5f);
                            ImGui::Checkbox("##mt", &sys->enableMultiThread);
                            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Enable Multi-Threading");
                            ImGui::PopID();

                            // --- Column 3: Debug Checkbox ---
                            ImGui::TableSetColumnIndex(3);
                            ImGui::PushID(sys->GetSystemID() + 10000); // IDずらし
                            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (cw - iw) * 0.5f);
                            ImGui::Checkbox("##vis", &sys->isDebugVisible);
                            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Show Debug Draw");
                            ImGui::PopID();
                        };

                        // グループ分けして表示
                        // 行としてタイトルを挟む
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextDisabled("-- Logic Systems --");
                        for (auto &sys : _logicSystems) drawSystemRow(sys);

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextDisabled("-- Render Systems --");
                        for (auto &sys : _renderSystems) drawSystemRow(sys);

                        ImGui::EndTable();
                    }
                    ImGui::EndTabItem();
                }

                // --------------------------------------------------------
                // Tab 2: Logic Inspectors (各システムのOnGuiを表示)
                // --------------------------------------------------------
                if (ImGui::BeginTabItem("Logic Inspectors")) {
                    for (auto &sys : _logicSystems) {
                        ImGui::PushID(sys->GetSystemID());
                        // 折りたたみヘッダーの中にシステムのGUIを表示
                        if (ImGui::CollapsingHeader(sys->GetName().c_str())) {
                            ImGui::Indent(); // 少しインデントして階層構造を見やすく
                            sys->OnGui();    // ★ここで各システムのGUIを呼ぶ
                            ImGui::Unindent();
                            ImGui::Spacing();
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndTabItem();
                }

                // --------------------------------------------------------
                // Tab 3: Render Inspectors
                // --------------------------------------------------------
                if (ImGui::BeginTabItem("Render Inspectors")) {
                    for (auto &sys : _renderSystems) {
                        ImGui::PushID(sys->GetSystemID());
                        if (ImGui::CollapsingHeader(sys->GetName().c_str())) {
                            ImGui::Indent();
                            sys->OnGui();
                            ImGui::Unindent();
                            ImGui::Spacing();
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndTabItem();
                }

              

                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }


}
