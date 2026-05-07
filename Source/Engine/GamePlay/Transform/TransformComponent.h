#pragma once
#include <DirectXMath.h>
#include "ECS/Common/CCL_Common.h"

// ===================================================================================
// TransformComponent 物理同期に関するルール
// - Joltの剛体をワープさせたい場合は、必ず SetPosition() または
// SetPositionAndRotation() を使用する。
// - これにより内部で isTeleported フラグが立ち、JoltPushSystem
// が安全に物理空間を上書きする。
// - ❌ 物理演算で動く物体（弾、キャラクター）の移動に SetPosition
// を毎フレーム使ってはいけない！
// ===================================================================================

/**
 * @brief エンティティの空間的な位置、回転、スケールを管理するコンポーネント。
 * @note データ指向設計（DOD）に基づき、ローカル座標（保存対象）とワールド行列（計算キャッシュ）を分離しています。
 */
struct TransformComponent {
    // --- 【データ部】（保存・編集する値 = Local） ---
    DirectX::XMFLOAT3   position    = {0, 0, 0};    // 親からの相対位置 (Local Position)
    DirectX::XMFLOAT4   rotation    = {0, 0, 0, 1}; // 親からの相対回転 (Local Rotation)
    DirectX::XMFLOAT3   scale       = {1, 1, 1};    // 親からの相対サイズ (Local Scale)

    // 静的オブジェクトフラグ (trueなら更新しない)
    bool isStatic = false;

    // 物理ワープ用のDirtyフラグ（1バイト増えますが許容範囲のコストです）
    bool isTeleported = false;

    // =========================================================
    // ★ 状態管理フラグ
    // =========================================================
    /**
     * @brief 行列の再計算を強制するフラグ。
     * @note 初期値を true にすることで、ロード直後や生成直後は isStatic = true でも必ず1回は worldMatrix が計算されます。
     * @warning 実行時の一時データであるため、シリアライズ（保存）の対象には含めないでください。
     */
    bool isDirty = true;


    // --- 【キャッシュ部】（システムが計算して書き込む値 = World） ---
    // ※手動では書き換えず、TransformUpdateSystemに計算させる
    DirectX::XMFLOAT4X4 worldMatrix = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

	// 親子関係データ
    // リンク情報
    CCL::ECS::EntityID parentID      = 0; // 親
    CCL::ECS::EntityID firstChildID  = 0; // 最初の子 (リストの先頭)
    CCL::ECS::EntityID prevSiblingID = 0; // 前の兄弟
    CCL::ECS::EntityID nextSiblingID = 0; // 次の兄弟

    // =========================================================
    // ★開発体験(DX)を最高にするための直感的Setter
    // =========================================================
    void SetPosition(const DirectX::XMFLOAT3 &newPos) {
      position = newPos;
      isTeleported = true; // 裏で自動的にフラグを立てる
      isStatic = false;    // 行列の再計算も要求
    }

    void SetPosition(float x, float y, float z) {
      position = {x, y, z};
      isTeleported = true;
      isStatic = false;
    }

    // ------------------------------------------------------------------
    // 直感的に使うためのアクセサ（ヘルパー関数）
    // ------------------------------------------------------------------

    // ワールド座標を取得（物理判定や描画、音の発生源などで使用）
    DirectX::XMFLOAT3 GetWorldPosition() const
    {
        return DirectX::XMFLOAT3(worldMatrix._41, worldMatrix._42, worldMatrix._43);
    }

    // ワールド回転 (絶対回転) を取得する
    DirectX::XMFLOAT4 GetWorldRotation() const
    {
        DirectX::XMVECTOR scale, rotQuat, trans;
        DirectX::XMMatrixDecompose(&scale, &rotQuat, &trans, DirectX::XMLoadFloat4x4(&worldMatrix));

        DirectX::XMFLOAT4 outRot;
        DirectX::XMStoreFloat4(&outRot, rotQuat);
        return outRot;
    }

    // ワールド空間での「前方」ベクトルを取得（弾の発射方向などで使用）
    // ※Z+方向を前方とする一般的なDirectX座標系の場合
    DirectX::XMFLOAT3 GetForward() const
    {
        // 行列の3行目がZ軸の向き（回転成分）
        using namespace DirectX;
        XMVECTOR f = XMVectorSet(worldMatrix._31, worldMatrix._32, worldMatrix._33, 0.0f);
        f          = XMVector3Normalize(f); // スケールが入っている場合に備えて正規化
        XMFLOAT3 out;
        XMStoreFloat3(&out, f);
        return out;
    }

    // ワールド空間での「右」ベクトル
    DirectX::XMFLOAT3 GetRight() const
    {
        using namespace DirectX;
        XMVECTOR r = XMVectorSet(worldMatrix._11, worldMatrix._12, worldMatrix._13, 0.0f);
        r          = XMVector3Normalize(r);
        XMFLOAT3 out;
        XMStoreFloat3(&out, r);
        return out;
    }

    // ワールド空間での「上」ベクトル
    DirectX::XMFLOAT3 GetUp() const
    {
        using namespace DirectX;
        XMVECTOR u = XMVectorSet(worldMatrix._21, worldMatrix._22, worldMatrix._23, 0.0f);
        u          = XMVector3Normalize(u);
        XMFLOAT3 out;
        XMStoreFloat3(&out, u);
        return out;
    }

    // 行列更新関数 (親の行列を受け取るように変更)
    void UpdateMatrix(const DirectX::XMMATRIX &parentMatrix = DirectX::XMMatrixIdentity())
    {
        using namespace DirectX;

        // ローカル行列作成
        XMMATRIX S           = XMMatrixScaling(scale.x, scale.y, scale.z);
        XMMATRIX R           = XMMatrixRotationQuaternion(XMLoadFloat4(&rotation));
        XMMATRIX T           = XMMatrixTranslation(position.x, position.y, position.z);
        XMMATRIX localMatrix = S * R * T;

        // ワールド行列 = ローカル × 親
        XMMATRIX world = localMatrix * parentMatrix;

        XMStoreFloat4x4(&worldMatrix, world);
    }
};