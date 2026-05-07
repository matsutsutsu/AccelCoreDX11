#pragma once

#include<DirectXMath.h>

/**
 * @brief エンティティの動的な移動・回転情報を管理するコンポーネント。
 * @note TransformComponentを直接書き換えるのではなく、このコンポーネントの値を
 *       システムが計算し、最終的にTransformへ反映（Integrate）させます。
 */
struct MotionComponent 
{
    // --- 【線形移動データ】 ---
    DirectX::XMFLOAT3 velocity = { 0, 0, 0 }; // 1秒あたりの移動量 (m/s)
    DirectX::XMFLOAT3 acceleration = { 0, 0, 0 }; // 加速度

    // --- 【回転移動データ】 ---
    // オイラー角での回転速度（deg/s または rad/s）
    // クォータニオンより直感的に力を加えやすいためこちらを採用
    DirectX::XMFLOAT3 angularVelocity = { 0, 0, 0 };

    // --- 【調整パラメータ】 ---
    float friction = 0.0f;     // 摩擦・減衰率（0なら減衰なし）
    float maxSpeed = 100.0f;   // 最大速度制限

    // --- 【蓄積された変位】 ---
    // 物理エンジンやプレイヤー入力システムが「今フレームでこれだけ動かしたい」という値を積む場所
    // システムの最後に Transform.position += impulse; して 0 にリセットする運用
    DirectX::XMFLOAT3 pendingMovement = { 0, 0, 0 };

    // =========================================================
    // ★ 便利メソッド
    // =========================================================

    /**
     * @brief 瞬間的な移動（テレポートではなく押し出しなど）を加算する
     */
    void AddImpulse(const DirectX::XMFLOAT3& impulse) {
        pendingMovement.x += impulse.x;
        pendingMovement.y += impulse.y;
        pendingMovement.z += impulse.z;
    }

    /**
     * @brief 力を加える（加速度への加算）
     */
    void AddForce(const DirectX::XMFLOAT3& force, float mass = 1.0f) {
        acceleration.x += force.x / mass;
        acceleration.y += force.y / mass;
        acceleration.z += force.z / mass;
    }
};