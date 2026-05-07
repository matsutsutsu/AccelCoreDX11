#pragma once

#include "ECS/System/CCL_System.h"

// 前方宣言
class TPSPlayerComponent;
class AnimParametersComponent;

/**
 * プレイヤーの入力を監視し、TPSPlayerComponent::input に値をセットするシステム
 */
class TPSPlayerInputSystem
    : public CCL::ECS::IfSystem<TPSPlayerInputSystem,
    CCL::ECS::Write<TPSPlayerComponent>, // 入力値を書き込むため Write
    CCL::ECS::Write<AnimParametersComponent>> // 入力値を書き込むため Write
{
public:
    TPSPlayerInputSystem() : IfSystem("TPSPlayerInputSystem") {}

    //毎フレーム更新：InputFacade(IInputAPI)から値を取得しコンポーネントへ反映
    void Update(float dt) override;
};