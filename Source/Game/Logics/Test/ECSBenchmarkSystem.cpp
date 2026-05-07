#include "ECSBenchmarkSystem.h"

// システムの実行順序の定義ヘッダー
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

void ECSBenchmarkSystem::Update(float dt)
{
    ForEachParallel([&](TransformComponent& trans, OrbitBenchmarkComponent& orbit) {
        // 1. 角度の更新
        orbit.angle += orbit.orbitSpeed * dt;

        // 2. CPUへの負荷テスト（三角関数による軌道計算）
        float x = std::cos(orbit.angle) * orbit.radius;
        float z = std::sin(orbit.angle) * orbit.radius;

        // 上下に波打つ立体的な動き
        float y = std::sin(orbit.angle * orbit.waveSpeed + orbit.radius) * 10.0f;

        // 3. Transformへの直接書き込み
        // （※エンジン側の仕様に合わせて localPosition 等に入れてください）
        trans.position = { x, y, z };
        });
}

// Logicグループ（マルチスレッドで並列実行されるフェーズ）に登録
REGISTER_LOGIC_SYSTEM(ECSBenchmarkSystem, Priority::LogicStage::L02_Update);