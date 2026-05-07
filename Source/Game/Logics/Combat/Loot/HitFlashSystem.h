#pragma once
#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Graphics/Core/MaterialComponent.h"
#include "HitFlashComponent.h"

// ダメージを受けたエンティティを一瞬だけ白く光らせるシステム
class HitFlashSystem : public CCL::ECS::IfSystem<HitFlashSystem,
                           CCL::ECS::Write<HitFlashComponent>,
                           CCL::ECS::Write<MaterialComponent>> {
  public:
    HitFlashSystem();
    virtual ~HitFlashSystem() = default;

    void Initialize() override;
    void Update(float dt) override;
};