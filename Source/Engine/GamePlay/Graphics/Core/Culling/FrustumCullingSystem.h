#pragma once
#include "ECS/System/CCL_System.h"
#include "CullingComponents.h"

// AABBとカメラの視錐台を判定し、可視フラグを切り替えるシステム
class FrustumCullingSystem : public CCL::ECS::IfSystem<FrustumCullingSystem,
    CCL::ECS::Read<BoundingBoxComponent>,
    CCL::ECS::Write<VisibilityComponent>>
{
public:
    FrustumCullingSystem();
    virtual ~FrustumCullingSystem() = default;

    void Update(float dt) override;
};