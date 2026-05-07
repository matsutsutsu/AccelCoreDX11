#include "PostProcessSystem.h"
#include "Engine/Graphics/Shader/PostProcess/PostProcessManager.h"



// システムの実行順序の定義ヘッダー
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

void PostProcessSystem::Update(float dt) {
    // Worldからリソースとして取得する
    if (!_world->HasResource<PostProcessManager *>()) return;
    auto *postProcess = _world->GetResource<PostProcessManager *>();

    // -------------------------------------------------------
    // 1. デフォルト値でリセット
    // -------------------------------------------------------
    // エンティティが一つもない場合、ブルームはOFF、露出は標準(1.0)にする
    BloomConfigComponent finalBloom;
    finalBloom.enable = false;

    ToneMapConfigComponent finalToneMap;
    finalToneMap.exposure = 1.0f;


    // -------------------------------------------------------
    // 2. BloomConfigComponent を検索
    // -------------------------------------------------------
    // ※ 本来は「GlobalVolume」タグやカメラ位置判定を行うが、
    //    ここではシンプルに「有効になっているものが見つかったら採用」する
    auto bloomView = _world->View<BloomConfigComponent>();
    for (auto entity : bloomView)
    {
        const auto* comp = _world->GetComponent<BloomConfigComponent>(entity);

        // 有効な設定が見つかったら上書き（後勝ち）
        if (comp && comp->enable) {
            finalBloom = *comp;
        }
    }

    // -------------------------------------------------------
    // 3. ToneMapConfigComponent を検索
    // -------------------------------------------------------
    auto toneMapView = _world->View<ToneMapConfigComponent>();
    for (auto entity : toneMapView)
    {
        const auto* comp = _world->GetComponent<ToneMapConfigComponent>(entity);

        if (comp) {
            finalToneMap = *comp;
        }
    }

    // -------------------------------------------------------
    // 4. マネージャーへ一括転送
    // -------------------------------------------------------
    postProcess->bloomConfig = finalBloom;
    postProcess->toneMapConfig = finalToneMap;
}


REGISTER_RENDER_SYSTEM(PostProcessSystem, Priority::RenderStage::R09_PostProcess);