#include "ModelRenderSystem.h"

// 実装詳細で必要なインクルード
#include "Engine/GamePlay/Graphics/Lighting/FogComponent.h"
#include "Engine/Graphics/Core/Graphics.h" // DeviceContext取得用
#include "Engine/Graphics/Renderer/ModelRenderer.h"
#include "Engine/Graphics/Resource/Model.h" // Modelポインタ用
#include "Engine/Graphics/Shader/ShaderResources.h"
#include <DirectXCollision.h> // 視錐台カリング用

#include "Engine/Graphics/Renderer/RenderQueue.h"
#include "Engine/Core/Math/StringHash.h"

// システムの実行順序の定義ヘッダー
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

using namespace CCL::ECS;
using namespace DirectX;

namespace {
    // クォータニオンの正規化チェック用ヘルパー
    inline bool IsUnitQuaternion(FXMVECTOR q, float epsilon = 1e-3f)
    {
        XMVECTOR lenSq = XMVector4Dot(q, q);
        float    value = XMVectorGetX(lenSq);
        return fabsf(value - 1.0f) < epsilon;
    }

  
} // namespace

ModelRenderSystem::ModelRenderSystem() : IfSystem("ModelRenderSystem"){}

void ModelRenderSystem::Update(float dt)
{

    // 最適化 シングルトンへのアクセス(ロック等のオーバーヘッド)をループの外に出す
    auto& resManager = ResourceManager::Instance();
    auto& renderQueue = RenderQueue::Instance();

    // TransformComponent と ModelComponent の組を全エンティティ分回す
    ForEach([&](const TransformComponent& transform,
        const ModelComponent& modelComp,
        const MaterialComponent& mat,
        const VisibilityComponent& vis) { // ★追加

            // ★ 最重要：ここでカリング結果を見て、画面外なら即座に処理をスキップ（サボる）
            if (!vis.isVisible) return;

            Model* model = modelComp.GetModel(); // ハンドルから取得
            if (!model) return;


        // ShadowMapでUpdateTransform行うのでここではやらない
        bool hasBones = false;
        for (const auto &mesh : model->GetMeshes()) {
            if (!mesh.bones.empty()) {
                hasBones = true;
                break;
            }
        }
        

        // シェーダーID決定（リストの先頭を使うなどの簡易判定）
        uint32_t shaderHash = "Phong"_hash;

        // IsValid() でチケットを確認し、実体をもらう
        // Materialを個別に持っている場合の条件
        if (!mat.overrideMaterials.empty() && mat.overrideMaterials[0].IsValid()) {
            MaterialData *matData =
                resManager.GetMaterial(mat.overrideMaterials[0]);
            if (matData) {
                shaderHash = matData->shaderHash; 
            }
		} // 共有マテリアルを使う場合の条件
        else {
            const auto &materials = model->GetMaterials();
            if (!materials.empty() && materials[0].data) 
            {
                shaderHash = materials[0].data->shaderHash; 
            }
        }

        // =========================================================
        // ★最重要変更：ModelRenderer::Draw() を呼ぶのをやめ、伝票を作って提出！
        // =========================================================
        RenderCommand cmd;
        cmd.shaderHash = shaderHash;
        cmd.assetHash = modelComp.assetHash;
        cmd.model = model;
        cmd.worldMatrix = transform.worldMatrix;

        // マテリアルオーバーライド配列のコピー
        cmd.overrideCount = static_cast<uint8_t>((std::min)(mat.overrideMaterials.size(), static_cast<size_t>(4)));
        for (uint8_t j = 0; j < cmd.overrideCount; ++j) {
            cmd.overrideMaterials[j] = mat.overrideMaterials[j];
        }

		cmd.customParams = mat.customParams;

        // RenderQueueのカウンターに提出！
        renderQueue.SubmitOpaque(cmd);

    });
}


REGISTER_RENDER_SYSTEM(ModelRenderSystem, Priority::RenderStage::R08_Main);