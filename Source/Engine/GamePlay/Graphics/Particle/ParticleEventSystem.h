#pragma once
#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Animation/Data/AnimEventMessages.h" // 前回作った手紙の定義

class ParticleEventSystem : public CCL::ECS::IfSystem<ParticleEventSystem> {
public:
    ParticleEventSystem() : IfSystem("ParticleEventSystem") {}

    void Initialize() override;
    //void Finalize() override;
    void Update(float dt) override;

private:
    // エフェクト再生の電波を受信したときに呼ばれる関数
    void OnEffectEvent(const AnimEventEffectMessage& msg);
};