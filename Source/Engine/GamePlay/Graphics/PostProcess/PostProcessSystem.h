#pragma once
#include "ECS/System/CCL_System.h"

#include "Engine/GamePlay/Graphics/PostProcess/BloomConfigComponent.h"
#include "Engine/GamePlay/Graphics/PostProcess/ToneMapConfigComponent.h"

// 前方宣言
class PostProcessManager;

class PostProcessSystem : public CCL::ECS::SystemBase {
public:
    // マネージャーを受け取るコンストラクタ
    PostProcessSystem()
        : SystemBase("PostProcessSystem")
    {
    }

    // どのデータにアクセスするかをスケジューラに自己申告する
    std::vector<CCL::ECS::TypeID> GetReadTypes() const override
    {
        return {CCL::ECS::TypeInfo<BloomConfigComponent>::ID(),
            CCL::ECS::TypeInfo<ToneMapConfigComponent>::ID()};
    }

    void Update(float dt) override;

};