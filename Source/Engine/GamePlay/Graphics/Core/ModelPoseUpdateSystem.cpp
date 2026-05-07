#include "ModelPoseUpdateSystem.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

using namespace CCL::ECS;

void ModelPoseUpdateSystem::Update(float dt) {
    // 全エンティティのモデルポーズを並列更新
    ForEachParallel([&](const TransformComponent& trans, ModelComponent& model) {
        auto* modelPtr = model.GetModel();
        if (!modelPtr) return;

        // モデル内部のノード階層に行列を流し込む
        // (アニメーションが有効な場合は、ここでボーンの計算も行う)
        // worldMatrix を渡すことで、モデル内の各メッシュがどこに居るべきか確定させる
        modelPtr->UpdateTransform(trans.worldMatrix);
        });
}

// ロジックフェーズ、かつトランスフォーム更新の直後あたりに登録
REGISTER_LOGIC_SYSTEM(ModelPoseUpdateSystem, Priority::LogicStage::L02_PostUpdate);