#pragma once
#include "ECS/Common/CCL_Common.h"
#include "ECS/Core/CCL_World.h"
#include <DirectXMath.h>
#include <d3d11.h>
#include <string>
#include <unordered_map>

namespace Prefab {

    using namespace CCL::ECS;

    // 終了処理
    void Cleanup();

    // 初期化 (デバイス保持など)
    void Initialize(ID3D11Device *device);

    // プリウォーム (事前生成)
    void PrewarmParticles(Core::World &world);

    // パーティクルをプールに戻す関数
    void ReturnParticleToPool(Core::World &world, EntityID id);

    // =========================================================================
    // ★新システム: IDではなくファイルパスで生成する
    // =========================================================================

    // JSONからプレハブを生成 (座標指定なし)
    EntityID SpawnPrefab(Core::World &world, const std::string &path);

    // 座標指定で生成
    EntityID SpawnPrefab(
        Core::World &world, const std::string &path, const DirectX::XMFLOAT3 &position);

    // 座標・回転(Quaternion)指定で生成
    EntityID SpawnPrefab(Core::World &world,
        const std::string            &path,
        const DirectX::XMFLOAT3      &position,
        const DirectX::XMFLOAT4      &rotation);

    // 座標・Y軸回転(度数法)指定で生成
    EntityID SpawnPrefab(Core::World &world,
        const std::string            &path,
        const DirectX::XMFLOAT3      &position,
        float                         rotationY);

    // パーティクル生成 (既存のまま維持)
    EntityID SpawnParticle(Core::World &world,
        const std::string              &path,
        EntityID                        parentID,
        const DirectX::XMFLOAT3        &localPos);

} // namespace CCL::Prefab