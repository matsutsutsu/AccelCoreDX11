#pragma once
#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "ZeroGDebrisComponent.h"

/**
 * @brief ボスアリーナ周囲の瓦礫を、数式（サイン波）を用いて無重力的に浮遊させるシステム
 * @note 物理エンジンを一切介さず、純粋な算術演算のみでTransformを上書きするため、数万個のオブジェクトでも極めて軽量に動作する。
 */
class ZeroGDebrisSystem : public CCL::ECS::IfSystem<ZeroGDebrisSystem,
    CCL::ECS::Write<TransformComponent>,
    CCL::ECS::Write<ZeroGDebrisComponent>>
{
public:
    ZeroGDebrisSystem() : IfSystem("ZeroGDebrisSystem") {}
    virtual ~ZeroGDebrisSystem() = default;

    void Update(float dt) override;
};