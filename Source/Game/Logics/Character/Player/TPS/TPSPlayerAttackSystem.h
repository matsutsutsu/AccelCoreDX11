#pragma once
#include "ECS/System/CCL_System.h"
#include "../PlayerStateComponent.h"

struct TPSPlayerComponent;
struct TransformComponent;
class AnimParametersComponent;
class AnimatorComponent;
class BlackboardComponent;

class TPSPlayerAttackSystem : public CCL::ECS::IfSystem<TPSPlayerAttackSystem,
    CCL::ECS::Write<TransformComponent>,
    CCL::ECS::Write<TPSPlayerComponent>,
    CCL::ECS::Write<TPSPlayerStateComponent>,
    CCL::ECS::Write<AnimParametersComponent>,
    CCL::ECS::Write<BlackboardComponent>,
    CCL::ECS::Read<AnimatorComponent>,
    CCL::ECS::Read<PlayerStateTag::IsAttackingTag>>
{
public:
    TPSPlayerAttackSystem() : IfSystem("TPSPlayerAttackSystem") {}
    void Update(float dt) override;
};