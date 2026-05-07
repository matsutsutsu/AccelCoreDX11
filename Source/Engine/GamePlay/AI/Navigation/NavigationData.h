#pragma once
#include <DetourNavMesh.h>
#include <DetourAlloc.h>
#include <DetourNavMeshQuery.h>


// Worldに1つだけ存在するナビゲーション用の共有データ
struct NavigationData {
    dtNavMesh* navMesh = nullptr;
    // 次のステップで使う「経路探索用エンジン」もここに追加しておきます
    dtNavMeshQuery* navQuery = nullptr;

    bool showDebugDraw = true; // デバッグ描画のON/OFF用

    // デストラクタでメモリを完全に解放する
    ~NavigationData() {
        if (navQuery) {
            dtFreeNavMeshQuery(navQuery);
            navQuery = nullptr;
        }
        if (navMesh) {
            dtFreeNavMesh(navMesh);
            navMesh = nullptr;
        }
    }
};