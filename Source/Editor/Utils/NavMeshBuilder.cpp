#include "NavMeshBuilder.h"
#include "ECS/Core/CCL_World.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "Engine/GamePlay/Graphics/Core/ModelComponent.h"
#include "Engine/Graphics/Resource/Model.h"
#include "Engine/Platform/Logger.h"

#include "Engine/GamePlay/AI/Navigation/NavigationData.h"

// Recast & Detour ライブラリのインクルード
#include <Recast.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>

NavMeshBuildData NavMeshBuilder::GatherGeometry(CCL::ECS::Core::World& world) {
    NavMeshBuildData data;
    int vertexOffset = 0; // 複数のモデル・メッシュを繋ぎ合わせるための頂点オフセット

    // あなたのECSの機能を使って、TransformとModelを持つEntityIDのリストを取得
    std::vector<CCL::ECS::EntityID> entities = world.View<TransformComponent, ModelComponent>();

    for (CCL::ECS::EntityID entity : entities) {
        // Worldからコンポーネントのポインタを取得
        TransformComponent* transform = world.GetComponent<TransformComponent>(entity);
        ModelComponent* modelComp = world.GetComponent<ModelComponent>(entity);

        // 安全のためのnullチェック
        if (!transform || !modelComp) continue;

        // ★アーキテクチャの要：動くもの（キャラクター等）はNavMeshに焼き付けない
        if (!transform->isStatic) {
            continue;
        }

        Model* model = modelComp->GetModel();
        if (!model) continue;

        const ModelResource* resource = model->GetResource();
        if (!resource) continue;

        // 1. ワールド行列の読み込み (DirectXMath型へ変換)
        DirectX::XMMATRIX worldMatrix = DirectX::XMLoadFloat4x4(&transform->worldMatrix);

        // 2. ModelResource内の全てのMeshを走査する
        for (const auto& mesh : resource->GetMeshes()) {

            // 2-A. 頂点の抽出とワールド空間への変換
            for (const auto& v : mesh.vertices) {
                // ローカル座標をベクトルに
                DirectX::XMVECTOR localPos = DirectX::XMVectorSet(v.position.x, v.position.y, v.position.z, 1.0f);

                // 行列を掛けてワールド座標へ変換
                DirectX::XMVECTOR worldPos = DirectX::XMVector3TransformCoord(localPos, worldMatrix);

                DirectX::XMFLOAT3 wp;
                DirectX::XMStoreFloat3(&wp, worldPos);

                // Recast用にフラットなfloat配列に詰め込む
                data.vertices.push_back(wp.x);
                data.vertices.push_back(wp.y);
                data.vertices.push_back(wp.z);
            }

            // 2-B. インデックスデータの抽出
            for (UINT idx : mesh.indices) {
                // ここが超重要！ 別のモデル/メッシュの頂点を参照しないように、
                // これまで追加した全頂点数（オフセット）をインデックスに足し込む
                data.indices.push_back(static_cast<int>(idx) + vertexOffset);
            }

            // 次のメッシュの処理に向けて、今回追加した頂点数をオフセットに加算
            vertexOffset += static_cast<int>(mesh.vertices.size());
        }
    }

    return data;
}


bool NavMeshBuilder::BuildNavMesh(CCL::ECS::Core::World& world, const NavMeshBuildSettings& settings) {
    // 1. ジオメトリの収集
    NavMeshBuildData geom = GatherGeometry(world);
    int nverts = geom.vertices.size() / 3;
    int ntris = geom.indices.size() / 3;

    if (nverts == 0 || ntris == 0) {
        CCL_LOG_ERROR(LogCategory::Core,"NavMesh Build Failed: No static geometry found.");
        return false;
    }

    // Recastの処理コンテキスト（エラーログ等を出力するためのクラス）
    rcContext ctx;

    // 2. バウンディングボックス（地形全体の最小・最大座標）の計算
    float bmin[3], bmax[3];
    rcCalcBounds(geom.vertices.data(), nverts, bmin, bmax);

    // 3. Config (rcConfig) のセットアップ
    // 人間の指定した「メートル」単位の数値を、Recastの「ボクセル（セル）数」に変換します。
    rcConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.cs = settings.cellSize;
    cfg.ch = settings.cellHeight;
    cfg.walkableSlopeAngle = settings.agentMaxSlope;
    cfg.walkableHeight = (int)ceilf(settings.agentHeight / cfg.ch);
    cfg.walkableClimb = (int)floorf(settings.agentMaxClimb / cfg.ch);
    cfg.walkableRadius = (int)ceilf(settings.agentRadius / cfg.cs);
    cfg.maxEdgeLen = (int)(settings.edgeMaxLen / settings.cellSize);
    cfg.maxSimplificationError = settings.edgeMaxError;
    cfg.minRegionArea = (int)rcSqr(settings.regionMinSize);
    cfg.mergeRegionArea = (int)rcSqr(settings.regionMergeSize);
    cfg.maxVertsPerPoly = (int)settings.vertsPerPoly;
    cfg.detailSampleDist = settings.detailSampleDist < 0.9f ? 0 : settings.cellSize * settings.detailSampleDist;
    cfg.detailSampleMaxError = settings.cellHeight * settings.detailSampleMaxError;
    rcVcopy(cfg.bmin, bmin);
    rcVcopy(cfg.bmax, bmax);

    // グリッドサイズ（X軸とZ軸のボクセル総数）の計算
    rcCalcGridSize(cfg.bmin, cfg.bmax, cfg.cs, &cfg.width, &cfg.height);

    CCL_LOG_INFO(LogCategory::Core, "NavMesh Voxel Grid Size: %d x %d", cfg.width, cfg.height);

    // 4. ハイトフィールド（ボクセル空間）のメモリ確保
    rcHeightfield* solid = rcAllocHeightfield();
    if (!solid || !rcCreateHeightfield(&ctx, *solid, cfg.width, cfg.height, cfg.bmin, cfg.bmax, cfg.cs, cfg.ch)) {
        CCL_LOG_ERROR(LogCategory::Core, "NavMesh Build Failed: Could not create solid heightfield.");
        return false;
    }

    // 5. 三角形ポリゴンの歩行可能判定とボクセルへのラスタライズ（焼き込み）
    // 法線角度から「傾斜がキツすぎる三角形」を弾き、平面の三角形だけをボクセル空間に刻み込みます。
    std::vector<unsigned char> triAreas(ntris, 0);
    rcMarkWalkableTriangles(&ctx, cfg.walkableSlopeAngle, geom.vertices.data(), nverts, geom.indices.data(), ntris, triAreas.data());

    if (!rcRasterizeTriangles(&ctx, geom.vertices.data(), nverts, geom.indices.data(), triAreas.data(), ntris, *solid, cfg.walkableClimb)) {
        CCL_LOG_ERROR(LogCategory::Core, "NavMesh Build Failed: Could not rasterize triangles.");
        rcFreeHeightField(solid);
        return false;
    }

    // 6. フィルタリング（通れない場所の削除）
    // 天井が低すぎる場所（机の下など）や、崖っぷちのボクセルを「歩行不可」として削り落とします。
    rcFilterLowHangingWalkableObstacles(&ctx, cfg.walkableClimb, *solid);
    rcFilterLedgeSpans(&ctx, cfg.walkableHeight, cfg.walkableClimb, *solid);
    rcFilterWalkableLowHeightSpans(&ctx, cfg.walkableHeight, *solid);

    CCL_LOG_INFO(LogCategory::Core, "NavMesh Rasterization Success!");

    // ---------------------------------------------------------
    // 【Step 3】領域分割とPolyMeshの構築
    // ---------------------------------------------------------

    // 7. コンパクト・ハイトフィールド（CHD）への圧縮
    // 以降の処理を高速化するため、ボクセル空間をよりメモリ効率の良い構造体に変換します。
    rcCompactHeightfield* chf = rcAllocCompactHeightfield();
    if (!chf || !rcBuildCompactHeightfield(&ctx, cfg.walkableHeight, cfg.walkableClimb, *solid, *chf)) {
        CCL_LOG_ERROR(LogCategory::Core, "NavMesh Build Failed: Could not build compact heightfield.");
        return false;
    }
    // 古い重いボクセルデータはここで解放（メモリ節約）
    rcFreeHeightField(solid);

    // 8. 歩行可能エリアの「浸食（Erosion）」
    // 壁からAIの半径（walkableRadius）分だけ歩ける領域を削り落とします。
    if (!rcErodeWalkableArea(&ctx, cfg.walkableRadius, *chf)) {
        CCL_LOG_ERROR(LogCategory::Core, "NavMesh Build Failed: Could not erode walkable area.");
        return false;
    }

    // 9. 領域（Region）の分割
    // 広大な空間を「小さな島」の集まりに分割します（Watershed法を使用）。
    if (!rcBuildDistanceField(&ctx, *chf)) {
        CCL_LOG_ERROR(LogCategory::Core, "NavMesh Build Failed: Could not build distance field.");
        return false;
    }
    if (!rcBuildRegions(&ctx, *chf, 0, cfg.minRegionArea, cfg.mergeRegionArea)) {
        CCL_LOG_ERROR(LogCategory::Core, "NavMesh Build Failed: Could not build regions.");
        return false;
    }

    // 10. 輪郭（Contour）の抽出
    // ボクセルの境界をなぞり、ベクターデータの輪郭線を生成します。
    rcContourSet* cset = rcAllocContourSet();
    if (!cset || !rcBuildContours(&ctx, *chf, cfg.maxSimplificationError, cfg.maxEdgeLen, *cset)) {
        CCL_LOG_ERROR(LogCategory::Core, "NavMesh Build Failed: Could not build contours.");
        return false;
    }

    // 11. ポリゴンメッシュ（PolyMesh）の構築
    // 輪郭線から、A*探索のノードとなる凸ポリゴンを生成します。
    rcPolyMesh* pmesh = rcAllocPolyMesh();
    if (!pmesh || !rcBuildPolyMesh(&ctx, *cset, cfg.maxVertsPerPoly, *pmesh)) {
        CCL_LOG_ERROR(LogCategory::Core, "NavMesh Build Failed: Could not build poly mesh.");
        return false;
    }

    // ポリゴンに「歩行可能」のフラグを立てる
    for (int i = 0; i < pmesh->npolys; ++i) {
        if (pmesh->areas[i] == RC_WALKABLE_AREA) {
            pmesh->areas[i] = 0; // Detourのデフォルトエリア（0）として扱う
            pmesh->flags[i] = 1; // ★これが超重要！ 1 (0x01) を設定しないとフィルターで無視される
        }
    }

    // 12. 詳細メッシュ（Detail Mesh）の構築
    // Y軸（高さ）の精度を担保するため、ポリゴンの表面に地形の起伏を焼き付けます。
    // （ランタイムでAIが地面にぴったり足を這わせるために必要です）
    rcPolyMeshDetail* dmesh = rcAllocPolyMeshDetail();
    if (!dmesh || !rcBuildPolyMeshDetail(&ctx, *pmesh, *chf, cfg.detailSampleDist, cfg.detailSampleMaxError, *dmesh)) {
        CCL_LOG_ERROR(LogCategory::Core, "NavMesh Build Failed: Could not build detail mesh.");
        return false;
    }

    // 生成されたポリゴン数をログ出力
    CCL_LOG_INFO(LogCategory::Core, "NavMesh PolyMesh Generated: %d Polygons", pmesh->npolys);

    // ---------------------------------------------------------
    // 【Step 4】Detourナビゲーション・バイナリのコンパイル
    // ---------------------------------------------------------
    // pmesh (ポリゴン) と dmesh (詳細地形) を統合し、
    // AIが最速でA*探索できるメモリ構造 (Detourデータ) を作ります。

    dtNavMeshCreateParams params;
    memset(&params, 0, sizeof(params));

    // 頂点とポリゴン情報
    params.verts = pmesh->verts;
    params.vertCount = pmesh->nverts;
    params.polys = pmesh->polys;
    params.polyAreas = pmesh->areas;
    params.polyFlags = pmesh->flags;
    params.polyCount = pmesh->npolys;
    params.nvp = pmesh->nvp;

    // 詳細メッシュ（AIが地面に足を這わせるためのY軸の起伏データ）
    params.detailMeshes = dmesh->meshes;
    params.detailVerts = dmesh->verts;
    params.detailVertsCount = dmesh->nverts;
    params.detailTris = dmesh->tris;
    params.detailTriCount = dmesh->ntris;

    // エージェント（AI）のサイズ情報
    params.walkableHeight = settings.agentHeight;
    params.walkableRadius = settings.agentRadius;
    params.walkableClimb = settings.agentMaxClimb;
    rcVcopy(params.bmin, pmesh->bmin);
    rcVcopy(params.bmax, pmesh->bmax);
    params.cs = cfg.cs;
    params.ch = cfg.ch;
    params.buildBvTree = true; // 経路探索を爆速にするためのBVHツリーを構築

    unsigned char* navData = 0;
    int navDataSize = 0;

    // 最終コンパイル実行
    if (!dtCreateNavMeshData(&params, &navData, &navDataSize)) {
        CCL_LOG_ERROR(LogCategory::Core, "NavMesh Build Failed: Could not build Detour navmesh.");

        // エラーで抜ける前に、必ず一時メモリを解放する
        rcFreeCompactHeightfield(chf);
        rcFreeContourSet(cset);
        rcFreePolyMesh(pmesh);
        rcFreePolyMeshDetail(dmesh);

        return false;
    }

    CCL_LOG_INFO(LogCategory::Core, "Detour NavMesh Binary Created: %d bytes", navDataSize);

    // ---------------------------------------------------------
    // メモリのクリーンアップ（一時データ群）
    // ---------------------------------------------------------
    rcFreeCompactHeightfield(chf);
    rcFreeContourSet(cset);
    rcFreePolyMesh(pmesh);
    rcFreePolyMeshDetail(dmesh);

    // ★重要: この navData (と navDataSize) が、ファイルにセーブすべき「完成品」です。
    // 今はテストのため、そのままランタイム用の dtNavMesh を作ってみます。

    dtNavMesh* runtimeNavMesh = dtAllocNavMesh();
    // DT_ALLOC_PERM を指定すると、runtimeNavMesh が navData のメモリ所有権を持ちます
    dtStatus status = runtimeNavMesh->init(navData, navDataSize, DT_ALLOC_PERM);

    if (dtStatusFailed(status)) {
        CCL_LOG_ERROR(LogCategory::Core, "Failed to init Detour NavMesh");
        dtFree(navData);
        return false;
    }

    // あなたのECSの強力なResource機能を使ってWorldに登録する！
    if (!world.HasResource<NavigationData>()) {
        world.AddResource<NavigationData>();
    }
    else {
        // 既にデータが存在する（2回目以降のBake）なら、古いメモリを必ず解放する
        auto& oldData = world.GetResource<NavigationData>();
        if (oldData.navQuery) {
            dtFreeNavMeshQuery(oldData.navQuery);
            oldData.navQuery = nullptr;
        }
        if (oldData.navMesh) {
            dtFreeNavMesh(oldData.navMesh);
            oldData.navMesh = nullptr;
        }
    }

    // 生成したNavMeshをシステム全体で共有
    world.GetResource<NavigationData>().navMesh = runtimeNavMesh;

    // ---------------------------------------------------------
    // 経路探索エンジン(NavMeshQuery)の初期化
    // ---------------------------------------------------------
    dtNavMeshQuery* runtimeNavQuery = dtAllocNavMeshQuery();
    // 2048 は探索時の最大ノード数（A*のオープンリスト/クローズドリストのサイズ）
    // 数千体の群集AIを動かす場合でも、一般的にはこれで十分です。
    dtStatus queryStatus = runtimeNavQuery->init(runtimeNavMesh, 2048);

    if (dtStatusFailed(queryStatus)) {
        CCL_LOG_ERROR(LogCategory::Core, "Failed to init Detour NavMeshQuery");
        dtFreeNavMeshQuery(runtimeNavQuery);
        return false;
    }

    // Queryもシステム全体で共有
    world.GetResource<NavigationData>().navQuery = runtimeNavQuery;

    return true;

}