#pragma once
#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Physics/RigidBody/JoltRigidbodyComponent.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"


// ===================================================================================
// ファイル名: JoltBodySetupSystem.h
// 役割:
// ECSの「設計図」から、Jolt空間に「実体」を生み出す全自動ファクトリーシステム。
//
// 【アーキテクチャ仕様】
// - 実行フェーズ: PrePhysics (物理計算の前)
// - 対象: Transform + Rigidbody + (各種Collider) を持ち、JoltHandle
// を持たないEntity。
//
// 【使い方・ルール】
// -
// 新しい形状（MeshColliderなど）を追加したい場合は、このシステムの分岐を増やす。
// - 生成が終わったEntityには JoltHandleComponent
// を付与し、二度とこの工場を通さないようにする。
// ===================================================================================


// 物理の「設計図」から「実体」を自動生成するファクトリーシステム
class JoltBodySetupSystem : public CCL::ECS::IfSystem<JoltBodySetupSystem,
                                CCL::ECS::Read<TransformComponent>,
                                CCL::ECS::Read<JoltRigidbodyComponent>> {
  public:
    JoltBodySetupSystem() : IfSystem("JoltBodySetupSystem") {}
    virtual ~JoltBodySetupSystem() = default;

    void Update(float dt) override;
};