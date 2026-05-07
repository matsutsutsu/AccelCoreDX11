#pragma once
#include "Editor/Core/EditorWindow.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>

class HierarchyWindow : public EditorWindow {
public:
    HierarchyWindow();

protected:
    void DrawContents(EditorContext& context) override;

private:
    struct FlatNode {
        CCL::ECS::EntityID entityID;
        int depth;
        bool hasChildren;
    };

    std::unordered_set<CCL::ECS::EntityID> _openNodes;

    // ★追加: 毎フレームのメモリ確保を防ぐためのキャッシュ用変数
    std::vector<CCL::ECS::EntityID> _rootEntities;
    std::vector<CCL::ECS::EntityID> _allEntities;
    std::vector<FlatNode>           _flatList;
    std::unordered_map<CCL::ECS::EntityID, std::vector<CCL::ECS::EntityID>> _childrenMap;

    void FlattenTree(
        CCL::ECS::EntityID entityID,
        int depth,
        const std::unordered_map<CCL::ECS::EntityID, std::vector<CCL::ECS::EntityID>>& childrenMap,
        std::vector<FlatNode>& outFlatList);

    void DrawFlatNode(
        const FlatNode& node,
        EditorContext& context,
        const std::unordered_map<CCL::ECS::EntityID, std::vector<CCL::ECS::EntityID>>& childrenMap);

    // ターゲットが、あるエンティティの子孫かどうかを判定する
    bool IsDescendantOf(
        CCL::ECS::EntityID targetEntity,
        CCL::ECS::EntityID ancestorEntity,
        const std::unordered_map<CCL::ECS::EntityID, std::vector<CCL::ECS::EntityID>>& childrenMap);

    char _searchBuffer[256] = { 0 };
};