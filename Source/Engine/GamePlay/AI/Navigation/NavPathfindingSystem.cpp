#include "NavPathfindingSystem.h"
#include "Engine/GamePlay/AI/Navigation/NavigationData.h"
#include <DetourNavMeshQuery.h>
#include "Engine/Platform/Logger.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

// ===================================================================================
// 【 ナビゲーション (経路探索) : NavPathfindingSystem 】
//
// [ 役割 ]
// Detourライブラリを用い、目的地までの最短経路（Waypoints）を計算する「地図読み」。
// 
// [ 実行順序 ]
// L02_AI_Think (220) : AIが目的地を決定した直後に、その道筋を計算する。
//
// [ ECSデータパイプライン ]
// 📥 READ  : TransformComponent (現在地)
// 📥 READ  : NavigationData     (NavMeshのリソースデータ)
// 📤 WRITE : NavAgentComponent  (waypointsリストの更新、isPathRequestedのリセット)
// ===================================================================================

void NavPathfindingSystem::Update(float dt) {

    if (!_world || !_world->HasResource<NavigationData>()) return;

    auto& navData = _world->GetResource<NavigationData>();
    dtNavMeshQuery* navQuery = navData.navQuery;
    if (!navQuery) return;

    // 探索フィルター（今回は全エリアを歩行可能として扱う設定）
    dtQueryFilter filter;
    filter.setIncludeFlags(0xFFFF);
    filter.setExcludeFlags(0);

    // 「今立っている座標」から、足元にあるNavMeshを探すための検索範囲 (X, Y, Z)
    // AIが少し宙に浮いていてもNavMeshを検知できるように、Y軸は少し大きめ(4.0)に取ります
    const float extents[3] = { 2.0f, 4.0f, 2.0f };

    ForEach([&](const TransformComponent& transform, NavAgentComponent& agent) {


        // 経路の計算要求が来ていなければスキップ（毎フレーム計算すると重いため）
        if (!agent.isPathRequested) return;

        agent.isPathRequested = false; // 要求を消化
        agent.waypoints.clear();
        agent.currentWaypointIndex = 0;

        float startPos[3] = { transform.position.x, transform.position.y, transform.position.z };
        float endPos[3] = { agent.targetPosition.x, agent.targetPosition.y, agent.targetPosition.z };

        dtPolyRef startRef = 0;
        dtPolyRef endRef = 0;
        float nearestStart[3], nearestEnd[3];

        // 1. スタート地点とゴール地点の「最寄りのポリゴン」を見つける
        navQuery->findNearestPoly(startPos, extents, &filter, &startRef, nearestStart);
        navQuery->findNearestPoly(endPos, extents, &filter, &endRef, nearestEnd);

        // ====================================================================
        // ★診断ログ: なぜ失敗したのかを特定する
        // ====================================================================
        if (!startRef) {
            CCL_LOG_ERROR(LogCategory::Core, "Pathfinding Failed: AI is outside the NavMesh! AI Pos: (%.2f, %.2f, %.2f)", startPos[0], startPos[1], startPos[2]);
            return;
        }
        if (!endRef) {
            CCL_LOG_ERROR(LogCategory::Core, "Pathfinding Failed: Target is outside the NavMesh! Target Pos: (%.2f, %.2f, %.2f)", endPos[0], endPos[1], endPos[2]);
            return;
        }

        // 2. ポリゴンレベルでの経路探索 (A*)
        const int MAX_POLYS = 256;
        dtPolyRef path[MAX_POLYS];
        int pathCount = 0;
        navQuery->findPath(startRef, endRef, nearestStart, nearestEnd, &filter, path, &pathCount, MAX_POLYS);

        if (pathCount > 0) {
            // 3. 直線経路（String Pulling）でウェイポイントのリストを計算
            const int MAX_WAYPOINTS = 256;
            float straightPath[MAX_WAYPOINTS * 3];
            unsigned char straightPathFlags[MAX_WAYPOINTS];
            dtPolyRef straightPathPolys[MAX_WAYPOINTS];
            int straightPathCount = 0;

            navQuery->findStraightPath(nearestStart, nearestEnd, path, pathCount,
                straightPath, straightPathFlags,
                straightPathPolys, &straightPathCount, MAX_WAYPOINTS);

            // 計算結果を NavAgentComponent の std::vector に詰め込む
            for (int i = 0; i < straightPathCount; ++i) {
                agent.waypoints.push_back({
                    straightPath[i * 3],
                    straightPath[i * 3 + 1],
                    straightPath[i * 3 + 2]
                    });
            }

            // ★成功ログ
            CCL_LOG_INFO(LogCategory::Core, "Pathfinding Success! Generated %d waypoints.", straightPathCount);
        }
        else {
            // ★失敗ログ（ポリゴンは見つかったが道が繋がっていない）
            CCL_LOG_ERROR(LogCategory::Core, "Pathfinding Failed: No valid path between start and end.");
        }
    });
}


REGISTER_LOGIC_SYSTEM(NavPathfindingSystem, Priority::LogicStage::L02_AI_Think);