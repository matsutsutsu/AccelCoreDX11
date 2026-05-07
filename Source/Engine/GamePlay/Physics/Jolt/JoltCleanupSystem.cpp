// --- JoltCleanupSystem.cpp ---
#include "JoltCleanupSystem.h"
#include "Engine/Physics/JoltPhysicsManager.h"
#include "ECS/Core/CCL_World.h"

#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

void JoltCleanupSystem::Update(float dt) {
    ExecuteCleanup(*_world);
}

// 直接チャンクを回して確実に剛体を消去する関数
void JoltCleanupSystem::ExecuteCleanup(CCL::ECS::Core::World& world)
{
    bool hasJolt = world.HasResource<JoltPhysicsManager>();
    JoltPhysicsManager* joltManager = hasJolt ? &world.GetResource<JoltPhysicsManager>() : nullptr;
    if (!joltManager) return;

    JPH::BodyInterface& bodyInterface = joltManager->GetBodyInterface();

    auto& chunks = world.GetChunkManager().GetChunks();
    for (auto& chunkPtr : chunks) {
        if (!chunkPtr) continue;
        auto* chunk = chunkPtr.get();

        // Jolt剛体を持っていて、かつDestroyTagがついているものを狙い撃ちする
        if (!chunk->HasComponent<Tag::DestroyTag>() || !chunk->HasComponent<JoltHandleComponent>()) continue;

        size_t count = chunk->GetEntityCount();
        const auto* joltHandles = chunk->GetComponentArray<JoltHandleComponent>();

        for (size_t i = 0; i < count; ++i) {
            if (chunk->IsEntityDestroyed(i)) continue;

            bodyInterface.RemoveBody(joltHandles[i].bodyID);
            bodyInterface.DestroyBody(joltHandles[i].bodyID);
        }
    }
}

// HierarchyCleanupSystem.cpp の末尾
REGISTER_LOGIC_SYSTEM(JoltCleanupSystem, Priority::LogicStage::L07_Cleanup);