#pragma once
#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "Engine/GamePlay/Graphics/Core/ModelComponent.h"


// モデルのノード行列やボーン行列を描画前に確定させるシステム
class ModelPoseUpdateSystem : public CCL::ECS::IfSystem<ModelPoseUpdateSystem,
    CCL::ECS::Read<TransformComponent>,
    CCL::ECS::Write<ModelComponent>> {
public:
    ModelPoseUpdateSystem() : IfSystem("ModelPoseUpdateSystem") {}

    void Update(float dt) override;
};
