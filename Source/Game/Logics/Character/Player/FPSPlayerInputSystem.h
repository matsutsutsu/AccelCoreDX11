#pragma once
#include "ECS/System/CCL_System.h"

// 前方宣言
class FPSPlayerComponent;

/**
 * @brief プレイヤーの入力を監視し、FPSPlayerComponent::input に値をセットするシステム
 */
class FPSPlayerInputSystem
    : public CCL::ECS::IfSystem<FPSPlayerInputSystem,
    CCL::ECS::Write<FPSPlayerComponent>> // 入力値を書き込むため Write
{
public:
    FPSPlayerInputSystem() : IfSystem("FPSPlayerInputSystem") {}

    //毎フレーム更新：InputFacade(IInputAPI)から値を取得しコンポーネントへ反映
    void Update(float dt) override;
};