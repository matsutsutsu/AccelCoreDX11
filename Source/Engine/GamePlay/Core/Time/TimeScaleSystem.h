#pragma once
#include "ECS/System/CCL_System.h"
#include "TimeState.h"

// Write<TimeState> を宣言。これによりTaskSchedulerが依存関係を正確に把握する
class TimeScaleSystem : public CCL::ECS::IfSystem<TimeScaleSystem, CCL::ECS::Write<TimeState>>
{
public:
    TimeScaleSystem();

    void Initialize() override;

    // 実際の時間の計算処理
    void Update(float rawDt) override;
};