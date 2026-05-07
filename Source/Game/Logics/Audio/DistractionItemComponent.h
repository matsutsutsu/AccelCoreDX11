#pragma once
#include <cstdint>

// ===================================================================================
// 【 音源誘導アイテム : DistractionItemComponent 】
// [ 役割 ] 物理的にぶつかった際に、AIの気を引く音（AISoundEvent）を発生させるフラグデータ。
// ===================================================================================
struct DistractionItemComponent {
    float volumeRadius = 15.0f; // AIの耳に届く半径 (m)
    bool  hasTriggered = false; // すでに音を鳴らしたか（バウンドする度に何度も鳴るのを防ぐ）

    // 実際にプレイヤーの耳(FMOD)に鳴らす音のハッシュ値
    uint32_t fmodEventHash = 0;

    // これ以上の速度(m/s)でぶつからないと音は鳴らない
    float bounceVelocityThreshold = 2.0f;
};