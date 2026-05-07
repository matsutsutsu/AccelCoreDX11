#pragma once
#include "ECS/System/CCL_System.h"
#include "Engine/Physics/JoltPhysicsManager.h"
#include "Engine/GamePlay/Physics/Jolt/JoltHandleComponent.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "Engine/GamePlay/Physics/RigidBody/JoltRigidbodyComponent.h"

// ===================================================================================
// ファイル名: JoltPushSystem.h
// 役割:
// ゲームロジック側で行われた「強制ワープ」の要求を、Jolt空間に伝達するシステム。
//
// 【アーキテクチャ仕様】
// - 実行フェーズ: PrePhysics (物理計算の直前)
// - 対象: Transform の isTeleported フラグが true になっているEntity。
//
// 【使い方・ルール】
// - 毎フレーム全Entityを並列スキャンし、フラグが立っているものだけを Jolt に
// SetPosition する。
// - 処理後はフラグを false
// に戻すため、1回の要求につき1度だけ安全にワープが実行される。
// ===================================================================================


class JoltPushSystem
  : public CCL::ECS::IfSystem<JoltPushSystem,
                                CCL::ECS::Write<TransformComponent>,
                                CCL::ECS::Read<JoltHandleComponent>,
                                CCL::ECS::Read<JoltRigidbodyComponent>> { 
  public:
    JoltPushSystem() : IfSystem("JoltPushSystem") {}
    void Update(float dt) override;
};