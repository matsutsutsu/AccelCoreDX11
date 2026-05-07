#include "TestAnimationPlayerSystem.h"
#include "ECS/Core/CCL_World.h"
#include "Engine/Platform/Input/Input.h"
#include "Engine/Core/Math/StringHash.h" // HashString用
#include <SimpleMath.h>
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

    using namespace DirectX::SimpleMath;

    void TestAnimationPlayerSystem::Update(float dt)
    {
        // 1. 入力リソースの取得
        if (!_world->HasResource<Input*>()) return;
        auto* input = _world->GetResource<Input*>();
        if (!input) return;

        auto& kb = input->GetKeyboard();
        auto& mouse = input->GetMouse();

        // 毎フレームのハッシュ計算を避けるため、static変数で事前計算してキャッシュする (DOD的最適化)
        static const uint32_t HASH_SPEED = CCL::Utils::HashString("Speed");
        static const uint32_t HASH_ATTACK = CCL::Utils::HashString("Attack");


        // 押された瞬間（1フレーム）だけを抽出するエッジ検知             
        static bool wasLeftDown = false; // 前回のフレームの入力状態を記憶する変数
        bool isLeftDown = mouse.IsDown(mouse.BTN_LEFT);

        // 「今は押されている」 かつ 「前回は押されていなかった」 時のみ true
        bool isAttackTriggered = isLeftDown && !wasLeftDown;

        wasLeftDown = isLeftDown; // 次のフレームのために、今の状態を記憶しておく


        // 2. エンティティごとの並列処理
        ForEach([&](TransformComponent& trans,
            AnimParametersComponent& animParams,
            const TestAnimationPlayerComponent& playerDef)
            {
                // --- 移動ベクトルの計算 (WASD) ---
                Vector3 moveDir = Vector3::Zero;
                if (kb.IsDown('W')) moveDir.z += 1.0f;
                if (kb.IsDown('S')) moveDir.z -= 1.0f;
                if (kb.IsDown('A')) moveDir.x -= 1.0f;
                if (kb.IsDown('D')) moveDir.x += 1.0f;

                float currentSpeed = 0.0f;

                if (moveDir.LengthSquared() > 0.001f) {
                    moveDir.Normalize();
                    currentSpeed = playerDef.moveSpeed;

                    // 物理演算を無視し、強制的に座標を書き換える
                    trans.position = trans.position + (moveDir * currentSpeed * dt);

                    // 振り向き（滑らかなSlerp回転）
                    float targetYaw = atan2f(moveDir.x, moveDir.z);
                    Quaternion targetRot = Quaternion::CreateFromAxisAngle(Vector3::UnitY, targetYaw);
                    trans.rotation = Quaternion::Slerp(trans.rotation, targetRot, playerDef.turnSpeed * dt);
                }

                // --- 掲示板 (AnimParametersComponent) の更新 ---
                // Animatorの存在を一切知ることなく、ただ事実（パラメータ）だけを書き込む
                animParams.SetFloat(HASH_SPEED, currentSpeed);
                if (isAttackTriggered)  animParams.SetTrigger(HASH_ATTACK);

            });
    }


REGISTER_LOGIC_SYSTEM(TestAnimationPlayerSystem, Priority::LogicStage::L02_Update);