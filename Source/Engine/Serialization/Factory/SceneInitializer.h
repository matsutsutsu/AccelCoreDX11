#pragma once
#include "ECS/Core/CCL_World.h"

// モデルクラスのパスは適宜合わせてください
#include <d3d11.h>
#include <memory>

// SceneInitializer.h
class SceneInitializer {
  public:
    // ライトなどの最初のシーン設定
    static void SpawnLightStartSet(CCL::ECS::Core::World &world, ID3D11Device *device);

    // ゲームシーン用のライト設定

    // カメラテスト
    static void SpawnCameraTest(CCL::ECS::Core::World &world, ID3D11Device *device, int count);


    // ボスとドローン群の生成テスト環境
    static void SpawnBossAndDronesTest(CCL::ECS::Core::World& world,int count);


	static void SpawnAnimationTest(CCL::ECS::Core::World& world, ID3D11Device* device);

	static void SpawnShaderTest(CCL::ECS::Core::World& world, ID3D11Device* device);

    // 物理エンジンの極限テスト（数万個の雪崩）
    static void SpawnPhysicsAvalancheTest(CCL::ECS::Core::World& world, ID3D11Device* device, int count);


    static void SpawnBossVFXTest(CCL::ECS::Core::World& world);
};