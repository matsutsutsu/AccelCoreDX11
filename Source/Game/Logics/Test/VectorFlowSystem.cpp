#include "VectorFlowSystem.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

void VectorFlowSystem::Update(float dt)
{
    ForEachParallel([&](TransformComponent& trans, VectorFlowComponent& flow) {

        flow.timeAcc += dt;

        // Transformの現在位置をベースに、3Dノイズ的(サイン波の合成)な「力場」を計算
        float velX = std::sin(trans.position.y * flow.frequency + flow.timeAcc) * flow.amplitude;
        float velY = std::cos(trans.position.z * flow.frequency + flow.timeAcc) * flow.amplitude;
        float velZ = std::sin(trans.position.x * flow.frequency + flow.timeAcc) * flow.amplitude;

        // 基本速度ベクトル + 力場の影響を加算して移動
        trans.position.x += velX * flow.baseSpeed * dt;
        trans.position.y += velY * flow.baseSpeed * dt;
        trans.position.z += velZ * flow.baseSpeed * dt;

        // 指定エリア外に出たらループさせる処理（シームレスな環境構築用）
        const float BOUND = 50.0f;
        if (trans.position.x > BOUND) trans.position.x -= BOUND * 2.0f;
        if (trans.position.x < -BOUND) trans.position.x += BOUND * 2.0f;

        if (trans.position.y > BOUND) trans.position.y -= BOUND * 2.0f;
        if (trans.position.y < -BOUND) trans.position.y += BOUND * 2.0f;

        if (trans.position.z > BOUND) trans.position.z -= BOUND * 2.0f;
        if (trans.position.z < -BOUND) trans.position.z += BOUND * 2.0f;
        });
}

REGISTER_LOGIC_SYSTEM(VectorFlowSystem, Priority::LogicStage::L02_Update);