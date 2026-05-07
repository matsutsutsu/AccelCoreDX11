#include "Editor/Windows/HierarchyWindow.h"
#include "Game/Core/AllComponents.h"
#include <algorithm>
#include <string>
#include "Engine/GamePlay/Transform/PendingParentComponent.h"
#include "Engine/Platform/Logger.h"

#include "Engine/Serialization/Factory/Prefab.h"
#include "Engine/Serialization/Factory/EntityFactory.h"


HierarchyWindow::HierarchyWindow() : EditorWindow("Hierarchy") {}

void HierarchyWindow::DrawContents(EditorContext& context)
{
    auto* world = context.world;
    auto& chunkManager = world->GetChunkManager();
    auto& chunks = chunkManager.GetChunks();

    // =========================================================================
    // ★ 究極の修正: 毎フレーム必ずキャッシュを空にする！
    // これを忘れると過去の親子関係が蓄積し、全てが循環参照と誤認されます。
    // =========================================================================
    _rootEntities.clear();
    _allEntities.clear();
    _flatList.clear();
    _childrenMap.clear();

    // ---------------------------------------------------------
    // ツールバー (生成 ＆ 検索)  ※ここが消えていた部分です！
    // ---------------------------------------------------------
    if (ImGui::Button("Spawn Entity")) {
        auto arch = ArchetypeHelper::Generate<TransformComponent, NameComponent>();
        EntityFactory::SpawnNamed(*world, "New Entity ", arch);
    }

    ImGui::SameLine();
    // 検索バーの実装
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##Search", "Search Entity...", _searchBuffer, sizeof(_searchBuffer));

    bool        isSearching = strlen(_searchBuffer) > 0;
    std::string searchStr = _searchBuffer;
    // 小文字に変換して大文字小文字を区別しない検索にする
    std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);

    ImGui::Separator();

    // ---------------------------------------------------------
    // 1. データ収集フェーズ (★ゼロ・アロケーション化)
    // ---------------------------------------------------------
    // ベクターやマップの「容量」は維持したまま、中身の要素数だけを0に戻す（超高速）
    _rootEntities.clear();
    _allEntities.clear();
    _flatList.clear();
    // マップ内のベクターも容量を維持してクリアする
    for (auto& pair : _childrenMap) pair.second.clear();

    for (auto& chunkPtr : chunks) {
        if (!chunkPtr) continue;
        CCL::ECS::Core::Chunk* chunk = chunkPtr.get();
        const size_t           count = chunk->GetEntityCount();

        TransformComponent* transforms = chunk->HasComponent<TransformComponent>()
            ? chunk->GetComponentArray<TransformComponent>()
            : nullptr;

        for (size_t i = 0; i < count; ++i) {
            CCL::ECS::EntityID entity = chunk->GetEntityByIndex(i);
            if (entity == CCL::ECS::InvalidEntityID) continue;

            _allEntities.push_back(entity);

            bool hasParent = false;
            if (transforms && transforms[i].parentID != 0) {
                _childrenMap[transforms[i].parentID].push_back(entity);
                hasParent = true;
            }
            if (!hasParent) {
                _rootEntities.push_back(entity);
            }
        }
    }

    // ---------------------------------------------------------
    // 2. 平坦化フェーズ 
    // ---------------------------------------------------------
    if (isSearching) {
        for (auto e : _allEntities) {
            // ★検索中のみ、重い文字列処理を行う
            auto* nameComp = world->GetComponent<NameComponent>(e);
            std::string nameLower = (nameComp && strlen(nameComp->name) > 0) ? nameComp->name : "entity " + std::to_string(e);
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

            if (nameLower.find(searchStr) != std::string::npos) {
                _flatList.push_back({ e, 0, false });
            }
        }
    }
    else {
        for (auto root : _rootEntities) {
            FlattenTree(root, 0, _childrenMap, _flatList);
        }
    }

    // ---------------------------------------------------------
    // 3. 描画フェーズ 
    // ---------------------------------------------------------
    ImGui::BeginChild("HierarchyTree", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    ImGuiListClipper clipper;
    clipper.Begin((int)_flatList.size());
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            DrawFlatNode(_flatList[i], context, _childrenMap);
        }
    }
    clipper.End();

    ImGui::EndChild();

    // ---------------------------------------------------------
    // 4. ドラッグ＆ドロップ受け入れ処理と選択解除
    // ---------------------------------------------------------
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PREFAB")) {
            const char* prefabPath = (const char*)payload->Data;
            CCL::ECS::EntityID newEntity = Prefab::SpawnPrefab(*context.world, prefabPath);
            if (newEntity != CCL::ECS::InvalidEntityID) {
                context.selectedEntity = newEntity;
            }
        }
        ImGui::EndDragDropTarget();
    }
 
    // =========================================================================
    // ★ 追加: ヒエラルキーの背景へのドロップ判定 (親の解除 / ルート化)
    // =========================================================================
    // 残りのウィンドウ領域をダミーアイテムで埋めて、ドロップ判定を有効にする
    ImGui::Dummy(ImGui::GetContentRegionAvail());
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_ENTITY_ID")) {
            CCL::ECS::EntityID draggedEntity = *(const CCL::ECS::EntityID*)payload->Data;

            // 親を 0（なし）に設定して予約
            if (context.world->HasComponent<PendingParentComponent>(draggedEntity)) {
                context.world->GetComponent<PendingParentComponent>(draggedEntity)->parentID = 0;
            }
            else {
                context.world->AddComponent<PendingParentComponent>(draggedEntity);
            }
            CCL_LOG_INFO(LogCategory::Editor, "Unparenting: Entity %u is now root", draggedEntity);
        }
        ImGui::EndDragDropTarget();
    }
    // 背景クリック時の選択解除
    if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered()) {
        context.selectedEntity = CCL::ECS::InvalidEntityID;
    }
}


// =========================================================================
// 個別ノードの描画 (再帰を排除した超高速描画)
// =========================================================================
void HierarchyWindow::DrawFlatNode(
    const FlatNode& node,
    EditorContext& context,
    const std::unordered_map<CCL::ECS::EntityID, std::vector<CCL::ECS::EntityID>>& childrenMap)
{
    // =========================================================
    // ★遅延評価: 画面に映る「たった40個」に対してだけ名前を生成する！
    // =========================================================
    std::string name = "Entity " + std::to_string(node.entityID);
    auto* nameComp = context.world->GetComponent<NameComponent>(node.entityID);
    if (nameComp && strlen(nameComp->name) > 0) {
        name = nameComp->name;
    }

    // ★重要: 深さに応じてUIの表示位置を右にずらす（見た目だけツリーにする）
    ImGui::Indent(node.depth * ImGui::GetTreeNodeToLabelSpacing());

    // ★重要: ImGuiTreeNodeFlags_NoTreePushOnOpen を必ずつける！
    // これにより、ImGuiの内部スタックを汚さず、単なる「見た目だけ」のツリーノードになります。
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_NoTreePushOnOpen;



    if (!node.hasChildren) flags |= ImGuiTreeNodeFlags_Leaf;
    if (context.selectedEntity == node.entityID) flags |= ImGuiTreeNodeFlags_Selected;

    // エンジン側で管理している開閉状態を ImGui に強制的に教える
    bool isOpen = _openNodes.count(node.entityID) > 0;
    ImGui::SetNextItemOpen(isOpen);

    // 描画
    ImGui::TreeNodeEx((void*)(uintptr_t)node.entityID, flags, "%s", name.c_str());

    // ユーザーが左の矢印(▶)をクリックして開閉状態を切り替えた時の処理
    if (ImGui::IsItemToggledOpen()) {
        if (isOpen) _openNodes.erase(node.entityID); // 閉じる
        else        _openNodes.insert(node.entityID); // 開く
    }

    // =======================================================================
    // ★ここから追加: ドラッグ＆ドロップの「送信元（Source）」としての設定
    // =======================================================================
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {

        // "DND_ENTITY_ID" という識別子で、EntityIDのメモリの塊をペイロードとして積む
        ImGui::SetDragDropPayload("DND_ENTITY_ID", &node.entityID, sizeof(CCL::ECS::EntityID));

        // ドラッグ中、マウスカーソルの横に表示されるプレビューUI
        ImGui::Text("Drop Entity: %u", node.entityID);

        ImGui::EndDragDropSource();
    }

    // =========================================================================
    // ★ 追加: ドラッグ＆ドロップ：受信先 (親子関係の設定)
    // =========================================================================
    if (ImGui::BeginDragDropTarget()) {
        // "DND_ENTITY_ID" タグの荷物を受け取る
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_ENTITY_ID")) {
            CCL::ECS::EntityID draggedEntity = *(const CCL::ECS::EntityID*)payload->Data;
            CCL::ECS::EntityID targetParent = node.entityID;

            // ① 自分自身には入れられない
            // ② ドロップ先(targetParent)が、ドラッグしている奴(draggedEntity)の子供や孫であってはならない
            if (draggedEntity != targetParent && !IsDescendantOf(targetParent, draggedEntity, childrenMap)) {
                // PendingParentComponent を使用して親子関係を予約
                if (context.world->HasComponent<PendingParentComponent>(draggedEntity)) {
                    context.world->GetComponent<PendingParentComponent>(draggedEntity)->parentID = targetParent;
                }
                else {
					PendingParentComponent ppc;
					ppc.parentID = targetParent;
                    context.world->AddComponent<PendingParentComponent>(draggedEntity, ppc);
                }
                CCL_LOG_INFO(LogCategory::Editor, "Parenting: Entity %u is now child of %u", draggedEntity, targetParent);
            }
        }
        else {
            // ブロックされた場合は警告を出す
            CCL_LOG_WARN(LogCategory::Editor, "Parenting Failed: Circular reference or self-parenting detected!");
        }
        
        ImGui::EndDragDropTarget();
    }

    // =======================================================================
    // クリック（選択）判定を「スマート・クリック」に進化させる
    // =======================================================================
    // 条件1: アイテムの上でマウスがホバーされている
    // 条件2: マウスの左ボタンが「離された（Released）」瞬間である
    // 条件3: TreeNodeの開閉ボタン(矢印)が押されたわけではない
    // 条件4: ★ ドラッグ操作をしていない（閾値以上マウスが移動していない）
    
    if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {

        // ImGuiの標準設定（io.MouseDragThreshold）を超えてドラッグされていないかチェック
        // MouseDragThreshold はデフォルトで 6.0f 程度に設定されています。
        if (!ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            // かつ、矢印（開閉ボタン）を押していない場合のみ選択を切り替える
            if (!ImGui::IsItemToggledOpen()) {
                context.selectedEntity = node.entityID;
            }
        }
    }

    // ---------------------------------------------------------
    //  右クリックメニュー (独立/Unparent 機能の追加)
    // ---------------------------------------------------------
    if (ImGui::BeginPopupContextItem()) {
        
        // 追加: 親から外れてルートに戻るボタン
        if (ImGui::MenuItem("Unparent (Make Root)")) {
            if (context.world->HasComponent<PendingParentComponent>(node.entityID)) {
                context.world->GetComponent<PendingParentComponent>(node.entityID)->parentID = 0;
            } else {
                context.world->AddComponent<PendingParentComponent>(node.entityID); // 0は親なしを意味する
            }
            CCL_LOG_INFO(LogCategory::Editor, "Entity %u unparented.", node.entityID);
        }
        
        ImGui::Separator(); // 区切り線

        if (ImGui::MenuItem("Delete Entity")) {
            context.world->Destroy(node.entityID);
            if (context.selectedEntity == node.entityID) context.selectedEntity = CCL::ECS::InvalidEntityID;
        }
        ImGui::EndPopup();
    }

    // UIの位置を元に戻す
    ImGui::Unindent(node.depth * ImGui::GetTreeNodeToLabelSpacing());
}


// =========================================================================
// ツリーを1次元の配列に押しつぶすアルゴリズム (非常に高速)
// =========================================================================
void HierarchyWindow::FlattenTree(
    CCL::ECS::EntityID entityID,
    int depth,
    const std::unordered_map<CCL::ECS::EntityID, std::vector<CCL::ECS::EntityID>>& childrenMap,
    std::vector<FlatNode>& outFlatList)
{
    // =========================================================================
    //  単にキーが存在するかだけでなく、「本当に中身(子供)がいるか」を確認する
    // =========================================================================
    bool hasChildren = childrenMap.count(entityID) > 0 && !childrenMap.at(entityID).empty();

    // まず自分自身をリストに追加する
    outFlatList.push_back({ entityID, depth, hasChildren });

    // ★もし自分に子供がいて、かつ「開かれている」状態なら、子供も再帰的にリストに追加する
    if (hasChildren && _openNodes.count(entityID) > 0) {
        const auto& children = childrenMap.at(entityID);
        for (auto childID : children) {
            FlattenTree(childID, depth + 1, childrenMap, outFlatList);
        }
    }
}

// =========================================================================
//  循環参照チェックの実装 (ファイルの末尾などに追加)
// =========================================================================
bool HierarchyWindow::IsDescendantOf(CCL::ECS::EntityID targetEntity, CCL::ECS::EntityID ancestorEntity, const std::unordered_map<CCL::ECS::EntityID, std::vector<CCL::ECS::EntityID>>& childrenMap)
{
    // 自分自身へのドロップも循環とみなす
    if (targetEntity == ancestorEntity) return true;

    // 先祖の子供リストを取得
    auto it = childrenMap.find(ancestorEntity);
    if (it != childrenMap.end()) {
        for (auto child : it->second) {
            // 子供、あるいはそのさらに子供がターゲットなら true (危険)
            if (IsDescendantOf(targetEntity, child, childrenMap)) {
                return true;
            }
        }
    }
    return false; // 安全
}