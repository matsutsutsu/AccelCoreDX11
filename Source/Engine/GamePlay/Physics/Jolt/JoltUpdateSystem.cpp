// Engine/GamePlay/Physics/Jolt/JoltUpdateSystem.cpp
#include "JoltUpdateSystem.h"
#include "ECS/Core/CCL_World.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"
#include <Jolt/Jolt.h>
#include "Engine/GamePlay/Physics/Character/JoltCharacterHandleComponent.h"
#include "Engine/GamePlay/Core/Time/TimeState.h"

void JoltStepSystem::Update(float dt)
{
    if (!_world->HasResource<JoltPhysicsManager>()) return;
    auto& joltManager = _world->GetResource<JoltPhysicsManager>();

    // 物理シミュレーションに渡す時間を、世界の親時計(globalScale)で歪める
    float physicsDt = dt;
    if (_world->HasResource<TimeContext>()) {
        const auto& ctx = _world->GetResource<TimeContext>();
        physicsDt *= ctx.globalScale;
    }

    // 物理の時間を dt 秒だけ進める
    if (physicsDt > 0.0f) {
        joltManager.Step(physicsDt); // ここでJolt内部の並列計算が走る
    }
}

void JoltPullSystem::Update(float dt)
{
    if (!_world->HasResource<JoltPhysicsManager>()) return;

    auto& joltManager = _world->GetResource<JoltPhysicsManager>();
    JPH::BodyInterface& bodyInterface = joltManager.GetBodyInterface();


    ForEachWithIDParallel([&](CCL::ECS::EntityID id, TransformComponent& trans, const JoltHandleComponent& joltBody) {

        // ========================================================================
        // 「キャラクター」として操作されているものは、ここで同期してはいけない！
        // （JoltCharacterUpdateSystem で既に正しい位置が同期されており、
        //   ここで回転をPullするとゲームロジックと競合してカクカクするため除外する）
        // ========================================================================
        if (_world->HasComponent<JoltCharacterHandleComponent>(id)) {
            return;
        }

        // Kinematic (アニメーション駆動) の剛体はPullしてはいけない！
        // Kinematicの「正しい座標」は常にECS側(TransformUpdate)にあるため、Joltから上書きさせない。
        if (bodyInterface.GetMotionType(joltBody.bodyID) == JPH::EMotionType::Kinematic) {
            return;
        }


        // スリープ状態（止まっている物体）なら、座標コピーの無駄な処理を省く
        if (!bodyInterface.IsActive(joltBody.bodyID)) {
            return;
        }

        // Joltから最新の座標を取得
        JPH::RVec3 pos;
        JPH::Quat  rot;
        bodyInterface.GetPositionAndRotation(joltBody.bodyID, pos, rot);

        // ECSのTransformを上書き
        trans.position = { pos.GetX(), pos.GetY(), pos.GetZ() };
        trans.rotation = { rot.GetX(), rot.GetY(), rot.GetZ(), rot.GetW() };

        // TransformUpdateSystemに変更を知らせて行列を再計算させる
        trans.isStatic = false;
        });
}

// 登録順序がすべてを決定する
REGISTER_LOGIC_SYSTEM(JoltStepSystem, Priority::LogicStage::L04_Physics);
// 物理計算の【後】に同期しなければならないため、新しくPostPhysicsなどのステージを作るか、Collisionの後に配置します
REGISTER_LOGIC_SYSTEM(JoltPullSystem, Priority::LogicStage::L05_Collision);