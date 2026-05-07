#pragma once

#include "ECS/Common/CCL_Common.h" // CCL::ECS::EntityID を使うため
#include <DirectXMath.h>           // DirectX::XMFLOAT3 を使うため

// GameEvents.h
struct EntityKilledEvent {
    CCL::ECS::EntityID killedEntityID;
    DirectX::XMFLOAT3 position;
    bool isPlayer;
    int droppedCoinAmount; // ★ 死亡時に落とす金額をイベントに持たせる
};

// 誰かがダメージを受けた時に発行されるイベント
struct DamageTakenEvent {
    CCL::ECS::EntityID targetID;     // ダメージを受けた人
    float              damageAmount; // 受けたダメージ量
    bool               isPlayer;     // プレイヤーかどうか（受け取り側が判定しやすくする便利フラグ）
};