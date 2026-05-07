#pragma once
#include "ECS/System/CCL_System.h"

class NavMeshDebugDrawSystem : public CCL::ECS::IfSystem<NavMeshDebugDrawSystem> {
public:
    
    NavMeshDebugDrawSystem() : IfSystem("NavMeshDebugDrawSystem") {
        // システム生成時に最初からデバッグ描画をONにする
        this->isDebugVisible = true;
    }

    void Update(float dt) override;
};