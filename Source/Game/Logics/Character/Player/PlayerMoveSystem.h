#pragma once
#include "ECS/System/CCL_System.h" 

// 必要なコンポーネントだけを残す
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "Game/Logics/Character/Player/PlayerComponent.h"
#include "Game/Logics/Character/CharacterMovementInputComponent.h"

// ===================================================================================
// 【 プレイヤー移動思考システム : PlayerMoveSystem 】
// [ 役割 ] Playerの入力状態を読み取り、移動したいベクトルを計算して InputComponent に書き込む。
// 物理計算やアニメーションは行わない純粋な「脳」のシステム。
// ===================================================================================
class PlayerMoveSystem : public CCL::ECS::IfSystem<PlayerMoveSystem,
    CCL::ECS::Write<TransformComponent>, // 回転させるためにWrite
    CCL::ECS::Read<PlayerComponent>,     // プレイヤーの入力状態とステータス(速度)
    CCL::ECS::Write<CharacterMovementInputComponent>> { // アクセルペダル
public:
    PlayerMoveSystem() : IfSystem("PlayerMoveSystem") {}
    virtual ~PlayerMoveSystem() = default;

    void Update(float dt) override;
};