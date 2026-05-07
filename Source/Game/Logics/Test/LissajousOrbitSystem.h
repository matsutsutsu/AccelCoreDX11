#pragma once
#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "LissajousOrbitComponent.h" // ※Componentの構造体定義ヘッダー
#include <cmath>

class LissajousOrbitSystem : public CCL::ECS::IfSystem<LissajousOrbitSystem,
    CCL::ECS::Write<TransformComponent>,
    CCL::ECS::Write<LissajousOrbitComponent>>
{
public:
    LissajousOrbitSystem() : IfSystem("LissajousOrbitSystem") {}

    void Update(float dt) override;
};