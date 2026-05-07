#include "PlayerFeedbackSystem.h"
#include "ECS/Core/CCL_World.h"
#include "Game/Logics/GameEvent/GameEvents.h"

// 必要に応じて、あなたのエンジンのマネージャー類をインクルードしてください

// ★カメラのコンポーネントを使うためにインクルードを追加
#include "Engine/GamePlay/Camera/VirtualCameraComponents.h"

// #include "Engine/Platform/Input/Input.h"
// #include "Engine/Audio/AudioManager.h"

// システムの実行順序の定義ヘッダー
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

PlayerFeedbackSystem::PlayerFeedbackSystem() : IfSystem("PlayerFeedbackSystem") {}

void PlayerFeedbackSystem::Initialize()
{
    if (!_world) return;

    // =======================================================================
    // 放送局（EventBus）から「ダメージを受けた」というニュースを受信する
    // =======================================================================
    // これを書くだけで、基底クラスが裏でチケットを保管し、
    // システムが破棄されるときに自動的にEventBusから登録を抹消（Unsubscribe）してくれる！
    ListenEvent<DamageTakenEvent>([this](const DamageTakenEvent& e) {
        // もしダメージを受けたのが「プレイヤー」だったら、各種演出を発動！
        if (e.isPlayer) {

            // -----------------------------------------------------------------
            // 演出1：カメラシェイク
            // （ダメージ量 e.damageAmount に応じて揺れの強さを変えるとリッチになります）
            // -----------------------------------------------------------------
            // ワールド内に存在するすべての VirtualCamera を探す
            auto vcamView = _world->View<VirtualCamera>();
            for (auto camID : vcamView) {

                // ダメージ量(e.damageAmount)に基づいて揺れの強さを計算
                // 例：10ダメージなら強さ1.0。上限を設けて揺れすぎを防ぐ
                float shakeStrength = std::clamp(e.damageAmount * 0.1f, 0.2f, 3.0f);
                float shakeDuration = 0.5f; // 揺れる時間は0.5秒

                // すでに CameraShake コンポーネントを持っているか確認
                if (auto *shake = _world->GetComponent<CameraShake>(camID)) {
                    // 持っていれば上書きして揺らす
                    shake->SetShake(shakeStrength, shakeDuration);
                }
                else {
                    // 持っていなければ新しく取り付けて揺らす
                    CameraShake newShake;
                    newShake.SetShake(shakeStrength, shakeDuration);
                    _world->AddComponent<CameraShake>(camID, newShake);
                }
            }

            // -----------------------------------------------------------------
            // 演出2：コントローラーの振動（ゲームパッド用）
            // -----------------------------------------------------------------
            // auto& pad = Input::Instance().GetGamePad();
            // pad.SetVibration(0.5f, 0.5f, 0.2f); // 左モーター, 右モーター, 秒数

            // -----------------------------------------------------------------
            // 演出3：ダメージSEの再生
            // -----------------------------------------------------------------
            // AudioManager::Instance().PlaySE("Player_Damage");

            // -----------------------------------------------------------------
            // 演出4：画面の縁を赤くする（UIエフェクト）
            // -----------------------------------------------------------------
            // UIManager::Instance().ShowDamageVignette();
        }
    });
}

void PlayerFeedbackSystem::Update(float dt)
{
    // イベント駆動システムのため、毎フレームエンティティを走査する必要はありません。

    // ※もし「画面の赤いエフェクトを、時間経過(dt)で徐々に透明にしていく」などの
    // 毎フレームの更新処理が必要になった場合は、ここに記述します。
}

// 演出系のシステムなので、物理やロジックが終わった後（PostUpdate等）に実行するのが安全です
REGISTER_LOGIC_SYSTEM(PlayerFeedbackSystem, Priority::LogicStage::L06_Resolution);