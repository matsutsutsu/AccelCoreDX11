// PlayerHidingSystem.cpp
#include "PlayerHidingSystem.h"
#include "ECS/Core/CCL_World.h"
#include <DirectXMath.h>
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"
#include <memory>
#include "Engine/Physics/IPhysicsAPI.h"
#include "Engine/Graphics/Camera.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "FPSPlayerComponent.h"

#include "Game/Logic/System/HidingComponents.h"
#include "Engine/GamePlay/Camera/VirtualCameraComponents.h"

using namespace DirectX;
using namespace CCL::ECS;

// ヘルパー：オブジェクトのTransform行列を考慮してローカルオフセットをワールド座標へ変換
XMVECTOR GetWorldPosFromOffset(const TransformComponent* tc, const XMFLOAT3& offset) 
{
    if (!tc) return XMVectorZero(); // セーフティ

    XMMATRIX world = XMMatrixTransformation(
        g_XMZero, g_XMIdentityR3,
        XMLoadFloat3(&tc->scale),    // -> を使用
        g_XMZero,
        XMLoadFloat4(&tc->rotation), // -> を使用
        XMLoadFloat3(&tc->position)  // -> を使用
    );
    return XMVector3TransformCoord(XMLoadFloat3(&offset), world);
}


void PlayerHidingSystem::Update(float dt) {

    if (!_world->HasResource<std::shared_ptr<IPhysicsAPI>>()) return;
    auto physics = _world->GetResource<std::shared_ptr<IPhysicsAPI>>();

    Camera* mainCamera = _world->HasResource<Camera*>() ? _world->GetResource<Camera*>() : nullptr;
    if (!mainCamera) return;

    ForEachWithID([&](EntityID pID, TransformComponent& pTrans, FPSPlayerComponent& fps) {

        // --- 1. 隠れていない場合：潜伏開始判定 (Raycast) ---
        if (!_world->HasComponent<PlayerTag::HideTag>(pID)) {
            if (fps.input.isHidePressed) {
                XMMATRIX invView = XMMatrixInverse(nullptr, mainCamera->GetView());
                XMFLOAT3 rayOrigin, rayDir;
                XMStoreFloat3(&rayOrigin, invView.r[3]);
                XMStoreFloat3(&rayDir, invView.r[2]);

                PhysicsHitInfo hit = physics->RayCast(rayOrigin, rayDir, 3.0f, pID);

                if (hit.hit && _world->HasComponent<HidingSpotComponent>(hit.entity)) {
                    auto* spot = _world->GetComponent<HidingSpotComponent>(hit.entity);
                    if (!spot->isOccupied) {
                        PlayerTag::HideTag newTag;
                        newTag.spotEntity = hit.entity;
                        newTag.originalPos = pTrans.position;
                        newTag.state = PlayerTag::HideState::Entering; // 進入開始
                        newTag.lerpT = 0.0f;

                        _world->AddComponent<PlayerTag::HideTag>(pID, newTag);
                        spot->isOccupied = true;
                        fps.currentState = PlayerState::Idle;
                    }
                }
            }
            return;
        }

        // --- 2. 隠れている（進入中・潜伏中・脱出中）場合の共通処理 ---
        auto tag = _world->GetComponent<PlayerTag::HideTag>(pID);
        auto spot = _world->GetComponent<HidingSpotComponent>(tag->spotEntity);
        auto sTrans = _world->GetComponent<TransformComponent>(tag->spotEntity);
        if (!spot || !sTrans) return;

        // --- A. 位置の補間・同期ロジック ---
    // --- A. 位置の補間・同期ロジック (Entering状態) ---
        if (tag->state == PlayerTag::HideState::Entering) {
            // 待機フェーズ (Openタグ等で設定された負の値を消化)
            if (tag->lerpT < 0.0f) {
                tag->lerpT += dt;
                return;
            }
            float targetSpeed = spot->translationSpeed;
            bool isCareful = fps.input.isHideHolding; // 押し続けていれば「慎重（遅い）」

            if (isCareful) {
                targetSpeed *= 1.0f; // 長押し中はゆっくり
            }
            else {
                targetSpeed *= 2.0f; // 離していれば素早く
            }

            // 急激な速度変化を避けたい場合は、現在の速度を線形補間(Lerp)してもOK
            tag->lerpT += dt * targetSpeed;
            float t = (std::min)(tag->lerpT, 1.0f);


            float heightOffset = 0.0f;
            float pitchOffset = 0.0f;

            // タグごとのパラメータを使用してシーケンスを計算
            if (auto* under = _world->GetComponent<HidingSpotTag::Under>(tag->spotEntity)) {
                HidingMath::CalculateUnderSequence(t, under->sinkDepth, under->lookDownAngle, heightOffset, pitchOffset);
            }
            else if (auto* topIn = _world->GetComponent<HidingSpotTag::TopIn>(tag->spotEntity)) {
                HidingMath::CalculateTopInSequence(t, topIn->riseHeight, topIn->lookDownAngle, topIn->sinkDepth, heightOffset, pitchOffset);
            }

            // --- カメラへの適用 (Pitch) ---
            auto cameraEntities = _world->View<CameraBodyFPS>();
            for (EntityID camID : cameraEntities) {
                auto* camBody = _world->GetComponent<CameraBodyFPS>(camID);
                if (camBody && camBody->targetEntity == pID) {
                    // 前フレームからの変化量ではなく、現在の基準位置にオフセットを加える運用を想定
                    // (既存のカメラシステムとの兼ね合いで調整してください)
                    camBody->currentPitch += pitchOffset * dt * 8.0f;
                    break;
                }
            }

            // --- トランスフォームへの適用 ---
            XMVECTOR startPos = XMLoadFloat3(&tag->originalPos);
            XMVECTOR targetPos = GetWorldPosFromOffset(sTrans, spot->localOffset);
            XMVECTOR currentPos = XMVectorLerp(startPos, targetPos, t);

            // 高度補正の合算 (シーケンス高度 + 汎用放物線)
            float arc = sinf(t * 3.14159f) * spot->trajectoryArc;
            currentPos = XMVectorSetY(currentPos, XMVectorGetY(currentPos) + heightOffset + arc);

            XMStoreFloat3(&pTrans.position, currentPos);

            if (tag->lerpT >= 1.0f) {
                tag->state = PlayerTag::HideState::Hiding;
                tag->lerpT = 0.0f;
                // Yaw回転初期化へ...
            }
        }
        else if (tag->state == PlayerTag::HideState::Hiding) {
            XMStoreFloat3(&pTrans.position, GetWorldPosFromOffset(sTrans, spot->localOffset));

            if (fps.input.isHidePressed) {
                tag->state = PlayerTag::HideState::Exiting;
                tag->lerpT = 0.0f;

                auto cameraEntities = _world->View<CameraBodyFPS>();
                for (EntityID camID : cameraEntities) {
                    auto* camBody = _world->GetComponent<CameraBodyFPS>(camID);
                    if (camBody && camBody->targetEntity == pID) {
                        tag->initialYaw = camBody->currentYaw;
                        tag->isRotating = true;
                        tag->rotationT = 0.0f;
                        break;
                    }
                }
            }
        }
        else if (tag->state == PlayerTag::HideState::Exiting) {
            tag->lerpT += dt * spot->translationSpeed;
            float t = (std::min)(tag->lerpT, 1.0f);

            // 【重要】アニメーション（高さ・角度）を逆再生するために1.0から減算
            float revT = 1.0f - t;

            float heightOffset = 0.0f;
            float pitchOffset = 0.0f;

            // 入る時と同じ関数に「逆転した時間」を渡すことで、姿勢を元に戻す
            if (auto* under = _world->GetComponent<HidingSpotTag::Under>(tag->spotEntity)) {
                HidingMath::CalculateUnderSequence(revT, under->sinkDepth, under->lookDownAngle, heightOffset, pitchOffset);
            }
            else if (auto* topIn = _world->GetComponent<HidingSpotTag::TopIn>(tag->spotEntity)) {
                HidingMath::CalculateTopInSequence(revT, topIn->riseHeight, topIn->lookDownAngle, topIn->sinkDepth, heightOffset, pitchOffset);
            }

            // --- カメラへの適用 (Pitch) ---
            auto cameraEntities = _world->View<CameraBodyFPS>();
            for (EntityID camID : cameraEntities) {
                auto* camBody = _world->GetComponent<CameraBodyFPS>(camID);
                if (camBody && camBody->targetEntity == pID) {
                    // 出る時はピッチの変化も逆にする
                    camBody->currentPitch += pitchOffset * dt * 8.0f;
                    break;
                }
            }

            // --- トランスフォームへの適用 ---
            // A: 潜伏中のワールド位置（スポットのローカルオフセットから計算）
            XMVECTOR startPos = GetWorldPosFromOffset(sTrans, spot->localOffset);

            XMVECTOR targetPos = XMLoadFloat3(&tag->originalPos);

            // 線形補間 (t=0 で潜伏位置、t=1 で元の位置)
            XMVECTOR currentPos = XMVectorLerp(startPos, targetPos, t);

            // 高度補正 (放物線は t=0.5 で最大になるのでそのまま t を使用)
            float arc = sinf(t * 3.14159f) * spot->trajectoryArc;
            currentPos = XMVectorSetY(currentPos, XMVectorGetY(currentPos) + heightOffset + arc);

            XMStoreFloat3(&pTrans.position, currentPos);

            if (tag->lerpT >= 1.0f && !tag->isRotating) {
                spot->isOccupied = false;
                _world->RequestRemoveComponent<PlayerTag::HideTag>(pID);
                // 必要であればここで状態を PlayerState::Normal などに戻す
            }
        }

        // --- B. カメラの回転補間 (共通) ---
        if (tag->isRotating) {
            tag->rotationT += dt * spot->camerarotationSpeed;
            float t = (std::min)(tag->rotationT, 1.0f);

            auto cameraEntities = _world->View<CameraBodyFPS>();
            for (EntityID camID : cameraEntities) {
                auto* camBody = _world->GetComponent<CameraBodyFPS>(camID);
                if (camBody && camBody->targetEntity == pID) {
                    // Enteringなら+180、Exitingなら-180 (結果的に0に戻る)
                    float targetOffset = (tag->state == PlayerTag::HideState::Exiting) ? 0 : spot->targetYawOffset;
                    camBody->currentYaw = tag->initialYaw + (targetOffset * t);

                    if (tag->rotationT >= 1.0f) tag->isRotating = false;
                }
            }
        }

        // --- C. 物理同期 (潜伏関連の全状態共通) ---
        if (fps.physicsBodyID != CCL::ECS::InvalidEntityID)
        {
            physics->SetPosition(fps.physicsBodyID, pTrans.position);
            physics->SetLinearVelocity(fps.physicsBodyID, { 0, 0, 0 });
        }
        });
}


REGISTER_LOGIC_SYSTEM(PlayerHidingSystem, Priority::LogicStage::L02_Update); // 状態更新の直後に実行