#pragma once
#include <DirectXMath.h>
#include <vector>

// ===================================================================================
// 【 ナビゲーションデータ : NavAgentComponent 】
//
// [ 役割 ]
// キャラクターの「目的地」「経路」「希望速度」を保持する。
// 脳（AI）と神経（Navigation）と肉体（Physics）を繋ぐための共通インターフェース。
//
// [ 設計思想 ]
// 連続したメモリに配置されるPOD（Plain Old Data）として設計。
// 物理計算やAIの具体的なステートは持たず、移動に必要な「純粋な数値」のみを管理する。
// ===================================================================================

struct NavAgentComponent {
    // 目的地
    DirectX::XMFLOAT3 targetPosition = { 0.0f, 0.0f, 0.0f };

    // String Pulling で計算された最短経路の頂点リスト（最大16〜32個程度で十分）
    std::vector<DirectX::XMFLOAT3> waypoints;

    // 現在向かっているウェイポイントのインデックス
    int currentWaypointIndex = 0;

    // 経路探索を要求したかどうかのフラグ
    bool isPathRequested = false;

    // 外部の脳（AIやスクリプト）が指定する「現在の目標移動速度」
    float currentDesiredSpeed = 0.0f;

    float stopDistance = 0.5f;
};