#include "TimerSystem.h"

// 実装で必要なヘッダ群
#include "ECS/Core/CCL_World.h"
#include "Engine/GamePlay/Core/DestroyTag.h"

// システムの実行順序の定義ヘッダー
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

TimerSystem::TimerSystem() : IfSystem("TimerSystem") {}

void TimerSystem::Update(float dt)
{
    ForEachWithID([&](CCL::ECS::EntityID id, TimerComponent &timer) {
        // 時間を進める
        timer.currentTime += dt;

        // 寿命チェック
        if (timer.lifeTime > 0.0f && timer.currentTime >= timer.lifeTime) {
            if (timer.isExpired) return;

            // Pooledだろうが通常Entityだろうが、WorldのDestroyを呼ぶだけ
            _world->Destroy(id);

            timer.isExpired = true;
        }
    });
}

// ==========================================
// マクロは必ず .cpp の末尾に1回だけ書く
// ==========================================
REGISTER_LOGIC_SYSTEM(TimerSystem, Priority::LogicStage::L02_Update);