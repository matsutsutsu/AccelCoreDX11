#include "TPSPlayerTargetingSystem.h"
#include "TPSPlayerComponent.h"
#include "../PlayerStateComponent.h" // TagやDashParamsの定義場所
#include "Engine/Physics/IPhysicsAPI.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include <DirectXMath.h>


#include "ECS/Core/CCL_World.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"
#include "Game/Logics/Combat/StaminaComponent.h"
#include "Game/Logics/Character/Enemy/EnemyTag.h"
#include "Engine/Graphics/Core/Camera.h"
#include "Engine/GamePlay/Camera/VirtualCameraComponents.h"

using namespace DirectX;
using namespace CCL::ECS;

void TPSPlayerTargetingSystem::Update(float dt)
{
    // カメラの方向を取得（入力方向の計算用）
    Camera* mainCamera = _world->HasResource<Camera*>() ? _world->GetResource<Camera*>() : nullptr;
    XMVECTOR camForward = XMVectorSet(0, 0, 1, 0);
    XMVECTOR camRight = XMVectorSet(1, 0, 0, 0);

    if (mainCamera) {
        // 1. カメラのビュー行列（XMFLOAT4X4）を取得し、SIMDレジスタ（XMMATRIX）にロード（Load）する
        XMMATRIX viewMat = XMLoadFloat4x4(&mainCamera->GetView());

        // 2. SIMDレジスタ上で逆行列計算を行う
        XMMATRIX invView = XMMatrixInverse(nullptr, viewMat);
        camForward = XMVector3Normalize(XMVectorSetY(invView.r[2], 0.0f));
        camRight = XMVector3Normalize(XMVectorSetY(invView.r[0], 0.0f));
    }

    if (!_world->HasResource<std::shared_ptr<IPhysicsAPI>>()) return;
    auto physics = _world->GetResource<std::shared_ptr<IPhysicsAPI>>();

    ForEachWithID([&](EntityID pID,
        TransformComponent& trans,
        TPSPlayerComponent& tps,
        TPSPlayerStateComponent& state,
        PlayerStateTag::IsLockOnTag& lockOnTag) 
        {
            auto* s = std::get_if<StateTargeting>(&state.activeState);
            if (!s) return;

            // ボタンが押された瞬間に一括ターゲットを実行（テスト用）
            if (tps.input.isTargetDecided)
            {
                // 1. ターゲット候補（EnemyTagを持つEntity）をすべて収集
                struct TargetCandidate {
                    EntityID entity;
                    float distanceToPlayer;
                };
                std::vector<TargetCandidate> candidates;

                // ワールド内のEnemyTagを持つEntityを全検索
                // ※Viewを使用して高速に EnemyTag と TransformComponent を持つものを抽出
                auto enemies = _world->View<EnemyTag, TransformComponent>();
                XMVECTOR playerPos = XMLoadFloat3(&trans.position);

                for (auto eID : enemies) {
                    auto* eTrans = _world->GetComponent<TransformComponent>(eID);
                    XMVECTOR ePos = XMLoadFloat3(&eTrans->position);
                    float dist = XMVectorGetX(XMVector3Length(XMVectorSubtract(ePos, playerPos)));

                    // 最大射程（maxRange）内にあるものだけ候補に入れる
                    if (dist <= s->config.maxRange) {
                        candidates.push_back({ eID, dist });
                    }
                }

                // 2. プレイヤーに近い順にソート
                std::sort(candidates.begin(), candidates.end(), [](const TargetCandidate& a, const TargetCandidate& b) {
                    return a.distanceToPlayer < b.distanceToPlayer;
                    });

                // 3. 射程が許す限り、近い順にターゲット登録
                s->targetCount = 0;
                s->remainingRange = s->config.maxRange;
                XMVECTOR lastPos = playerPos;

                for (const auto& cand : candidates) {
                    if (s->targetCount >= StateTargeting::MAX_TARGETS) break;

                    auto* targetTrans = _world->GetComponent<TransformComponent>(cand.entity);
                    XMVECTOR currentPos = XMLoadFloat3(&targetTrans->position);

                    // 「直前のターゲット」からの距離を計算（一筆書きコスト）
                    float stepDist = XMVectorGetX(XMVector3Length(XMVectorSubtract(currentPos, lastPos)));

                    if (s->remainingRange >= stepDist) {
                        s->targets[s->targetCount] = cand.entity;
                        s->targetCount++;
                        s->remainingRange -= stepDist;
                        lastPos = currentPos; // 次のターゲットへの起点更新
                    }
                    else {
                        // これ以上遠くの敵には届かない
                        break;
                    }
                }
            }
        });
    //テスト用にすべてのEnemyをロックオンしています
        //{
        //// 現在のステートが StateLockOn であることを確認
        //auto* s = std::get_if<StateTargeting>(&state.activeState);
        //if (!s) return;

        //// --- 1. Raycastの準備 (ハイドシステムの手法) ---
        // XMMATRIX invView = XMMatrixInverse(nullptr, mainCamera->GetView());
        // XMFLOAT3 rayOrigin, rayDir;
        // XMStoreFloat3(&rayOrigin, invView.r[3]);
        // XMStoreFloat3(&rayDir, invView.r[2]);

        //// レイを飛ばす（射程は残り射程分）
        //PhysicsHitInfo hit = physics->RayCast(rayOrigin, rayDir, s->remainingRange,tps.physicsBodyID);
        //// EnemyTag または EnemyComponent を持っているかチェック
        //if (hit.hit && _world->HasComponent<EnemyTag>(hit.entity)) {

        //    // 選択ボタンが押された瞬間に配列へ追加
        //    if (tps.input.isTargetDecided) { // ここは決定用の入力に変えてもOK

        //        // 重複チェック
        //        bool alreadyTargeted = false;
        //        for (uint32_t i = 0; i < s->targetCount; ++i) {
        //            if (s->targets[i] == hit.entity) {
        //                alreadyTargeted = true;
        //                break;
        //            }
        //        }

        //        if (!alreadyTargeted && s->targetCount < StateTargeting::MAX_TARGETS)
        //        {

        //            // --- 3. 距離計算ロジック ---
        //            XMVECTOR targetPos = XMLoadFloat3(&_world->GetComponent<TransformComponent>(hit.entity)->position);
        //            XMVECTOR prevPos;

        //            if (s->targetCount == 0) {
        //                // 1体目：プレイヤーとの距離
        //                prevPos = XMLoadFloat3(&trans.position);
        //            }
        //            else {
        //                // 2体目以降：直前のターゲットとの距離
        //                prevPos = XMLoadFloat3(&_world->GetComponent<TransformComponent>(s->targets[s->targetCount - 1])->position);
        //            }

        //            float distance = XMVectorGetX(XMVector3Length(XMVectorSubtract(targetPos, prevPos)));

        //            // 射程内であれば追加
        //            if (s->remainingRange >= distance) {
        //                s->targets[s->targetCount] = hit.entity;
        //                s->targetCount++;
        //                s->remainingRange -= distance; // 残り射程を減らす

        //                // 成功時、何らかのフィードバック（SE等）があると良い
        //            }
        //        }
        //    }
        //}
        //});
}

REGISTER_LOGIC_SYSTEM(TPSPlayerTargetingSystem, Priority::LogicStage::L02_Update); // 状態更新の直後に実行
