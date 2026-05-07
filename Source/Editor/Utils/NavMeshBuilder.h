#pragma once
#include <vector>

namespace CCL::ECS::Core {
    class World;
}

// エディタ（ImGui）で調整するためのBakeパラメータ群
struct NavMeshBuildSettings {
    float cellSize = 0.3f;          // ボクセルのXY解像度（小さいほど高精度・重い）
    float cellHeight = 0.2f;        // ボクセルのZ(Y)解像度（高さの精度）
    float agentHeight = 2.0f;       // AIの身長（この高さ以下の天井は通れない）
    float agentRadius = 0.6f;       // AIの半径（壁からこの距離だけ離れる）
    float agentMaxClimb = 0.9f;     // 登れる段差の最大高（階段などをスロープ化）
    float agentMaxSlope = 45.0f;    // 歩ける最大傾斜角度

    // (以降はStep 3で使うパラメータですが定義しておきます)
    float regionMinSize = 8.0f;
    float regionMergeSize = 20.0f;
    float edgeMaxLen = 12.0f;
    float edgeMaxError = 1.3f;
    float vertsPerPoly = 6.0f;
    float detailSampleDist = 6.0f;
    float detailSampleMaxError = 1.0f;
};

struct NavMeshBuildData {
    std::vector<float> vertices;
    std::vector<int> indices;
};

class NavMeshBuilder {
public:
    // 前回作ったジオメトリ収集関数（privateにしてもOKです）
    static NavMeshBuildData GatherGeometry(CCL::ECS::Core::World& world);

    // ★今回追加するBake本体関数
    static bool BuildNavMesh(CCL::ECS::Core::World& world, const NavMeshBuildSettings& settings);
};