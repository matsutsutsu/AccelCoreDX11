#include "PlayerCarInputSystem.h"

#include "ECS/Core/CCL_Chunk.h"
// ★修正: 生の Input.h ではなく、抽象化された API をインクルード
#include "Engine/Platform/Input/IInputAPI.h"

// システムの実行順序の定義ヘッダー
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

using namespace CCL::ECS;

PlayerCarInputSystem::PlayerCarInputSystem()
    : IfSystem("PlayerCarInputSystem")
{
}

void PlayerCarInputSystem::Update(float dt)
{
    // 生の Input* ではなく、抽象化された Facade (IInputAPI) をスマートポインタで受け取る
    if (!_world->HasResource<std::shared_ptr<IInputAPI>>()) return;
    auto input = _world->GetResource<std::shared_ptr<IInputAPI>>();


    ForEach([&](PlayerCarComponent& car) {

        // =======================================================
        // ★ 魔法の構文 "_hash" を使った超高速・データ駆動入力
        // Wキーの同時押し対策やスティックの判定は、すべて Facade の中で解決済み！
        // =======================================================

        // アクセル (前進・後退)
        car.input.throttle = input->GetAxis("Throttle"_hash);

        // ステアリング (左右)
        car.input.steering = input->GetAxis("Steer"_hash);

        // ジャンプ (単発ボタン)
        // ※ GetActionTriggered は「押された瞬間」の判定になります。
        car.input.jump = input->GetActionTriggered("Jump"_hash);

        });
    
}

REGISTER_LOGIC_SYSTEM(PlayerCarInputSystem, Priority::LogicStage::L01_Input);