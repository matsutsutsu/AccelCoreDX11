#include "JoltPushSystem.h"
#include "ECS/Core/CCL_World.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

// Joltのモーションタイプ判定用
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyInterface.h>

void JoltPushSystem::Update(float dt) {
    if (!_world->HasResource<JoltPhysicsManager>())
        return;
    auto& joltManager = _world->GetResource<JoltPhysicsManager>();
    JPH::BodyInterface& bodyInterface = joltManager.GetBodyInterface();

    // 並列処理で全エンティティを最速チェック
    ForEachParallel(
        [&](TransformComponent& trans, const JoltHandleComponent& joltBody, const JoltRigidbodyComponent& rigid) {

            // ====================================================================
            //  エディタで Dynamic <-> Kinematic に変更された場合、即座にJoltへ反映させる！
            // ====================================================================
            if (bodyInterface.GetMotionType(joltBody.bodyID) != rigid.motionType) {
                bodyInterface.SetMotionType(joltBody.bodyID, rigid.motionType, JPH::EActivation::Activate);
            }

            // ====================================================================
            //  ローカル座標ではなく、最新のワールド(絶対)座標を抽出する
            // ====================================================================
            DirectX::XMFLOAT3 worldPos = trans.GetWorldPosition();
            DirectX::XMFLOAT4 worldRot = trans.GetWorldRotation();

            JPH::RVec3 joltPos(worldPos.x, worldPos.y, worldPos.z);
            JPH::Quat  joltRot(worldRot.x, worldRot.y, worldRot.z, worldRot.w);
            joltRot = joltRot.Normalized(); // ★超重要: わずかな計算誤差でもJoltは荒ぶるため必ず正規化する

            // ====================================================================
            // 処理A: フラグが立った時の「強制ワープ」
            // ====================================================================
            if (trans.isTeleported) {
                bodyInterface.SetPositionAndRotation(
                    joltBody.bodyID,
                    joltPos,
                    joltRot,
                    JPH::EActivation::Activate);

                trans.isTeleported = false;
            }
            // ====================================================================
            //  アニメーションで動く物体（Kinematic剛体）の「毎フレーム同期」
            // ====================================================================
            else {
                // この剛体が「Kinematic（プログラム/アニメーション駆動）」なら、
                // 毎フレームECS側の最新のワールド座標を物理エンジンに押し付ける
                if (bodyInterface.GetMotionType(joltBody.bodyID) == JPH::EMotionType::Kinematic) {

                    if (trans.isDirty) {
                        // SetPositionAndRotation を使うことで、アニメーションの動きに合わせて
                        // 正確にJoltのコリジョン枠が追従します。
                        // (もし他の剛体を押しのけたい場合は bodyInterface.MoveKinematic を使います)
                        bodyInterface.SetPositionAndRotation(
                            joltBody.bodyID,
                            joltPos,
                            joltRot,
                            JPH::EActivation::Activate
                        );
                    }
                }
            }
        });
}

// ★物理計算の前にワープを完了させる
REGISTER_LOGIC_SYSTEM(JoltPushSystem, Priority::LogicStage::L03_PrePhysics);