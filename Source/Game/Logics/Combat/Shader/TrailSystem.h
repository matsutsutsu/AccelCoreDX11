#pragma once
#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "Game/Logics/Combat/Shader/TrailComponent.h" // パスは環境に合わせてください

class TrailSystem : public CCL::ECS::IfSystem<TrailSystem,
    CCL::ECS::Write<TrailComponent>,
    CCL::ECS::Read<TransformComponent>>
{
public:
    TrailSystem();
    virtual ~TrailSystem() = default;

    void Update(float dt) override;
};