#include "DissolveSystem.h"
#include "Engine/Graphics/Resource/ResourceManager.h" 

// システムの実行順序の定義ヘッダー
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

using namespace CCL::ECS;

DissolveSystem::DissolveSystem() : IfSystem("DissolveSystem") {}

void DissolveSystem::Update(float dt)
{
    // IfSystem内で直接Destroyを呼ぶとイテレータが壊れる可能性があるため、削除リストを作る
    std::vector<EntityID> deadEntities;

    ForEachWithID([&](EntityID id, DissolveComponent &dissolve, MaterialComponent &matComp) {
        // 1. 進行度を更新
        dissolve.currentThreshold += dissolve.dissolveSpeed * dt;

        // 2. GPUへ送る汎用パラメータのX成分に、ディゾルブ進行度を代入
        matComp.customParams.x = dissolve.currentThreshold;

        // 3. 完全に消滅したら削除リストへ
        if (dissolve.currentThreshold >= 1.0f) {
            deadEntities.push_back(id);
        }

    });

    // 4. 世界から完全に消去
    for (auto id : deadEntities) {
        _world->Destroy(id);
    }
}


REGISTER_RENDER_SYSTEM(DissolveSystem, Priority::RenderStage::R08_Main);