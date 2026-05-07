#pragma once
#include "ECS/Common/CCL_Common.h"
#include <DirectXMath.h>

// レイキャストの結果を返すための構造体
struct PhysicsHitInfo {
    bool hit = false;
    CCL::ECS::EntityID entity = CCL::ECS::InvalidEntityID; // 誰に当たったか
    DirectX::XMFLOAT3 position = {0.0f, 0.0f, 0.0f};         // どこに当たったか
    DirectX::XMFLOAT3 normal = {0.0f, 1.0f, 0.0f};           // 当たった面の向き（跳弾計算などに使う）
    float distance = 0.0f;                                   // 発射位置からの距離
};

class IPhysicsAPI {
public:
    virtual ~IPhysicsAPI() = default;

    // =======================================================
    // 🔵 直進の力と速度 (Linear)
    // =======================================================
    // 継続的な力を加える（アクセル、風など）
    virtual void AddForce(CCL::ECS::EntityID entity, const DirectX::XMFLOAT3& force) = 0;
    // 瞬間的な衝撃を加える（ジャンプ、爆発など）
    virtual void AddImpulse(CCL::ECS::EntityID entity, const DirectX::XMFLOAT3& impulse) = 0;
    
    virtual void SetLinearVelocity(CCL::ECS::EntityID entity, const DirectX::XMFLOAT3& velocity) = 0;
    virtual DirectX::XMFLOAT3 GetLinearVelocity(CCL::ECS::EntityID entity) = 0;

    // =======================================================
    // 🟢 回転の力と速度 (Angular)
    // =======================================================
    // 継続的な回転力を加える
    virtual void AddTorque(CCL::ECS::EntityID entity, const DirectX::XMFLOAT3& torque) = 0;
    // 瞬間的な回転の衝撃を加える（車が横転する時など）
    virtual void AddAngularImpulse(CCL::ECS::EntityID entity, const DirectX::XMFLOAT3& impulse) = 0;

    virtual void SetAngularVelocity(CCL::ECS::EntityID entity, const DirectX::XMFLOAT3& velocity) = 0;
    virtual DirectX::XMFLOAT3 GetAngularVelocity(CCL::ECS::EntityID entity) = 0;

    // =======================================================
    // 🟡 強制的な姿勢制御 (Kinematic Warp)
    // =======================================================
    // 物理法則を無視して指定座標にワープさせる（リスポーン時など）
    virtual void SetPosition(CCL::ECS::EntityID entity, const DirectX::XMFLOAT3& position) = 0;
    virtual void SetRotation(CCL::ECS::EntityID entity, const DirectX::XMFLOAT4& rotation) = 0;

    // 特定の方向を向かせるヘルパー関数（Yaw回転）
    virtual void SetLookDirection(CCL::ECS::EntityID entity, const DirectX::XMFLOAT3& direction) = 0;

    // =======================================================
    // 🔴 空間クエリ (Queries)
    // =======================================================
    // 既存：真下への接地判定
    virtual bool CheckGrounded(CCL::ECS::EntityID entity, float distance) = 0;
    
    // 追加：任意の場所から任意の方向へレーザーを撃つ（銃の弾や、敵の視界判定に使う）
    virtual PhysicsHitInfo RayCast(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float maxDistance, CCL::ECS::EntityID ignoreEntity = CCL::ECS::InvalidComponentID) = 0;
};