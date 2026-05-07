#pragma once
#include "ECS/Core/CCL_World.h"
#include <string>

// 特定のエンティティをPrefab(JSON)として保存するクラス
class PrefabSerializer {
  public:
    static void Save(const std::string &filepath,
        CCL::ECS::Core::World          *world,
        CCL::ECS::EntityID              entity,
        const std::string              &name);
};