#pragma once
#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "JoltCharacterConfigComponent.h"

// プレイヤー専用のコントローラーを生成するファクトリーシステム
class JoltCharacterSetupSystem : public CCL::ECS::IfSystem<JoltCharacterSetupSystem,
                                     CCL::ECS::Read<TransformComponent>,
                                     CCL::ECS::Read<JoltCharacterConfigComponent>> {
  public:
    JoltCharacterSetupSystem() : IfSystem("JoltCharacterSetupSystem") {}
    virtual ~JoltCharacterSetupSystem() = default;

    void Update(float dt) override;
};