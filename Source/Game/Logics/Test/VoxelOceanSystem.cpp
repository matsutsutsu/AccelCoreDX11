#include "VoxelOceanSystem.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

void VoxelOceanSystem::Update(float dt)
{
    // ECSの真骨頂: メモリが連続しているため、猛烈な速度で10万回の計算が終わります
    ForEachParallel([&](TransformComponent& trans, VoxelOceanComponent& ocean) {

        // 経過時間を進める
        ocean.timeElapsed += dt * ocean.waveSpeed;

        float x = trans.position.x;
        float z = trans.position.z;
        float t = ocean.timeElapsed;

        // -------------------------------------------------------------
        // 🌊 複合波の合成（Gerstner Wave 簡易版）
        // -------------------------------------------------------------
        // 波1: 全体的にゆっくり大きくうねるベースの波
        float wave1 = std::sin(x * 0.1f + t) * 2.0f;

        // 波2: 少し斜め方向から来る、少し速い中くらいの波
        float wave2 = std::cos(z * 0.15f + t * 1.2f) * 1.5f;

        // 波3: 細かく波立つ、対角線方向の速い波
        float wave3 = std::sin((x + z) * 0.2f - t * 1.5f) * 1.0f;

        // 3つの波を足し合わせて、最終的な高さを決定する
        trans.position.y = wave1 + wave2 + wave3;

        // （おまけ）少し揺れるような回転を加えるとより滑らかに見えます
        // 簡易的にX軸とZ軸にわずかな傾きを与えます
        float tiltX = std::cos(x * 0.1f + t) * 0.1f;
        float tiltZ = -std::sin(z * 0.15f + t * 1.2f) * 0.1f;

        // クォータニオンをEuler角から簡易生成して代入 (※エンジンの仕様に合わせてください)
        // ここでは簡単な見栄えのための擬似的な回転です
        // もし重ければ回転計算は削除しても波としては成立します
        });
}

// LogicグループのUpdateフェーズで回す
REGISTER_LOGIC_SYSTEM(VoxelOceanSystem, Priority::LogicStage::L02_Update);