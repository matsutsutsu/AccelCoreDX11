#pragma once
#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "DistractionItemComponent.h" // パスは適宜合わせてください

// ===================================================================================
// 【 音源アイテム可視化システム : DistractionItemDebugDrawSystem 】
// [ 役割 ]
// 音を鳴らすアイテム（空き缶など）を中心に、「音がどこまで届くか」の球体を描画する。
// プランナーがエディタ上で volumeRadius を調整した際、リアルタイムに大きさが変化する。
// ===================================================================================
class DistractionItemDebugDrawSystem : public CCL::ECS::IfSystem<DistractionItemDebugDrawSystem,
    CCL::ECS::Read<TransformComponent>,
    CCL::ECS::Read<DistractionItemComponent>> {
public:
    DistractionItemDebugDrawSystem() : IfSystem("DistractionItemDebugDrawSystem") {
        this->isDebugVisible = true; // デフォルトで表示ON
    }
    virtual ~DistractionItemDebugDrawSystem() = default;

    void Update(float dt) override;
};