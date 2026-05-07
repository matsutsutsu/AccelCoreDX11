// --- JoltCleanupSystem.h ---
#pragma once
#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Core/DestroyTag.h"
#include "Engine/GamePlay/Physics/Jolt/JoltHandleComponent.h"

class JoltCleanupSystem : public CCL::ECS::IfSystem<JoltCleanupSystem,
    CCL::ECS::Read<Tag::DestroyTag>, CCL::ECS::Read<JoltHandleComponent>> {
public:
    JoltCleanupSystem() : IfSystem("JoltCleanupSystem") {}
    void Update(float dt) override;

    // 外部から強制的にクリーンアップを走らせる静的関数
    static void ExecuteCleanup(CCL::ECS::Core::World& world);
};

