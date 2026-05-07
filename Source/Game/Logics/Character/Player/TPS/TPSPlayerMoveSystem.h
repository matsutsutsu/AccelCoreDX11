#pragma once

#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Core/Time/TimeState.h"

// 前方宣言
struct TPSPlayerComponent;
struct TPSPlayerStateComponent;
struct CharacterMovementInputComponent;
struct TransformComponent;
struct StaminaComponent;
struct ModifierStatusComponent;
class AnimParametersComponent;
/**
 * TPSプレイヤーの移動ロジックを司るシステム
 */
class TPSPlayerMoveSystem
    : public CCL::ECS::IfSystem<TPSPlayerMoveSystem,
    CCL::ECS::Write<TransformComponent>,
    CCL::ECS::Write<TPSPlayerComponent>,             // 入力データ
    CCL::ECS::Write<TPSPlayerStateComponent>,       // 内部ステート(timer, speed)
    CCL::ECS::Write<StaminaComponent>,
    CCL::ECS::Write<AnimParametersComponent>,
    CCL::ECS::Write<CharacterMovementInputComponent>,
    CCL::ECS::Read<ModifierStatusComponent>,
    CCL::ECS::Read<TimeState>> // ★追加
{
public:
    // コンストラクタで基底クラスにシステム名を渡す（これが無いとエラーになります）
    TPSPlayerMoveSystem() : IfSystem("TPSPlayerMoveSystem") {}

    void Update(float dt) override;
};