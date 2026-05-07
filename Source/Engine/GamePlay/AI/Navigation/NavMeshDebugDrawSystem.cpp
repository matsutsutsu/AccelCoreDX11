#include "NavMeshDebugDrawSystem.h"
#include "NavigationData.h"
#include <DetourNavMesh.h>
#include <DirectXMath.h>
#include <cmath> // 円の計算用

#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"
#include "Engine/Graphics/Renderer/ShapeRenderer.h"

// ★AIエージェントの情報を読み取るためにインクルード
#include "Engine/GamePlay/Transform/TransformComponent.h"
#include "Engine/GamePlay/AI/Navigation/NavAgentComponent.h"

void NavMeshDebugDrawSystem::Update(float dt)
{
    if (!isDebugVisible) return;

    if (!_world->HasResource<ShapeRenderer*>()) return;
    auto* renderer = _world->GetResource<ShapeRenderer*>();
    if (!renderer) return;

    // ========================================================================
    // 1. NavMesh（歩行可能エリア）の描画
    // ========================================================================
    if (_world->HasResource<NavigationData>()) {
        auto& navData = _world->GetResource<NavigationData>();
        if (navData.navMesh && navData.showDebugDraw) {
            const dtNavMesh* navMesh = navData.navMesh;

            for (int i = 0; i < navMesh->getMaxTiles(); ++i) {
                const dtMeshTile* tile = navMesh->getTile(i);
                if (!tile || !tile->header) continue;

                for (int j = 0; j < tile->header->polyCount; ++j) {
                    const dtPoly* p = &tile->polys[j];
                    if (p->getType() == DT_POLYTYPE_OFFMESH_CONNECTION) continue;

                    // ① ポリゴンの外周（境界線）を黒い線で描画
                    for (int k = 0; k < p->vertCount; ++k) {
                        float* v0 = &tile->verts[p->verts[k] * 3];
                        float* v1 = &tile->verts[p->verts[(k + 1) % p->vertCount] * 3];

                        // Y軸（高さ）をほんの少し上げて、地面に埋まらないようにする
                        DirectX::XMFLOAT3 p0 = { v0[0], v0[1] + 0.08f, v0[2] };
                        DirectX::XMFLOAT3 p1 = { v1[0], v1[1] + 0.08f, v1[2] };

                        // 黒い線で境界を描画
                        renderer->DrawLine(p0, p1, { 0.0f, 0.0f, 0.0f, 1.0f });
                    }

                    // ② ポリゴンの面（青い半透明）を描画
                    const dtPolyDetail* pd = &tile->detailMeshes[j];
                    for (int k = 0; k < pd->triCount; ++k) {
                        const unsigned char* t = &tile->detailTris[(pd->triBase + k) * 4];
                        DirectX::XMFLOAT3 v[3];
                        for (int m = 0; m < 3; ++m) {
                            if (t[m] < p->vertCount) {
                                float* pos = &tile->verts[p->verts[t[m]] * 3];
                                // ★ 面は線より下（+ 0.05f）
                                v[m] = { pos[0], pos[1] + 0.05f, pos[2] };
                            }
                            else {
                                float* pos = &tile->detailVerts[(pd->vertBase + t[m] - p->vertCount) * 3];
                                // ★ 面は線より下（+ 0.05f）
                                v[m] = { pos[0], pos[1] + 0.05f, pos[2] };
                            }
                        }
                        // 面を塗りつぶす
                        renderer->DrawSolidTriangle(v[0], v[1], v[2], { 0.0f, 0.5f, 1.0f, 0.4f });
                    }
                }
            }
        }
    }

    // ========================================================================
    // 2. AIキャラクターの経路と半径（太さ）の描画
    // ========================================================================
    auto view = _world->View<TransformComponent, NavAgentComponent>();
    for (auto entity : view) {
        auto* transform = _world->GetComponent<TransformComponent>(entity);
        auto* agent = _world->GetComponent<NavAgentComponent>(entity);

        // ① ウェイポイント（赤い線）の描画
        if (!agent->waypoints.empty() && agent->currentWaypointIndex < agent->waypoints.size()) {
            DirectX::XMFLOAT3 prevPos = transform->position;
            // 自分の位置から、残りのウェイポイントをすべて線で結ぶ
            for (size_t i = agent->currentWaypointIndex; i < agent->waypoints.size(); ++i) {
                DirectX::XMFLOAT3 nextPos = agent->waypoints[i];
                // 少し浮かせる
                prevPos.y += 0.2f;
                nextPos.y += 0.2f;
                renderer->DrawLine(prevPos, nextPos, { 1.0f, 0.0f, 0.0f, 1.0f }); // 赤い線
                prevPos = nextPos;
            }
        }

        // ② AIの物理的な太さ（黄色い円）の描画
        float drawRadius = 0.6f;
        int segments = 16;
        for (int i = 0; i < segments; ++i) {
            float theta1 = (i / (float)segments) * 2.0f * 3.14159f;
            float theta2 = ((i + 1) / (float)segments) * 2.0f * 3.14159f;

            DirectX::XMFLOAT3 p1 = {
                transform->position.x + cos(theta1) * drawRadius,
                transform->position.y + 0.1f, // 地面より少し浮かせる
                transform->position.z + sin(theta1) * drawRadius
            };
            DirectX::XMFLOAT3 p2 = {
                transform->position.x + cos(theta2) * drawRadius,
                transform->position.y + 0.1f,
                transform->position.z + sin(theta2) * drawRadius
            };

            renderer->DrawLine(p1, p2, { 1.0f, 1.0f, 0.0f, 1.0f }); // 黄色い線
        }
    }
}

REGISTER_RENDER_SYSTEM(NavMeshDebugDrawSystem, Priority::RenderStage::R08_Main);