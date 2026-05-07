#include "JoltBodySetupSystem.h"
#include "ECS/Core/CCL_World.h"
#include "Engine/Physics/JoltPhysicsManager.h"

#include "Engine/GamePlay/Physics/Jolt/JoltHandleComponent.h"
#include "Engine/GamePlay/Physics/Collision/JoltBoxColliderComponent.h"
#include "Engine/GamePlay/Physics/Collision/JoltSphereColliderComponent.h"
#include "Engine/GamePlay/Physics/Collision/JoltCapsuleColliderComponent.h"
#include "Engine/GamePlay/Physics/Collision/JoltMeshColliderComponent.h"
#include "Engine/GamePlay/Graphics/Core/ModelComponent.h"
#include "Engine/Graphics/Resource/ModelResource.h"


#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h> // スケール適用に必須
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <DirectXMath.h>

#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

using namespace DirectX;

namespace {
    // ヘルパー：Degree(度)から JoltのQuaternion への変換（UpdateSystemと同じもの）
    inline JPH::Quat EulerDegreeToJoltQuat(const XMFLOAT3& eulerDegree) {
        float pitch = XMConvertToRadians(eulerDegree.x);
        float yaw = XMConvertToRadians(eulerDegree.y);
        float roll = XMConvertToRadians(eulerDegree.z);
        return JPH::Quat::sEulerAngles(JPH::Vec3(pitch, yaw, roll));
    }
}

void JoltBodySetupSystem::Update(float dt)
{
    if (!_world->HasResource<JoltPhysicsManager>()) return;
    auto               &joltManager   = _world->GetResource<JoltPhysicsManager>();
    JPH::BodyInterface &bodyInterface = joltManager.GetBodyInterface();

    // ★あなたのエンジンの IfSystem の恩恵により、
    // Transform と Rigidbody を持っているエンティティ「だけ」がここに流れてくる！
    ForEachWithID([&](CCL::ECS::EntityID            id,
                      const TransformComponent     &trans,
                      const JoltRigidbodyComponent &rigid) {
        // 【唯一の除外条件】すでに実体ID(Handle)が作られているならスキップ
        if (_world->HasComponent<JoltHandleComponent>(id)) {
            return;
        }

        DirectX::XMFLOAT3 spawnPos = trans.position;
        DirectX::XMFLOAT4 spawnRot = trans.rotation;
        DirectX::XMFLOAT3 spawnScale = trans.scale;

        JPH::ShapeRefC shape = nullptr;

        // =======================================================
        // 1. Box Collider の生成
        // =======================================================
        if (auto* box = _world->GetComponent<JoltBoxColliderComponent>(id)) {
            JPH::BoxShapeSettings baseSettings(JPH::Vec3(box->halfExtent.x, box->halfExtent.y, box->halfExtent.z));
            JPH::ShapeRefC baseShape = baseSettings.Create().Get();

            // ★ オフセットと回転を適用したラッパー(包み紙)でくるむ
            JPH::RotatedTranslatedShapeSettings offsetSettings(
                JPH::Vec3(box->localOffset.x, box->localOffset.y, box->localOffset.z),
                EulerDegreeToJoltQuat(box->localRotationEuler),
                baseShape
            );
            shape = offsetSettings.Create().Get();
        }
        // =======================================================
        // 2. Sphere Collider の生成
        // =======================================================
        else if (auto* sphere = _world->GetComponent<JoltSphereColliderComponent>(id)) {
            JPH::SphereShapeSettings baseSettings(sphere->radius);
            JPH::ShapeRefC baseShape = baseSettings.Create().Get();

            // ★ 球は回転の影響を受けないため、位置ズレのみ適用
            JPH::RotatedTranslatedShapeSettings offsetSettings(
                JPH::Vec3(sphere->localOffset.x, sphere->localOffset.y, sphere->localOffset.z),
                JPH::Quat::sIdentity(),
                baseShape
            );
            shape = offsetSettings.Create().Get();
        }
        // =======================================================
        // 3. Capsule Collider の生成
        // =======================================================
        else if (auto* capsule = _world->GetComponent<JoltCapsuleColliderComponent>(id)) {
            JPH::CapsuleShapeSettings baseSettings(capsule->halfHeight, capsule->radius);
            JPH::ShapeRefC baseShape = baseSettings.Create().Get();

            // ★ オフセットと回転を適用
            JPH::RotatedTranslatedShapeSettings offsetSettings(
                JPH::Vec3(capsule->localOffset.x, capsule->localOffset.y, capsule->localOffset.z),
                EulerDegreeToJoltQuat(capsule->localRotationEuler),
                baseShape
            );
            shape = offsetSettings.Create().Get();
        }
        // =======================================================
        // 4. Mesh Collider (地形) の自動抽出と生成
        // =======================================================
        // JoltBodySetupSystem.cpp の MeshCollider ブロック
        // =======================================================
        // 4. Mesh Collider (地形) の自動抽出と生成
        // =======================================================
        else if (auto* meshCol = _world->GetComponent<JoltMeshColliderComponent>(id)) {

            auto* modelComp = _world->GetComponent<ModelComponent>(id);
            if (!modelComp || !modelComp->GetModel() || !meshCol->isEnabled) return;

            Model* model = modelComp->GetModel();
            const ModelResource* res = model->GetResource();
            if (!res) return;

            // ==============================================================
            // 親を辿って確実な「ワールド行列」を自己計算する
            // ==============================================================
            DirectX::XMMATRIX worldMat = DirectX::XMMatrixIdentity();
            CCL::ECS::EntityID currNode = id;
            std::vector<CCL::ECS::EntityID> hierarchy;

            while (currNode != CCL::ECS::InvalidEntityID && currNode != 0) {
                auto* t = _world->GetComponent<TransformComponent>(currNode);
                if (!t) break;
                hierarchy.push_back(currNode);
                CCL::ECS::EntityID nextNode = t->parentID;
                if (nextNode == currNode) break;
                currNode = nextNode;
            }

            for (auto it = hierarchy.rbegin(); it != hierarchy.rend(); ++it) {
                auto* t = _world->GetComponent<TransformComponent>(*it);
                t->UpdateMatrix(worldMat);
                worldMat = DirectX::XMLoadFloat4x4(&t->worldMatrix);
            }

            // 行列分解：絶対スケール、絶対回転、絶対座標を抽出
            DirectX::XMVECTOR vScale, vRot, vPos;
            DirectX::XMMatrixDecompose(&vScale, &vRot, &vPos, worldMat);

            spawnPos = { DirectX::XMVectorGetX(vPos), DirectX::XMVectorGetY(vPos), DirectX::XMVectorGetZ(vPos) };
            spawnRot = { DirectX::XMVectorGetX(vRot), DirectX::XMVectorGetY(vRot), DirectX::XMVectorGetZ(vRot), DirectX::XMVectorGetW(vRot) };
            spawnScale = { DirectX::XMVectorGetX(vScale), DirectX::XMVectorGetY(vScale), DirectX::XMVectorGetZ(vScale) };

            // ==============================================================
            // ★ システム内キャッシュ（辞書）
            // ==============================================================
            static std::unordered_map<const ModelResource*, JPH::ShapeRefC> shapeCache;
            JPH::ShapeRefC baseShape;

            auto it = shapeCache.find(res);
            if (it != shapeCache.end()) {
                baseShape = it->second;
            }
            else {
                JPH::VertexList joltVertices;
                JPH::IndexedTriangleList joltTriangles;

                // ==============================================================
                // ★ 究極の修正: 二重スケール(0.01倍極小化)の防止
                // そのまま抽出するとすでに0.1倍された金型ができ、さらにScaledShape(0.1)されてしまう。
                // そのため、一旦 Entity の影響を消す「単位行列(1.0倍)」を流し込んでリセットする！
                // ==============================================================
                DirectX::XMMATRIX identityMat = DirectX::XMMatrixIdentity();
                DirectX::XMFLOAT4X4 tempMat; // 一時保存用の構造体

                // 1. 単位行列でリセットする場合
                DirectX::XMStoreFloat4x4(&tempMat, identityMat);
                model->UpdateTransform(tempMat);

                for (const Model::Mesh& mesh : model->GetMeshes()) {
                    if (!mesh.data || !mesh.node) continue;
                    uint32_t vertexOffset = static_cast<uint32_t>(joltVertices.size());

                    // 純粋な FBX/GLTF のオリジナルローカル行列(Scale 1.0) が取得できる
                    DirectX::XMMATRIX nodeMat = DirectX::XMLoadFloat4x4(&mesh.node->globalTransform);

                    for (const auto& v : mesh.data->vertices) {
                        DirectX::XMVECTOR localPos = DirectX::XMVectorSet(v.position.x, v.position.y, v.position.z, 1.0f);
                        DirectX::XMVECTOR nodePos = DirectX::XMVector3TransformCoord(localPos, nodeMat);
                        joltVertices.push_back(JPH::Float3(
                            DirectX::XMVectorGetX(nodePos), DirectX::XMVectorGetY(nodePos), DirectX::XMVectorGetZ(nodePos)
                        ));
                    }

                    for (size_t i = 0; i < mesh.data->indices.size(); i += 3) {
                        joltTriangles.push_back(JPH::IndexedTriangle(
                            mesh.data->indices[i + 0] + vertexOffset,
                            mesh.data->indices[i + 2] + vertexOffset, // 反転
                            mesh.data->indices[i + 1] + vertexOffset
                        ));
                    }
                }

                if (joltTriangles.empty()) return;

                JPH::MeshShapeSettings meshSettings(joltVertices, joltTriangles);
                baseShape = meshSettings.Create().Get();
                shapeCache[res] = baseShape;

                // ★ 抽出が終わったら、描画システムが壊れないように元の世界行列（worldMat）に戻してあげる
                DirectX::XMStoreFloat4x4(&tempMat, worldMat);
                model->UpdateTransform(tempMat);
            }

            if (!baseShape) return;

            // ==============================================================
            // 組み立て: 金型(純粋な1.0倍)に、ECSの絶対Scale(0.1倍など)を適用して完成！
            // ==============================================================
            JPH::ScaledShapeSettings scaledSettings(
                baseShape,
                JPH::Vec3(spawnScale.x, spawnScale.y, spawnScale.z)
            );
            shape = scaledSettings.Create().Get();
        }

        // コライダーコンポーネントが一つも無い場合は物理実体を作れないのでスキップ
        if (shape == nullptr) return;


        // 2. BodyCreationSettings を構築
        JPH::BodyCreationSettings bodySettings;
        bodySettings.SetShape(shape);
        bodySettings.mMotionType = rigid.motionType;
        bodySettings.mObjectLayer = rigid.objectLayer;

        if (_world->HasComponent<JoltMeshColliderComponent>(id)) {
            // ★ MeshColliderは頂点に「ワールド座標」が既に焼き付いているため、剛体の原点は必ず(0,0,0)！
            // これが「昔は下にズレていた」という最初のバグの真犯人だった。
            bodySettings.mPosition = JPH::RVec3(0, 0, 0);
            bodySettings.mRotation = JPH::Quat::sIdentity();
        }
        else {
            // 他のコライダー（BoxやCapsule）はローカル形状なので今まで通り配置する
            bodySettings.mPosition = JPH::RVec3(trans.position.x, trans.position.y, trans.position.z);
            JPH::Quat joltRot(trans.rotation.x, trans.rotation.y, trans.rotation.z, trans.rotation.w);
            bodySettings.mRotation = joltRot.Normalized();
        }


        bodySettings.mRestitution = rigid.restitution;
        bodySettings.mFriction    = rigid.friction;

        // =========================================================
        //  Joltに初速と詳細設定を叩き込む！
        // =========================================================
        bodySettings.mLinearVelocity = JPH::Vec3(rigid.initialVelocity.x, rigid.initialVelocity.y, rigid.initialVelocity.z);
        bodySettings.mGravityFactor  = rigid.gravityFactor;
        bodySettings.mIsSensor       = rigid.isSensor;

        // 弾が高速すぎて壁をすり抜ける（トンネリング）現象を防ぐ最高品質の計算
        if (rigid.useCCD) {
            bodySettings.mMotionQuality = JPH::EMotionQuality::LinearCast;
        }

        // =========================================================
        // 　Jolt剛体の記憶に、ECSのEntityIDを刻み込む！
        // =========================================================
        bodySettings.mUserData = static_cast<JPH::uint64>(id);

        JPH::Body *body = bodyInterface.CreateBody(bodySettings);

        // Static(静的)なものは最初からスリープ状態で空間に追加し、無駄な計算を省く
        JPH::EActivation activation = (rigid.motionType == JPH::EMotionType::Static)
                                          ? JPH::EActivation::DontActivate
                                          : JPH::EActivation::Activate;
        bodyInterface.AddBody(body->GetID(), activation);

        // 3. ECSに「Handle（実体ID）」を登録
        // これが付与されることで、次フレームからこの生成処理はスキップされ、
        // Pull / Push / Cleanup システムがこのエンティティを認識し始める。
        JoltHandleComponent handleComp;
        handleComp.bodyID = body->GetID();
        _world->AddComponent<JoltHandleComponent>(id, handleComp);
    });
}

// 登録順序: 物理計算が始まる前の PrePhysics フェーズで剛体を生成しておく必要がある。
REGISTER_LOGIC_SYSTEM(JoltBodySetupSystem, Priority::LogicStage::L03_PrePhysics);