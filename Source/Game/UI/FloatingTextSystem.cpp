#include "FloatingTextSystem.h"
#include "Game/Logics/Character/Player/PlayerComponent.h"
#include <SimpleMath.h>

// システムの実行順序の定義ヘッダー
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

using namespace DirectX::SimpleMath;
//
//void FloatingTextSystem::Update(float dt)
//{
//    // 1. プレイヤーの位置を取得
//    Vector3 playerPos   = Vector3::Zero;
//    bool    playerFound = false;
//
//    auto playerView = _world->View<PlayerComponent, TransformComponent>();
//    if (!playerView.empty()) {
//        auto *trans = _world->GetComponent<TransformComponent>(playerView[0]);
//        playerPos   = trans->GetWorldPosition();
//        playerFound = true;
//    }
//
//    if (!playerFound) return;
//
//    // 2. 全てのフローティングテキストをチェック
//    ForEachWithID([&](CCL::ECS::EntityID     entityID,
//                      FloatingTextComponent &floatText,
//                      TransformComponent    &trans) {
//        Vector3 targetPos = trans.GetWorldPosition();
//        float   dist      = Vector3::Distance(targetPos, playerPos);
//
//        // --- 範囲内に入った場合の処理 ---
//        if (dist <= floatText.triggerRadius) {
//
//            // まだ表示していなければ追加
//            if (!floatText.isShowing) {
//                floatText.isShowing = true;
//                // ユニーク名生成
//                floatText.uiName = "FloatText_" + std::to_string(entityID);
//
//                // 新しいAPI: 持続的な3Dテキストを追加
//                _textManager.AddWorldText(
//                    floatText.uiName, floatText.text, targetPos + floatText.offset);
//            }
//            // 既に表示中なら位置だけ更新
//            else {
//                // 新しいAPI: 位置更新
//                _textManager.UpdateWorldTextPos(floatText.uiName, targetPos + floatText.offset);
//            }
//        }
//        // --- 範囲外に出た場合の処理 ---
//        else {
//            if (floatText.isShowing) {
//                floatText.isShowing = false;
//                // 新しいAPI: 削除
//                _textManager.RemoveWorldText(floatText.uiName);
//            }
//        }
//    });
//}
//
//REGISTER_RENDER_SYSTEM(FloatingTextSystem, Priority::RenderStage::UI);