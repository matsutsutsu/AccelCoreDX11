#pragma once
#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "Game/Logics/AI/NavAI/AIComponents.h"

/**
 * @brief AIの内部状態（視界、聴覚範囲、現在のステート）を可視化するデバッグ描画システム。
 * @note
 * このシステムはデータへの副作用（Write）を持たず、Readのみで構成される。
 * レンダリングフェーズ（RenderStage）で実行されるため、ロジック系の計算とは分離されている。
 */
class AIDebugDrawSystem : public CCL::ECS::IfSystem<AIDebugDrawSystem,
    CCL::ECS::Read<TransformComponent>,
    CCL::ECS::Read<AIPerceptionComponent>,
    CCL::ECS::Read<AIStateComponent>> {
public:
    AIDebugDrawSystem() : IfSystem("AIDebugDrawSystem") {
        // デフォルトで表示オン。エディタのUI等からトグル可能にする。
        this->isDebugVisible = true;
    }
    virtual ~AIDebugDrawSystem() = default;

    void Update(float dt) override;
};