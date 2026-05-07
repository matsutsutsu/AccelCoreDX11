#pragma once
#include "ECS/Core/CCL_World.h"
#include "Game/Core/AllComponents.h"
#include "Engine/Serialization/Factory/Prefab.h"

// ゲームレイヤー専用のユーティリティ名前空間
namespace EntityFactory {

    // ========================================================================
    //  ECSコアを汚さずに「名前付きSpawn」を実現する魔法のヘルパー関数
    // ========================================================================
    // アーキタイプ変数と名前を渡すだけで、自動でNameComponentを付与して生成します。
    inline CCL::ECS::Core::EntityRef SpawnNamed(CCL::ECS::Core::World &world,
                                            const char *name,
                                            const CCL::ECS::Archetype &baseArch)
    {
        // 1. World の純粋な機能を使ってエンティティを生成
        auto ref = world.Spawn(baseArch);

        // 2. 名前をセット（NameComponentを持っていなければ、自動的に追加キューに乗る）
        ref.Set(NameComponent{ name });

        return ref;
    }


    // プレハブをSpawnして、便利な EntityRef を返す
    inline CCL::ECS::Core::EntityRef SpawnPrefab(CCL::ECS::Core::World &world,
                                                 const char *prefabPath) {
      CCL::ECS::EntityID id = Prefab::SpawnPrefab(world, prefabPath);
      return {&world, id};
    }

} // namespace CCL::Game