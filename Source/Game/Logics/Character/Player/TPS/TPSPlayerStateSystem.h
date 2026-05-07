#pragma once
#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Core/Time/TimeState.h"

// 前方宣言
struct TPSPlayerComponent;
struct TPSPlayerStateComponent;
struct StaminaComponent;
struct AnimParametersComponent;


class TPSPlayerStateSystem
    : public CCL::ECS::IfSystem<TPSPlayerStateSystem,
    CCL::ECS::Write<TPSPlayerComponent>,
    CCL::ECS::Write<TPSPlayerStateComponent>,
    CCL::ECS::Write<AnimParametersComponent>,
    CCL::ECS::Write<StaminaComponent>,
    CCL::ECS::Read<TimeState>>
{
public:
    // コンストラクタを明示的に定義
    TPSPlayerStateSystem() : IfSystem("TPSPlayerStateSystem") {}

    void Update(float dt) override;
};
