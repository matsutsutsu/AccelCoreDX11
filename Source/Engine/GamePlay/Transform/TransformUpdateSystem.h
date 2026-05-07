#pragma once

#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include <DirectXMath.h>
#include "Engine/GamePlay/Core/DestroyTag.h"

#include "PendingParentComponent.h"

// 前方宣言
namespace CCL::ECS::Core {
    class World;
}

class TransformUpdateSystem : public CCL::ECS::IfSystem<TransformUpdateSystem,CCL::ECS::Write<TransformComponent>> {
  public:
    TransformUpdateSystem();
    virtual ~TransformUpdateSystem() = default;

    // フレーム毎の更新処理
    void Update(float dt) override;


    // --- 親子関係操作用ヘルパー関数 (static) ---
    // 外部（PrefabやLogic）から TransformUpdateSystem::SetParent(...) として呼び出します
    static void SetParent(
        CCL::ECS::Core::World &world, CCL::ECS::EntityID child, CCL::ECS::EntityID parent);

    // 親から子を切り離す
    static void DetachChild(CCL::ECS::Core::World &world, CCL::ECS::EntityID child);

    // 階層構造を安全に維持しながら削除予約する
    // destroyChildren: trueなら子供も道連れにする（通常はtrue推奨）
    static void DestroyEntityWithHierarchy(
        CCL::ECS::Core::World &world, CCL::ECS::EntityID entity, bool destroyChildren = true);


    // 名前を変更: DestroyEntityWithHierarchy -> MarkForDestruction
    // 直接消すのではなく、「削除タグ」を再帰的に付与する関数に変更
    static void MarkForDestruction(
        CCL::ECS::Core::World &world, CCL::ECS::EntityID entity, bool destroyChildren = true);

  private:
 
    // 保留中の親子関係を適用する関数
    void ProcessPendingParents();

    // リンク切れを修復する関数（メインスレッド実行用）
    void RepairBrokenLinks();

};