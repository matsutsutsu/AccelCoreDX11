#pragma once
#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Graphics/Core/MaterialComponent.h"
#include "DissolveComponent.h"

// ディゾルブ演出の進行と、完了時のエンティティ破棄を専任で行うシステム
class DissolveSystem : public CCL::ECS::IfSystem<DissolveSystem,
                           CCL::ECS::Write<DissolveComponent>,
                           CCL::ECS::Write<MaterialComponent>> {
  public:
    DissolveSystem();
    virtual ~DissolveSystem() = default;

    void Update(float dt) override;
};