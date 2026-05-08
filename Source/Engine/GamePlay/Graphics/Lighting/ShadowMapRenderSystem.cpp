#include "ShadowMapRenderSystem.h"
#include "ECS/Core/CCL_World.h"
#include "Engine/GamePlay/Graphics/Lighting/ShadowMapConfigComponent.h"
#include "Engine/Graphics/Core/Camera.h"
#include "Engine/Graphics/Core/Light.h"
#include "Engine/Graphics/Renderer/ModelRenderer.h"
#include "Engine/Graphics/Resource/Model.h"
#include "Engine/Graphics/Renderer/RenderQueue.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

// ShadowMapリソースへのアクセス用
#include "Engine/Graphics/Shader/Pass/ShadowMap.h"
#include "Engine/Graphics/Shader/Pass/ShadowUtils.h"

using namespace CCL::ECS;
using namespace DirectX;



ShadowMapRenderSystem::ShadowMapRenderSystem()
    : IfSystem("ShadowMapRenderSystem")
{
}

void ShadowMapRenderSystem::Update(float dt)
{
    // シャドウマップへの描画
    RenderShadowMap();
}

void ShadowMapRenderSystem::RenderShadowMap()
{
    // 1. 必要なリソースの取得
    if (!_world->HasResource<ShadowMap*>() || !_world->HasResource<Camera*>() || !_world->HasResource<LightManager*>()) return;
    
    ShadowMap* shadowMap = _world->GetResource<ShadowMap*>();
    Camera* camera = _world->GetResource<Camera*>();
    LightManager* lightManager = _world->GetResource<LightManager*>();

    // 2. View を使ったデータの同期 (Single Source of Truth)
    // テンプレートシグネチャに縛られず、設定コンポーネントだけを検索してパラメータを上書きする
    auto view = _world->View<ShadowMapConfigComponent>();
    for (auto entity : view) {
        auto* config = _world->GetComponent<ShadowMapConfigComponent>(entity);
        if (config) {
            shadowMap->params.shadowBias = config->shadowBias;

            shadowMap->params.normal_bias = config->normalBiasMultiplier; // ★追加

            // ★追加: ディレクショナルライトの方向を取得して渡す
            shadowMap->params.light_direction = lightManager->GetDirectionalLight().direction;

            shadowMap->params.shadowColor = config->shadowColor;
            shadowMap->params.cascadeSplits[0] = config->cascadeSplits.x;
            shadowMap->params.cascadeSplits[1] = config->cascadeSplits.y;
            shadowMap->params.cascadeSplits[2] = config->cascadeSplits.z;
            break; // 設定を持つエンティティは1つだけのはずなので、見つけたら即終了（高速化）
        }
    }

  
    // ================================================================
    //  カスケードの行列を計算し、交差判定用のOBBを3つ作成する
    // ================================================================
    DirectX::XMMATRIX lightVPs[3];
    CalculateCascadeMatrices(
        DirectX::XMLoadFloat4x4(&camera->GetView()),
        camera->GetFovY(), 
        camera->GetAspect(), 
        lightManager->GetDirectionalLight().direction, 
        shadowMap->params.cascadeSplits, 
        lightVPs
    );

    DirectX::BoundingOrientedBox cascadeOBBs[3];
    // DirectXのNDC空間の箱 (X,Yは -1~1, Zは 0~1) => Center(0,0,0.5), Extents(1,1,0.5)
    DirectX::BoundingBox ndcBox(DirectX::XMFLOAT3(0.0f, 0.0f, 0.5f), DirectX::XMFLOAT3(1.0f, 1.0f, 0.5f));
    
    for(int i = 0; i < 3; ++i) {
        DirectX::BoundingOrientedBox::CreateFromBoundingBox(cascadeOBBs[i], ndcBox);
        // ライト行列の「逆行列」を掛けることで、NDCの立方体をワールド空間の傾いた箱(OBB)に逆変換する
        cascadeOBBs[i].Transform(cascadeOBBs[i], DirectX::XMMatrixInverse(nullptr, lightVPs[i]));
    }


    auto& renderQueue = RenderQueue::Instance();

    // ★修正: 引数に bounds を追加
    ForEach([&](const TransformComponent &trans, const ModelComponent &model, const BoundingBoxComponent &bounds) {
        if (!model.GetModel()) return;

        // ================================================================
        // 3つのカスケード箱とモデルのAABBの交差判定を行い、マスクを作る
        // ================================================================
        uint8_t cascadeMask = 0;
        if (cascadeOBBs[0].Intersects(bounds.worldAABB)) cascadeMask |= (1 << 0);
        if (cascadeOBBs[1].Intersects(bounds.worldAABB)) cascadeMask |= (1 << 1);
        if (cascadeOBBs[2].Intersects(bounds.worldAABB)) cascadeMask |= (1 << 2);

        // ★最強のカリング: どのカスケードからも見えないなら、キューにすら積まない（完全消去）
        if (cascadeMask == 0) return;

        // 全てのメッシュを影描画キューに登録
        for (const auto& mesh : model.GetModel()->GetMeshes()) {

            // 影用の伝票を作成
            ShadowCommand cmd;
            cmd.model = model.GetModel();
            cmd.mesh = &mesh;
            cmd.worldMatrix = trans.worldMatrix;
			cmd.cascadeMask = cascadeMask; // どのカスケードで描画するかを示すビットマスクをセット

            // カウンターに提出
            renderQueue.SubmitShadow(cmd);
        }
        
    });
}


REGISTER_RENDER_SYSTEM(ShadowMapRenderSystem, Priority::RenderStage::R06_Shadow);