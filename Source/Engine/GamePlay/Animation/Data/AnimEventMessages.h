#pragma once
#include "ECS/Common/CCL_Common.h"

// ===================================================================================
// ファイル: AnimEventMessages.h
// 概要: アニメーションから他システムへ送る「通知（イベント）メッセージ」
// 
// [ 役割 ]
// AnimationSystem 内で台本の指定時間に到達した際、EventBus を介して
// サウンドシステムやコリジョン生成システム等に処理を依頼するためのデータ構造。
// これにより、アニメーションと他システムを密結合させずに連携できる。
// ===================================================================================

// 1. 戦闘システム（CombatSystem）宛ての手紙
struct AnimEventHitBoxMessage {
    CCL::ECS::EntityID entity;
    bool isActive; // true なら判定ON、false なら判定OFF
};

// 2. 音響システム（AudioSystem）宛ての手紙
struct AnimEventSoundMessage {
    CCL::ECS::EntityID entity;
    // ※将来的に「どの音を鳴らすか」のIDなどをここに追加します
};

// 3. エフェクトシステム（ParticleSystem）宛ての手紙
struct AnimEventEffectMessage {
    CCL::ECS::EntityID entity;
    std::string effectPrefabPath; // 召喚すべきエフェクトの設計図
};