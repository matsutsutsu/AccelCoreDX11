#include "DistractionItemDebugDrawSystem.h"
#include "ECS/Core/CCL_World.h"
#include "Engine/Graphics/Renderer/ShapeRenderer.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

#include <SimpleMath.h>

using namespace DirectX::SimpleMath;
using namespace CCL::ECS;

void DistractionItemDebugDrawSystem::Update(float dt) {
    if (!isDebugVisible) return;

    if (!_world || !_world->HasResource<ShapeRenderer*>()) return;
    auto renderer = _world->GetResource<ShapeRenderer*>();
    if (!renderer) return;

    // AIの聴覚(マゼンタ)と見分けるため、音の波紋は「シアン(水色)」で描画する
    DirectX::XMFLOAT4 soundWaveColor = { 0.0f, 0.0f, 0.0f, 0.8f };

    ForEach([&](const TransformComponent& transform, const DistractionItemComponent& item) {

        // 音が届く範囲が0以下の場合は描画しない
        if (item.volumeRadius <= 0.0f) return;

        Vector3 pos = transform.position;

        // ---------------------------------------------------------
        // 3D空間の球体を表現するため、3つの直交する円を描画する
        // ---------------------------------------------------------

        // 1. 水平な円 (XZ平面) - 地面に広がる波紋
        Quaternion rotXZ = Quaternion::CreateFromAxisAngle(Vector3::UnitX, DirectX::XM_PIDIV2);
        renderer->DrawWireframeCircle(pos, item.volumeRadius, soundWaveColor, rotXZ);

        // 2. 垂直な円 (XY平面)
        Quaternion rotXY = Quaternion::Identity;
        renderer->DrawWireframeCircle(pos, item.volumeRadius, soundWaveColor, rotXY);

        // 3. 垂直な円 (YZ平面)
        Quaternion rotYZ = Quaternion::CreateFromAxisAngle(Vector3::UnitY, DirectX::XM_PIDIV2);
        renderer->DrawWireframeCircle(pos, item.volumeRadius, soundWaveColor, rotYZ);

        });
}

// デバッグ描画なので、メインの描画フェーズに登録
REGISTER_RENDER_SYSTEM(DistractionItemDebugDrawSystem, Priority::RenderStage::R08_Main);