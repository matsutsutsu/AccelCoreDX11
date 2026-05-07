// PlayerComponent.h
#pragma once
#include <DirectXMath.h>

struct PlayerComponent {
    // --- 抽象化された入力データ (Input State) ---
    struct InputData {
        DirectX::XMFLOAT3 moveDir = { 0, 0, 0 };// 移動の意思 (-1.0 ~ 1.0)
        DirectX::XMFLOAT3 lookPos = {0, 0, 0};  // 見るべき場所（ワールド座標）
        bool jump = false;                   // ジャンプの意思
        bool attack = false;                 // 攻撃の意思
    } input;

    // --- プレイヤー固有のパラメータ ---
    float moveSpeed = 30.0f;
    float turnSpeed = 10.0f;
    int padSlot = 0; // どのコントローラーで操作するか (マルチプレイ拡張用)
};