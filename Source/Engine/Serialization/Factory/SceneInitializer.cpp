#include "SceneInitializer.h"
#include "ECS/Common/CCL_Common.h"
#include "Game/Core/AllComponents.h"
#include "Engine/Serialization/Factory/Prefab.h"
#include "Engine/Serialization/Factory/EntityFactory.h"
#include "Game/Logics/Character/Player/FPSPlayerComponent.h"
#include "Game/Logics/Character/Player/TPS/TPSPlayerComponent.h"
#include "Game/Logics/Character/Player/PlayerStateComponent.h"
#include "Game/Logics/Combat/StaminaComponent.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "Game/Logics/Character/Player/PhysicsTag.h"
#include "Game/Logics/Character/Enemy/EnemyTag.h"
#include "Game/Logics/Character/Player/FPSPlayerViewComponent.h"
#include "Game/Logics/Character/Player/TPS/PlayerViewComponent.h"
#include "Game/Logics/System/Modifier/ModifierComponent.h"
#include <random>
#include <vector>



using namespace CCL::ECS;


void SceneInitializer::SpawnCameraTest(
    CCL::ECS::Core::World &world, ID3D11Device *device, int count)
{
    // プレイヤー
    EntityID playerID = Prefab::SpawnPrefab(world, "Assets/Prefabs/Player.json", {0, 0, 0});

    // 追従カメラ (ID指定ではなくJSONパス)
    // CameraBrainSystemが入っていれば自動リンクされるが、念のため手動リンクのコードも残しておく例
    EntityID camID = Prefab::SpawnPrefab(world, "Assets/Prefabs/Camera/FollowCamera.json");

    // フリーカメラ
    Prefab::SpawnPrefab(world, "Assets/Prefabs/Camera/FreeCamera.json", {0, 10, -10});

    // 床
    Prefab::SpawnPrefab(world, "Assets/Prefabs/Stage/Map_Floor.json");
}

void SceneInitializer::SpawnFPSPlayer(CCL::ECS::Core::World& world)
{
   

    const auto FPSPlayerArch = ArchetypeHelper::Generate<
        TransformComponent,
        FPSPlayerComponent,
        StaminaComponent,
        FPSPlayerViewComponent,
        ModifierComponent,
        ModifierStatusComponent,
        ModelComponent,
        MaterialComponent,
        JoltCharacterConfigComponent, // 物理設定
        JoltBoxColliderComponent, // 衝突形状
        JoltRigidbodyComponent // 衝突形状
    >();

    // 2. 見た目エンティティ (ModelEnty) の生成
    auto FPSPlayer = EntityFactory::SpawnNamed(world, "Player_Model", FPSPlayerArch);

    ModelComponent modelComp;
    modelComp.SetModel("Data/Model/Jammo/Jammo.gltf");

    FPSPlayer.Set(TransformComponent{ .rotation = {0.0f, 0.0f, 0.0f, 1.0f},.position = {0, 0, 0},.scale{0,0,0} })
        .Set(FPSPlayerComponent())
        .Set(StaminaComponent())
        .Set(FPSPlayerViewComponent())
        .Set(ModifierComponent())
        .Set(ModifierStatusComponent())
        .Set(std::move(modelComp))
        .Set(MaterialComponent())
        .Set(JoltRigidbodyComponent{
                    .motionType = JPH::EMotionType::Dynamic,
                    .objectLayer = PhysicsLayers::MOVING,
                    .restitution = 0.0f,   // 反発係数（跳ねない）
                    .friction = 0.05f,   // 摩擦係数（よく滑るように低く）
                    .gravityFactor = 0
                    })
        .Set(JoltCharacterConfigComponent{ .walkSpeed = 7.0f, .jumpSpeed = 8.0f })
        .Set(JoltBoxColliderComponent{ .halfExtent = {1.0f, 1.4f, 1.0f} });

    // 3. 相互リンクの構築 (IDの書き込み)
    // 見た目 -> 物理
    FPSPlayer.Patch<FPSPlayerComponent>([&](FPSPlayerComponent& fps)
        {
        fps.physicsBodyID = FPSPlayer.id; 
        });

}

void SceneInitializer::SpawnTPSPlayer(CCL::ECS::Core::World& world)
{
    //──────────────────────────────────────────
    //1. 床の生成 (Box)
    //──────────────────────────────────────────
    const auto floorArch = ArchetypeHelper::Generate<
        TransformComponent,
        ModelComponent,
        MaterialComponent,
        PrimitiveComponent,
        JoltRigidbodyComponent,
        JoltBoxColliderComponent,
        EnemyTag
    >();
    
    auto floor = EntityFactory::SpawnNamed(world, "Floor_Box", floorArch);
    
    floor.Set(TransformComponent{ .rotation = {0.0f, 0.0f, 0.0f, 1.0f},.position = {0, 0, 0},.scale{1.0f,1.0f,1.0f} })
         .Set(JoltRigidbodyComponent{ .motionType = JPH::EMotionType::Static, .objectLayer = PhysicsLayers::NON_MOVING })
         .Set(JoltBoxColliderComponent{ .halfExtent = {50.0f, 1.0f, 50.0f} })
         .Set(PrimitiveComponent{ .type = PrimitiveType::Box, .size = {50.0f, 1.0f, 50.0f}, .color = {0.5f, 0.5f, 0.5f, 1.0f} })
         .Set(EnemyTag());

        auto playerHitArch = ArchetypeHelper::Generate<
            TransformComponent,
            JoltCapsuleColliderComponent,
            JoltRigidbodyComponent,
            PhysicsTag
        >();

    auto playerHit = EntityFactory::SpawnNamed(world, "PhysicsBox", playerHitArch);

    // ★修正: JoltRigidbodyComponentの定義順（motionType -> objectLayer -> restitution -> friction）を厳守
    playerHit.Set(TransformComponent{ .rotation = {0.0f, 0.0f, 0.0f, 1.0f},.position = {0, 0, 0},.scale{1.0f,1.0f,1.0f} })
        .Set(JoltCapsuleColliderComponent{ })
        .Set(JoltRigidbodyComponent{
            .motionType = JPH::EMotionType::Dynamic,
            .objectLayer = PhysicsLayers::MOVING,
            .restitution = 0.0f,   // 反発係数（跳ねない）
            .friction = 0.05f   // 摩擦係数（よく滑るように低く）
            });



    const auto TPSPlayerArch = ArchetypeHelper::Generate<
        TransformComponent,
        TPSPlayerComponent,
        TPSPlayerStateComponent,
        PlayerViewComponent,
        StaminaComponent,
        ModifierComponent,
        ModifierStatusComponent,
        ModelComponent,
        MaterialComponent,
        JoltCharacterConfigComponent, // 物理設定
        JoltRigidbodyComponent // 衝突形状
    >();

    auto TPSPlayer = EntityFactory::SpawnPrefab(world, "Data/Prefabs/Player/JammoPlayer.json");


    playerHit.Set(PhysicsTag{ .parentID = TPSPlayer.id });

    // 4. カメラのECS化とプレハブ生成
    EntityFactory::SpawnPrefab(world, "Data/Prefabs/Camera/TPSCamera.json");
    EntityFactory::SpawnPrefab(world, "Data/Prefabs/Camera/FPSCamera.json");
    EntityFactory::SpawnPrefab(world, "Data/Prefabs/Object/HideSpot.json");
    EntityFactory::SpawnPrefab(world, "Data/Prefabs/World/Sponza.json");
    auto allEnt = world.View<TransformComponent>();

}

// ※あなたのプロジェクトにおけるDroneComponent等の正しいパスをインクルードしてください
#include "Game/Logics/AI/BehaviorTree/Drone/DroneComponent.h"
#include "Game/Logics/AI/BehaviorTree/Data/BehaviorTreeComponents.h"


void SceneInitializer::SpawnBossAndDronesTest(CCL::ECS::Core::World& world, int TOTAL_DRONES)
{
    EntityFactory::SpawnPrefab(world, "Assets/Prefabs/Camera/FreeCamera.json");
    EntityFactory::SpawnPrefab(world, "Assets/Prefabs/World/ExampleStage.json");
    EntityFactory::SpawnPrefab(world, "Assets/Prefabs/Enemy/DroneTarget.json");

    // 1. ボスの生成（中央に配置）
    // ※Boss.json がない場合は適当なモデルのプレハブパスに書き換えてください
    EntityID bossId = Prefab::SpawnPrefab(world, "Assets/Prefabs/Enemy/BossEnemy.json", { 0.0f, 0.0f, 0.0f });
    auto bossRef = CCL::ECS::Core::EntityRef(&world, bossId);


    // 2. ドローン群の生成
    for (uint16_t i = 0; i < TOTAL_DRONES; ++i) {

        // ドローンの生成（ボスの少し上にSpawn）
        // ※Drone.json がない場合は、適当なキューブ等のプレハブを指定してください
        EntityID droneId = Prefab::SpawnPrefab(world, "Assets/Prefabs/Enemy/Drone.json", { 0.0f, 5.0f, 0.0f });
        auto droneRef = CCL::ECS::Core::EntityRef(&world, droneId);

        // 操り人形としての「記憶（DroneComponent）」を初期化して付与
        DroneComponent droneComp;
        droneComp.ownerBossId = bossId;       // 親は先ほど作ったボス
        droneComp.localIndex = i;             // 自分が何番目のドローンか（重要！）
        droneComp.totalDrones = TOTAL_DRONES; // 全体の数（360度を等分するために必要）
        droneComp.orbitRadius = 10.0f;        // 旋回半径を10mに設定
        droneComp.moveSpeed = 40.0f;          // 突撃時の最高速度

        // コンポーネントをアタッチ
        droneRef.Set(droneComp);
    }

}

void SceneInitializer::SpawnLightStartSet(CCL::ECS::Core::World &world, ID3D11Device *device)
{
    // ライティングセットアップ
    Prefab::SpawnPrefab(world, "Assets/Prefabs/Light/LightingSetup.json");
}


void SceneInitializer::SpawnAnimationTest(CCL::ECS::Core::World& world, ID3D11Device* device)
{
    EntityFactory::SpawnPrefab(world, "Assets/Prefabs/Camera/FreeCamera.json");
    EntityFactory::SpawnPrefab(world, "Assets/Prefabs/World/ExampleStage.json");
    EntityFactory::SpawnPrefab(world, "Assets/Prefabs/AnimationModel.json");
   
}

void SceneInitializer::SpawnShaderTest(CCL::ECS::Core::World& world, ID3D11Device* device)
{
    EntityFactory::SpawnPrefab(world, "Assets/Prefabs/Camera/FreeCamera.json");
    EntityFactory::SpawnPrefab(world, "Assets/Prefabs/Sponza.json");
    EntityFactory::SpawnPrefab(world, "Assets/Prefabs/PBRModel.json");
}


#include <cmath>

void SceneInitializer::SpawnPhysicsAvalancheTest(
    CCL::ECS::Core::World& world, ID3D11Device* device, int count)
{
    // 1. アーキタイプの生成
    const auto staticArch = CCL::ECS::ArchetypeHelper::Generate<
        TransformComponent,
        PrimitiveComponent,
        JoltRigidbodyComponent,
        JoltBoxColliderComponent
    >();

    const auto dynamicArch = CCL::ECS::ArchetypeHelper::Generate<
        TransformComponent,
        PrimitiveComponent,
        JoltRigidbodyComponent,
        JoltBoxColliderComponent
    >();

    // -------------------------------------------------------------
    // 2. 巨大な斜面の生成（受け止める床）
    // -------------------------------------------------------------
    auto slope = EntityFactory::SpawnNamed(world, "Slope", staticArch);

    // X軸周りに20度回転させたクォータニオンを作成
    DirectX::XMVECTOR q = DirectX::XMQuaternionRotationAxis(
        DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f),
        DirectX::XMConvertToRadians(20.0f)
    );
    DirectX::XMFLOAT4 rot;
    DirectX::XMStoreFloat4(&rot, q);

    slope.Set(TransformComponent{ .rotation = rot,.position = {0, -10, 30},.scale{1.0f,1.0f,1.0f} })
        .Set(PrimitiveComponent{ .type = PrimitiveType::Box, .size = {120.0f, 3.0f, 120.0f}, .color = {0.3f, 0.3f, 0.3f, 1.0f} })
        .Set(JoltBoxColliderComponent{ .halfExtent = {120.0f, 3.0f, 120.0f} }) // sizeの半分
        .Set(JoltRigidbodyComponent{
            .motionType = JPH::EMotionType::Static,
            .objectLayer = PhysicsLayers::NON_MOVING,
            .friction = 0.5f
            });

    // -------------------------------------------------------------
    // 3. 上空から降り注ぐキューブ群の生成
    // -------------------------------------------------------------
    // count(例: 50,000)の立方根をとって、3Dのグリッド状に配置する
    int sideSize = static_cast<int>(std::cbrt(count));
    float spacing = 5.0f; // 1.0fのキューブに対して少し隙間を空ける
    float offset = (sideSize * spacing) / 2.0f;

    std::mt19937 gen(12345);
    std::uniform_real_distribution<float> colorDist(0.5f, 1.0f);

    int spawned = 0;
    for (int y = 0; y < sideSize && spawned < count; ++y) {
        for (int x = 0; x < sideSize && spawned < count; ++x) {
            for (int z = 0; z < sideSize && spawned < count; ++z) {

                auto cube = EntityFactory::SpawnNamed(world, "PhysicsCube", dynamicArch);

                float posX = (x * spacing) - offset;
                float posY = 160.0f + (y * spacing); // 上空に配置
                float posZ = (z * spacing) - offset;

                // ランダムな色（視覚的に個体を見分けやすくする）
                DirectX::XMFLOAT4 color = { colorDist(gen), colorDist(gen), 1.0f, 1.0f };

                cube.Set(TransformComponent{ .rotation = {0,0,0,1},.position = {posX, posY, posZ},.scale{1.0f,1.0f,1.0f} })
                    .Set(PrimitiveComponent{ .type = PrimitiveType::Box, .size = {1.0f, 1.0f, 1.0f}, .color = color })
                    .Set(JoltBoxColliderComponent{ .halfExtent = {1.0f, 1.0f, 1.0f} })
                    .Set(JoltRigidbodyComponent{
                        .motionType = JPH::EMotionType::Dynamic,
                        .objectLayer = PhysicsLayers::MOVING,
                        .restitution = 0.2f, // 少し弾む
                        .friction = 0.3f
                        });

                spawned++;
            }
        }
    }
}

#include "Game/Logics/Test/ZeroGDebrisComponent.h"

void SceneInitializer::SpawnBossVFXTest(CCL::ECS::Core::World& world)
{
    // ボス中心座標
    DirectX::SimpleMath::Vector3 bossCenter(0.0f, 20.0f, 0.0f);

    std::mt19937 gen(777);
    std::uniform_real_distribution<float> distAngle(0.0f, DirectX::XM_2PI);
    std::uniform_real_distribution<float> dist1_0(0.0f, 1.0f);
    std::uniform_real_distribution<float> distRadius(40.0f, 100.0f); // アリーナの広さ
    std::uniform_real_distribution<float> distHeight(30.0f, 70.0f);

    // ========================================================
    // 1. 瓦礫の波紋 (ZeroG Debris) - 約500個
    // ========================================================
    auto debrisArch = ArchetypeHelper::Generate<TransformComponent, PrimitiveComponent, ZeroGDebrisComponent>();
    for (int i = 0; i < 500; ++i) {
        auto e = EntityFactory::SpawnNamed(world, "Debris", debrisArch);

        float angle = distAngle(gen);
        float radius = distRadius(gen);
        float baseY = distHeight(gen);

        e.Set(TransformComponent{
            .position = { bossCenter.x + std::cos(angle) * radius, baseY, bossCenter.z + std::sin(angle) * radius },
            .scale = {0.5f, 0.5f, 0.5f}
            });
        e.Set(PrimitiveComponent{ .type = PrimitiveType::Box, .color = {1.0f, 1.0f, 1.0f, 1.0f} });

        e.Set(ZeroGDebrisComponent{
            .baseY = baseY,
            .distanceFromCenter = radius,
            .floatAmplitude = 2.0f + dist1_0(gen) * 2.0f,
            .floatFrequency = 1.0f,
            .waveSpeed = 1.0f,
            .rotationAxis = { dist1_0(gen), dist1_0(gen), dist1_0(gen) },
            .rotationSpeed = dist1_0(gen) * 2.0f,
            .timeAcc = 0.0f
            });
    }
}