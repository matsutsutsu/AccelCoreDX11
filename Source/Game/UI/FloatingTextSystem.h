#pragma once
#include "ECS/System/CCL_System.h"
#include "FloatingTextComponent.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "Engine/UI/Text/TextManager.h"

class FloatingTextSystem
    : public CCL::ECS::IfSystem<FloatingTextSystem, 
    CCL::ECS::Write<FloatingTextComponent>,
                               CCL::ECS::Write<TransformComponent>> {
  public:
    // TextManagerへの参照を受け取る
    //FloatingTextSystem(TextManager &textManager)
    //    : IfSystem("FloatingTextSystem"), _textManager(textManager)
    //{
    //}

    FloatingTextSystem()
        : IfSystem("FloatingTextSystem")
    {
    }

    void Update(float dt) override;

  private:
    //TextManager &_textManager;
};