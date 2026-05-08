#include "TrailSystem.h"
#include "ECS/Core/CCL_World.h"
#include "Engine/Graphics/Renderer/RenderQueue.h"
#include "Engine/Graphics/Core/Graphics.h"
#include "Engine/Platform/Logger.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"
#include "Engine/Platform/Logger.h"

using namespace DirectX;

TrailSystem::TrailSystem() : IfSystem("TrailSystem") {}

void TrailSystem::Update(float dt)
{
#ifdef TRACY_ENABLE
    ZoneScopedN("System: Trail");
#endif

    // 現在のフレームの RenderQueue を取得
    auto& renderQueue = RenderQueue::Instance();

    ForEach([dt, &renderQueue](TrailComponent& trail, const TransformComponent& trans) {

		// テクスチャの遅延ロード（必要なときに初めてロードする）
        if (!trail.textureHandle.IsValid() && !trail.texturePath.empty()) {
            trail.textureHandle = ResourceManager::Instance().LoadTexture(trail.texturePath.c_str());
        }

        // =======================================================
        // 1. ロジック更新 (Update)
        // =======================================================
        if (trail.count > 0 || trail.isEmitting) {
            // 寿命更新
            for (int i = 0; i < trail.count; ++i) {
                int index = (trail.headIndex - i + TrailComponent::MAX_TRAIL_POINTS) % TrailComponent::MAX_TRAIL_POINTS;
                trail.history[index].age += dt;
            }

            // 寿命切れ（古い点）の自動削除
            while (trail.count > 0) {
                int tailIndex = (trail.headIndex - trail.count + 1 + TrailComponent::MAX_TRAIL_POINTS) % TrailComponent::MAX_TRAIL_POINTS;
                if (trail.history[tailIndex].age > trail.lifeTime) {
                    trail.count--;
                }
                else {
                    break;
                }
            }

            // エミット中の頂点追加と追従
            if (trail.isEmitting) {
                XMMATRIX worldMat = XMLoadFloat4x4(&trans.worldMatrix);
                XMVECTOR baseLocal = XMLoadFloat3(&trail.localBasePos);
                XMVECTOR tipLocal = XMLoadFloat3(&trail.localTipPos);
                XMVECTOR baseWorld = XMVector3TransformCoord(baseLocal, worldMat);
                XMVECTOR tipWorld = XMVector3TransformCoord(tipLocal, worldMat);

                XMFLOAT3 currentBase, currentTip;
                XMStoreFloat3(&currentBase, baseWorld);
                XMStoreFloat3(&currentTip, tipWorld);

                if (trail.count == 0) {
                    trail.AddPoint(currentBase, currentTip);
                }
                else {
                    const auto& lastPoint = trail.GetLatestPoint();
                    XMVECTOR lastBase = XMLoadFloat3(&lastPoint.basePos);
                    XMVECTOR diff = XMVectorSubtract(baseWorld, lastBase);
                    float dist = XMVectorGetX(XMVector3Length(diff));

                    if (dist >= trail.minVertexDistance) {
                        trail.AddPoint(currentBase, currentTip);
                    }
                    else {
                        trail.history[trail.headIndex].basePos = currentBase;
                        trail.history[trail.headIndex].tipPos = currentTip;
                    }
                }
            }
        }

        // =======================================================
        // 2. 描画キューへの登録 (Submit)
        // =======================================================
        // ポリゴンが形成できる（点が2つ以上ある）場合のみ、描画コマンドを発行する
        if (trail.count >= 2) {
            TrailCommand cmd;
            cmd.trailData = &trail;
            renderQueue.SubmitTrail(cmd);
        }
        });
}

REGISTER_LOGIC_SYSTEM(TrailSystem, Priority::RenderStage::R08_Main);