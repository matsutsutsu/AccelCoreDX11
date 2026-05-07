#include "ZeroGDebrisSystem.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"
#include <cmath>

void ZeroGDebrisSystem::Update(float dt)
{
    // 分岐(if)のない純粋な算術演算ループ
    ForEachParallel([dt](TransformComponent& trans, ZeroGDebrisComponent& debris) {
        debris.timeAcc += dt;

        // 1. 波紋伝播による高さ(Y)の計算
        float phase = debris.distanceFromCenter - (debris.timeAcc * debris.waveSpeed);
        trans.position.y = debris.baseY + std::sin(phase * debris.floatFrequency) * debris.floatAmplitude;

        // 2. 無重力感を出すためのゆっくりとした回転加算
        DirectX::SimpleMath::Quaternion currentRot(trans.rotation.x, trans.rotation.y, trans.rotation.z, trans.rotation.w);
        DirectX::SimpleMath::Quaternion deltaRot = DirectX::SimpleMath::Quaternion::CreateFromAxisAngle(debris.rotationAxis, debris.rotationSpeed * dt);

        currentRot = currentRot * deltaRot;
        currentRot.Normalize();

        trans.rotation = { currentRot.x, currentRot.y, currentRot.z, currentRot.w };

        // =======================================================
        // ★修正: エンジンに行列の再計算を要求する
        // =======================================================
        trans.isDirty = true;
        trans.isStatic = false; // 動くオブジェクトであることの保証
        });
}

REGISTER_LOGIC_SYSTEM(ZeroGDebrisSystem, Priority::LogicStage::L02_Update);