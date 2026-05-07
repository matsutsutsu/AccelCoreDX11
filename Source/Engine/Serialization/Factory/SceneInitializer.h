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


    // ジョルトのテスト環境
    static void SpawnJoltTest(CCL::ECS::Core::World &world, ID3D11Device *device);

	static void SpawnAnimationTest(CCL::ECS::Core::World& world, ID3D11Device* device);

	static void SpawnShaderTest(CCL::ECS::Core::World& world, ID3D11Device* device);

    // 球体カーコントローラーのテスト環境
    static void SpawnPlayerCarTest(CCL::ECS::Core::World& world, ID3D11Device* device);

    // ボスとドローン群の生成テスト環境
    static void SpawnBossAndDronesTest(CCL::ECS::Core::World& world, ID3D11Device* device);


    // =======================================================
    //  指定した数のモデルをグリッド状に並べるテスト
    // =======================================================
    static void SpawnModelGridTest(CCL::ECS::Core::World& world, ID3D11Device* device, int count);


    // 「ECS限界テスト（ギャラクシー・スウォーム）」環境
    static void SpawnECSBenchmarkTest(CCL::ECS::Core::World& world, ID3D11Device* device, int count);

    // 「幾何学の海」ベンチマークテスト
    static void SpawnVoxelOceanTest(CCL::ECS::Core::World& world, ID3D11Device* device, int gridSize);

    // 物理エンジンの極限テスト（数万個の雪崩）
    static void SpawnPhysicsAvalancheTest(CCL::ECS::Core::World& world, ID3D11Device* device, int count);


    // 既存の宣言の下に追加
    static void SpawnLissajousTest(CCL::ECS::Core::World& world, ID3D11Device* device, int count);
    static void SpawnTornadoTest(CCL::ECS::Core::World& world, ID3D11Device* device, int count);
    static void SpawnVectorFlowTest(CCL::ECS::Core::World& world, ID3D11Device* device, int count);

};