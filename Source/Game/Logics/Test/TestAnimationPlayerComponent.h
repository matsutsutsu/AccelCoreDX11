#pragma once

// 物理システムを無視して直接座標を動かすためのテスト用コンポーネント
struct TestAnimationPlayerComponent {
    float moveSpeed = 15.0f; // テスト用の移動速度
    float turnSpeed = 10.0f; // 振り向きの滑らかさ
};
