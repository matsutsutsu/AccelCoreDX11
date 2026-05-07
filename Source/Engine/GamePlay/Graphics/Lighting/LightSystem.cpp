#include "LightSystem.h"

// 実装で必要なヘッダ群
#include "ECS/Core/CCL_World.h"
#include "Engine/Graphics/Core/Light.h"
#include "Game/Core/AllComponents.h" // 必要に応じて個別のコンポーネントに細分化推奨

// システムの実行順序の定義ヘッダー
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

LightSystem::LightSystem() : SystemBase("LightSystem") {}

std::vector<CCL::ECS::TypeID> LightSystem::GetReadTypes() const
{
    return {CCL::ECS::TypeInfo<DirectionalLightComponent>::ID(),
        CCL::ECS::TypeInfo<PointLightComponent>::ID(),
        CCL::ECS::TypeInfo<SpotLightComponent>::ID(),
        CCL::ECS::TypeInfo<TransformComponent>::ID()};
}

void LightSystem::Update(float dt)
{
    // Worldからライトマネージャーを取得
    if (!_world || !_world->HasResource<LightManager *>()) return;
    LightManager *lightManager = _world->GetResource<LightManager *>();
    if (!lightManager) return;

    // 1. マネージャーの状態をリセット
    // (Systemを分けてしまうと、これがうまく制御できなくなります)
    lightManager->Clear();

    // 2. DirectionalLightComponent を持っているエンティティを収集
    auto dirView = _world->View<DirectionalLightComponent, TransformComponent>();
    for (auto entity : dirView) {
        auto *lightComp = _world->GetComponent<DirectionalLightComponent>(entity);
        auto *transComp = _world->GetComponent<TransformComponent>(entity);

        if (!lightComp || !transComp) continue;

        // グラフィックス用の構造体を作成してデータを詰める
        DirectionalLight gfxLight;
        gfxLight.color     = lightComp->color; // アロー演算子(->)を使う
        gfxLight.intensity = lightComp->intensity;

        // 環境光のコピー
        gfxLight.skyColor    = lightComp->skyColor;
        gfxLight.groundColor = lightComp->groundColor;

        // 向きは Transform の回転から計算して詰める
        gfxLight.direction = CalculateDirection(transComp->rotation);

        // 1つの構造体として渡す
        lightManager->SetDirectionalLight(gfxLight);
    }

    // --- 2. Point Light ---
    auto pointView = _world->View<PointLightComponent, TransformComponent>();
    for (auto entity : pointView) {
        auto *lightComp = _world->GetComponent<PointLightComponent>(entity);
        auto *transComp = _world->GetComponent<TransformComponent>(entity);

        if (!lightComp || !transComp) continue;

        // グラフィックス用の構造体を作成
        PointLights gfxLight; // Light.h で定義されている構造体名
        gfxLight.color                = lightComp->color;
        gfxLight.intensity            = lightComp->intensity;
        gfxLight.range                = lightComp->range;
        gfxLight.constantAttenuation  = lightComp->constant;
        gfxLight.linearAttenuation    = lightComp->linear;
        gfxLight.quadraticAttenuation = lightComp->quadratic;

        // 位置は Transform からコピー
        gfxLight.position = transComp->GetWorldPosition();

        lightManager->AddPointLight(gfxLight);
    }

    // --- 3. Spot Light ---
    auto spotView = _world->View<SpotLightComponent, TransformComponent>();
    for (auto entity : spotView) {
        auto *lightComp = _world->GetComponent<SpotLightComponent>(entity);
        auto *transComp = _world->GetComponent<TransformComponent>(entity);

        if (!lightComp || !transComp) continue;

        SpotLights gfxLight;
        gfxLight.color     = lightComp->color;
        gfxLight.intensity = lightComp->intensity;
        gfxLight.range     = lightComp->range;
        gfxLight.innerCos  = lightComp->innerCos;
        gfxLight.outerCos  = lightComp->outerCos;

        // 位置と向きを Transform からコピー
        gfxLight.position  = transComp->GetWorldPosition();
        gfxLight.direction = CalculateDirection(transComp->rotation);

        lightManager->AddSpotLight(gfxLight);
    }

    // 5. GPUへ転送（Bind）は ModelRenderer::Render の中で呼んでもいいですが、
    //    ここで準備完了としておくのが責務として綺麗
    //    (※Bindに必要なContextが無い場合は、ここは省略してRenderer側で呼んでもOK)
}

DirectX::XMFLOAT3 LightSystem::CalculateDirection(const DirectX::XMFLOAT4 &rotation)
{
    using namespace DirectX;
    XMVECTOR q       = XMLoadFloat4(&rotation);
    XMVECTOR forward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    XMVECTOR dir     = XMVector3Rotate(forward, q);

    DirectX::XMFLOAT3 result;
    XMStoreFloat3(&result, dir);
    return result;
}

// ==========================================
// マクロは必ず .cpp の末尾に1回だけ書く
// ==========================================
REGISTER_RENDER_SYSTEM(LightSystem, Priority::RenderStage::R02_Environment);