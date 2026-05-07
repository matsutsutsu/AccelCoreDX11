#include "ECSDebugWindow.h"
#include <implot.h>
#include "ECS/Core/CCL_Chunk.h"
#include "ECS/Core/CCL_PendingOps.h"

#include "Editor/Inspector/ComponentGuiRegistry.h"

// ---------------------------------------------------------
// ScrollingBuffer の実装
// ---------------------------------------------------------
ScrollingBuffer::ScrollingBuffer(int max_size) {
    MaxSize = max_size;
    Offset = 0;
    Data.reserve(MaxSize);
}

void ScrollingBuffer::AddPoint(float x, float y) {
    if (Data.size() < MaxSize)
        Data.push_back(ImVec2(x, y));
    else {
        Data[Offset] = ImVec2(x, y);
        Offset = (Offset + 1) % MaxSize;
    }
}

// ---------------------------------------------------------
// ECSDebugWindow の実装
// ---------------------------------------------------------
ECSDebugWindow::ECSDebugWindow() : EditorWindow("ECS Debugger") {}

void ECSDebugWindow::DrawContents(EditorContext& context) {
    if (!context.world || !context.systemManager) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "World or SystemManager is not initialized.");
        return;
    }

    if (ImGui::BeginTabBar("ECSDebugTabs")) {
        if (ImGui::BeginTabItem("Memory & Chunks")) {
            DrawMemoryTab(context.world);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Systems Profiler")) {
            DrawSystemsTab(context.systemManager);
            ImGui::EndTabItem();
        }

        // 実行グラフのタブ
        if (ImGui::BeginTabItem("Execution Graph")) {
            DrawExecutionGraphTab(context.systemManager);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Pending Ops")) {
            DrawPendingOpsTab(context.world);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

void ECSDebugWindow::DrawSystemsTab(CCL::ECS::SystemManager* systemManager) {
    float dt = ImGui::GetIO().DeltaTime;

    // =======================================================
    // 1. ツールバー (UXの劇的改善)
    // =======================================================
    ImGui::Checkbox("Pause Profiler", &_pauseProfiler);
    ImGui::SameLine();

    // ★ UX改善 1: 一括ON/OFFボタン
    if (ImGui::Button("Hide All")) {
        for (auto& pair : _systemTimings) pair.second.isVisible = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Show All")) {
        for (auto& pair : _systemTimings) pair.second.isVisible = true;
    }

    // ★ UX改善 2: グラフのY軸上限を手動で固定するスライダー
    // これにより、グラフが勝手に伸び縮みして目が回るのを防ぎます
    static float yAxisMax = 16.66f; // デフォルトは60fps(16.6ms)のライン
    ImGui::SameLine(0, 20);
    ImGui::SetNextItemWidth(150);
    ImGui::SliderFloat("Y-Axis Max (ms)", &yAxisMax, 1.0f, 60.0f, "%.1f");


    // データの記録用ヘルパー (前回と同じ)
    auto addData = [&](const std::string& name, float timeMs) {
        auto& data = _systemTimings[name];
        if (data.buffer.Data.empty()) {
            float hue = fmod(_systemTimings.size() * 0.618033988749895f, 1.0f);
            float r, g, b;
            ImGui::ColorConvertHSVtoRGB(hue, 0.7f, 0.9f, r, g, b);
            data.color = ImVec4(r, g, b, 1.0f);
        }
        data.buffer.AddPoint(_time, timeMs);
        };

    // 2. データの記録 (前回と同じ)
    if (!_pauseProfiler) {
        _time += dt;
        for (const auto& sys : systemManager->GetLogicSystems()) {
            addData(sys->GetName(), sys->GetLastUpdateTime());
        }
        for (const auto& sys : systemManager->GetRenderSystems()) {
            addData(sys->GetName(), sys->GetLastUpdateTime());
        }
    }

    // =======================================================
    // 3. ImPlot による折れ線グラフの描画
    // =======================================================
    if (ImPlot::BeginPlot("System Execution Times (ms)", ImVec2(-1, 250))) {

        // ★ UX改善 2の適用: Y軸の最大値をスライダーの値で「完全に固定」する
        ImPlot::SetupAxes("Time (s)", "Execution Time (ms)", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_Lock);
        ImPlot::SetupAxisLimits(ImAxis_X1, _time - 5.0, _time, ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, yAxisMax, ImGuiCond_Always); // ★ Y軸を固定

        for (auto& pair : _systemTimings) {
            if (!pair.second.isVisible || pair.second.buffer.Data.empty()) continue;

            float thickness = pair.second.isHovered ? 3.0f : 1.0f;
            ImPlot::SetNextLineStyle(pair.second.color, thickness);
            ImPlot::PlotLine(pair.first.c_str(),
                &pair.second.buffer.Data[0].x,
                &pair.second.buffer.Data[0].y,
                pair.second.buffer.Data.size(),
                0,
                pair.second.buffer.Offset,
                sizeof(ImVec2));
        }
        ImPlot::EndPlot();
    }

    ImGui::Separator();

    // =======================================================
    // 4. 詳細テーブル
    // =======================================================
    ImGui::Text("Current Performance Breakdown");
    if (ImGui::BeginTable("SystemPerfTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY, ImVec2(0, 0))) {
        ImGui::TableSetupColumn("System Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Latest Time (ms)", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Load Bar", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableHeadersRow();

        for (auto& pair : _systemTimings) {
            if (pair.second.buffer.Data.empty()) continue;

            int latestIdx = 0;
            if (pair.second.buffer.Data.size() < pair.second.buffer.MaxSize) {
                latestIdx = pair.second.buffer.Data.size() - 1;
            }
            else {
                latestIdx = (pair.second.buffer.Offset == 0) ? pair.second.buffer.MaxSize - 1 : pair.second.buffer.Offset - 1;
            }
            float latestTime = pair.second.buffer.Data[latestIdx].y;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            ImGui::PushID(pair.first.c_str());
            ImGui::Checkbox("##vis", &pair.second.isVisible);
            ImGui::SameLine();

            ImGui::ColorButton("##col", pair.second.color, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(14, 14));
            ImGui::SameLine();

            // ★ UX改善 4: Selectableの引数を修正し、クリック時の違和感（点滅や入力奪取）を消滅させました
            ImGui::Selectable(pair.first.c_str(), false, ImGuiSelectableFlags_SpanAllColumns);
            pair.second.isHovered = ImGui::IsItemHovered();

            ImGui::PopID();

            ImGui::TableNextColumn();
            ImGui::Text("%.3f ms", latestTime);

            ImGui::TableNextColumn();
            float ratio = latestTime / 16.666f;
            ImVec4 barColor = (ratio > 0.5f) ? ImVec4(1, 0, 0, 1) : ImVec4(0, 1, 0, 1);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColor);
            ImGui::ProgressBar(ratio, ImVec2(-1, 14), "");
            ImGui::PopStyleColor();
        }
        ImGui::EndTable();
    }
}


// =========================================================
// ヘルパー関数: 型IDから一時的な文字列を生成
// =========================================================
const char* ECSDebugWindow::GetFriendlyTypeName(uint32_t id) {
    // ★ 修正: Registryの既存機能を使って名前を逆引きする
    const std::string& name = ComponentGuiRegistry::Instance().GetComponentName(id);

    // 辞書に登録されていればその名前を返す
    if (!name.empty()) {
        return name.c_str();
    }

    // エディタ用GUIを持たない隠しコンポーネントなどの場合はIDでフォールバック
    static char buf[64];
    snprintf(buf, sizeof(buf), "Unknown (ID:%u)", id);
    return buf;
}

// =========================================================
// ヘルパー関数: 安全で美しい16進数（Hex）メモリダンプ描画
// =========================================================
static void DrawHexDump(const void* data, size_t size) {
    const unsigned char* bytes = static_cast<const unsigned char*>(data);
    char lineBuffer[128];

    // 1行あたり16バイトずつ描画
    for (size_t i = 0; i < size; i += 16) {
        int offset = snprintf(lineBuffer, sizeof(lineBuffer), "%04zX: ", i);

        // 16進数部分
        for (size_t j = 0; j < 16; ++j) {
            if (i + j < size) {
                offset += snprintf(lineBuffer + offset, sizeof(lineBuffer) - offset, "%02X ", bytes[i + j]);
            }
            else {
                offset += snprintf(lineBuffer + offset, sizeof(lineBuffer) - offset, "   ");
            }
        }

        // ASCII文字部分（右側に表示）
        offset += snprintf(lineBuffer + offset, sizeof(lineBuffer) - offset, " | ");
        for (size_t j = 0; j < 16 && i + j < size; ++j) {
            char c = bytes[i + j];
            // 表示可能な文字ならそのまま、それ以外はピリオド '.' にする
            lineBuffer[offset++] = (c >= 32 && c < 127) ? c : '.';
        }
        lineBuffer[offset] = '\0';

        ImGui::TextDisabled("%s", lineBuffer);
    }
}

// =========================================================
// タブ1: Memory & Chunks (メモリのヒートマップとインスペクタ)
// =========================================================
void ECSDebugWindow::DrawMemoryTab(CCL::ECS::Core::World* world) {
    auto& chunks = world->GetChunkManager().GetChunks();

    // =======================================================
    // チャンクの全体統計
    // =======================================================
    size_t totalEntities = 0;
    size_t totalCapacity = 0;
    size_t totalMemory = 0;

    for (const auto& chunk : chunks) {
        if (!chunk) continue;
        totalEntities += chunk->GetEntityCount();
        totalCapacity += chunk->GetCapacity();
        totalMemory += chunk->GetChunkSize() * chunk->GetCapacity();
    }

    ImGui::Text("Active Chunks: %zu", chunks.size());
    ImGui::SameLine(200);
    ImGui::Text("Total Entities: %zu / %zu", totalEntities, totalCapacity);
    ImGui::SameLine(450);
    ImGui::Text("Est. Component Memory: %.3f MB", (float)totalMemory / (1024.0f * 1024.0f));
    ImGui::Spacing();

    static int selectedChunkIdx = -1;

    ImGui::Columns(2, "MemoryTabSplitter", true);
    ImGui::SetColumnWidth(0, ImGui::GetWindowWidth() * 0.40f);

    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Chunk Hierarchy by Archetype");
    ImGui::Separator();

    // --- 左カラム: アーキタイプごとにグループ化されたチャンクリスト ---
    if (ImGui::BeginChild("ChunkList")) {

        // ★ 改良 1: チャンクをアーキタイプハッシュごとに分類する
        std::map<uint64_t, std::vector<int>> chunksByArchetype;
        for (size_t i = 0; i < chunks.size(); ++i) {
            if (chunks[i]) chunksByArchetype[chunks[i]->GetArchetypeHash()].push_back((int)i);
        }

        // 分類したアーキタイプごとにツリーを描画
        for (const auto& pair : chunksByArchetype) {
            uint64_t hash = pair.first;
            const auto& chunkIndices = pair.second;

            // ツリーノードの表示（例: Archetype [1284912] (3 Chunks) ）
            char archLabel[128];
            snprintf(archLabel, sizeof(archLabel), "Archetype [%llu] (%zu Chunks)", hash, chunkIndices.size());

            if (ImGui::TreeNode(archLabel)) {
                for (int idx : chunkIndices) {
                    auto& chunk = chunks[idx];
                    size_t count = chunk->GetEntityCount();
                    size_t cap = chunk->GetCapacity();

                    ImGui::PushID(idx);
                    bool isSelected = (selectedChunkIdx == idx);

                    char label[128];
                    snprintf(label, sizeof(label), "Chunk %d [%zu/%zu]", idx, count, cap);
                    if (ImGui::Selectable(label, isSelected)) {
                        selectedChunkIdx = idx;
                    }

                    // ヒートマップの描画
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    ImVec2 p = ImGui::GetCursorScreenPos();
                    float width = ImGui::GetContentRegionAvail().x;
                    float height = 10.0f; // 少し高さを抑えてスタイリッシュに
                    float boxWidth = width / (cap > 0 ? cap : 1);

                    drawList->AddRectFilled(p, ImVec2(p.x + width, p.y + height), IM_COL32(30, 30, 30, 255), 2.0f);

                    const bool hasDestroyed = chunk->HasDestroyedEntities();
                    for (size_t e = 0; e < count; ++e) {
                        ImU32 color = IM_COL32(0, 200, 100, 255);
                        if (hasDestroyed && chunk->IsEntityDestroyed(e)) {
                            color = IM_COL32(200, 50, 50, 255);
                        }
                        drawList->AddRectFilled(
                            ImVec2(p.x + e * boxWidth, p.y), // 隙間を無くしてソリッドなバーにする
                            ImVec2(p.x + (e + 1) * boxWidth - 0.5f, p.y + height),
                            color
                        );
                    }
                    ImGui::Dummy(ImVec2(width, height + 4));
                    ImGui::PopID();
                }
                ImGui::TreePop();
            }
        }
    }
    ImGui::EndChild();

    ImGui::NextColumn();

    // --- 右カラム: 選択中チャンクのエンティティ詳細 ---
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Memory Inspector (Hex Dump)");
    ImGui::Separator();

    if (selectedChunkIdx >= 0 && selectedChunkIdx < (int)chunks.size()) {
        auto& chunk = chunks[selectedChunkIdx];
        if (chunk && ImGui::BeginChild("EntityDetailList")) {

            // チャンクに含まれるコンポーネントの種類を上部に表示
            ImGui::Text("Components in this chunk:");
            for (const auto& td : chunk->GetTypeDatas()) {
                ImGui::BulletText("%s (%zu bytes)", GetFriendlyTypeName(td.id), td.typeSize);
            }
            ImGui::Separator();
            ImGui::Spacing();

            for (size_t e = 0; e < chunk->GetEntityCount(); ++e) {
                // ★ 改良: 削除済みのエンティティはスキップするか、目立たなくする
                if (chunk->HasDestroyedEntities() && chunk->IsEntityDestroyed(e)) continue;

                CCL::ECS::EntityID id = chunk->GetEntityByIndex(e);
                char eLabel[64];
                snprintf(eLabel, sizeof(eLabel), "Slot %zu: Entity ID %llu", e, id);

                if (ImGui::TreeNode(eLabel)) {
                    for (const auto& td : chunk->GetTypeDatas()) {
                        if (ImGui::TreeNode((void*)(intptr_t)td.id, "Component: %s", GetFriendlyTypeName(td.id))) {
                            void* data = chunk->GetComponentPtrByType(td.id, e);
                            if (data) {

                                // =========================================================
                                // ★ 究極の進化: あなたの構築したGUIレジストリを呼び出すだけ！
                                // =========================================================
                                // これにより、TransformならPosition(x,y,z)などのスライダーが自動生成されます。
                                // Undo/Redo のフックも ComponentMetaImGui 内で自動処理されます。
                                ComponentGuiRegistry::Instance().Draw(td.id, data, id, world);


                                ImGui::Spacing();

                                // =========================================================
                                // 既存のHexダンプは「Advanced (詳細)」として折りたたんで残す
                                // =========================================================
                                if (ImGui::TreeNodeEx("Raw Memory (Hex Dump)")) {
                                    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
                                    DrawHexDump(data, td.typeSize);
                                    ImGui::PopFont();
                                    ImGui::TreePop();
                                }
                            }
                            ImGui::TreePop();
                        }
                    }
                    ImGui::TreePop();
                }
            }
        }
        ImGui::EndChild();
    }
    else {
        ImGui::TextDisabled("Select a chunk on the left to see details.");
    }
    ImGui::Columns(1);
}

// =========================================================
// タブ3: Pending Operations (遅延処理の可視化)
// =========================================================
void ECSDebugWindow::DrawPendingOpsTab(CCL::ECS::Core::World* world) {
    auto& pending = world->GetPendingOps();
    auto& ops = pending.Ops();

    ImGui::Text("Queue Size: %zu", ops.size());
    ImGui::SameLine();

    // ★ 改良: ボタンの色を目立たせる
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
    if (ImGui::Button("Sync Now (ScrutinyAndApply)")) {
        world->ScrutinyAndApply();
    }
    ImGui::PopStyleColor();
    ImGui::Separator();

    if (ImGui::BeginChild("OpsLog", ImVec2(0, 0), true)) {
        for (const auto& op : ops) {
            const char* typeName = "Unknown";
            ImVec4 color = ImVec4(1, 1, 1, 1);

            // 操作に応じた色分け
            switch (op.kind) {
            case CCL::ECS::Core::PendingOpKind::Spawn:           typeName = "SPAWN";   color = ImVec4(0.2f, 1.0f, 0.2f, 1.0f); break;
            case CCL::ECS::Core::PendingOpKind::Destroy:         typeName = "DESTROY"; color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f); break;
            case CCL::ECS::Core::PendingOpKind::AddComponent:    typeName = "ADD";     color = ImVec4(0.2f, 0.7f, 1.0f, 1.0f); break;
            case CCL::ECS::Core::PendingOpKind::RemoveComponent: typeName = "REMOVE";  color = ImVec4(1.0f, 0.6f, 0.2f, 1.0f); break;
            }

            ImGui::TextColored(color, "[%s]", typeName);
            ImGui::SameLine();
            ImGui::Text("EntityID: %llu | TypeID: %u", op.entity, op.type);
        }
    }
    ImGui::EndChild();
}

// ---------------------------------------------------------
//  実行グラフの可視化
// ---------------------------------------------------------
void ECSDebugWindow::DrawExecutionGraphTab(CCL::ECS::SystemManager* systemManager) {
    ImGui::TextWrapped("Shows the current parallel execution batches resolved by the TaskScheduler.");
    ImGui::Spacing();

    auto drawGraph = [](const char* title, const CCL::ECS::TaskScheduler& scheduler) {
        ImGui::SeparatorText(title);
        const auto& batches = scheduler.GetBatches();
        if (batches.empty()) {
            ImGui::TextDisabled("Graph not built yet or no systems registered.");
            return;
        }
        for (size_t i = 0; i < batches.size(); ++i) {
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Batch %zu (Parallel):", i);
            ImGui::Indent();
            for (auto* sys : batches[i]) {
                ImGui::BulletText("%s", sys->GetName().c_str());
            }
            ImGui::Unindent();
            ImGui::Spacing();
        }
        };

    drawGraph("Logic Systems Graph", systemManager->GetLogicScheduler());
    ImGui::Spacing();
    drawGraph("Render Systems Graph", systemManager->GetRenderScheduler());
}