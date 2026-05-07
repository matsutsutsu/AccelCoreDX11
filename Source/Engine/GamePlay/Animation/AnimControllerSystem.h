#pragma once
#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Animation/Data/AnimStateMachine.h"
#include "Engine/GamePlay/Animation/AnimParametersComponent.h"
#include "Engine/GamePlay/Animation/AnimatorComponent.h"

// ===================================================================================
// ファイル: AnimControllerSystem.h
// 概要: アニメーションの状態遷移を決定する「評価器（舞台監督）」システム
// 
// [ 役割 ]
// 毎フレーム、AnimParametersComponent（掲示板）の値を読み取り、
// AnimStateMachine（ルールブック）の遷移条件を満たしているかをチェックする。
// 条件を満たした場合、現在の状態を書き換え、
// AnimatorComponent に対して新しいアニメーションの再生を命令する。
// ※物理計算や描画は一切行わない純粋なステートマシン評価器。
// ===================================================================================

class AnimControllerSystem : public CCL::ECS::IfSystem<AnimControllerSystem,
    CCL::ECS::Write<AnimStateMachineComponent>,
    CCL::ECS::Write<AnimatorComponent>,
    CCL::ECS::Write<AnimParametersComponent>>
{
public:
    // 基底クラスのコンストラクタ呼び出しもフルパスで指定
    AnimControllerSystem() : IfSystem("AnimControllerSystem") {}
    virtual ~AnimControllerSystem() = default;

    void Update(float dt) override;
};