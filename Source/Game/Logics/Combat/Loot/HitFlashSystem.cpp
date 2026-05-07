#include "HitFlashSystem.h"
#include "Game/Logics/GameEvent/GameEvents.h"
#include <vector>

// システムの実行順序の定義ヘッダー
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

HitFlashSystem::HitFlashSystem() : IfSystem("HitFlashSystem") {}

void HitFlashSystem::Initialize()
{
    if (!_world) return;

    // 1. 誰か（プレイヤーでも敵でも）がダメージを受けた時のイベントを購読
    _world->GetEventBus().Subscribe<DamageTakenEvent>([this](const DamageTakenEvent &e) {
        // 描画される実体（MaterialComponent）を持っているか確認
        if (_world->HasComponent<MaterialComponent>(e.targetID)) {

            HitFlashComponent flash;
            flash.duration = 0.4f;
            flash.timer    = 0.4f;

            // 既にフラッシュ中なら時間を上書き（連続ヒット対策）、無ければ新しく追加
            if (_world->HasComponent<HitFlashComponent>(e.targetID)) {
                *_world->GetComponent<HitFlashComponent>(e.targetID) = flash;
            }
            else {
                _world->AddComponent<HitFlashComponent>(e.targetID, flash);
            }
        }
    });
}

void HitFlashSystem::Update(float dt)
{
    std::vector<CCL::ECS::EntityID> finishedEntities;

    ForEachWithID([&](CCL::ECS::EntityID id, HitFlashComponent &flash, MaterialComponent &mat) {
        // 1. タイマーを減らす
        flash.timer -= dt;

        // 2. 現在のフラッシュ強度を計算（時間が経つにつれてスッと消えるようにする）
        float intensity = 0.0f;
        if (flash.timer > 0.0f) {
            float ratio = flash.timer / flash.duration;
            intensity   = flash.maxIntensity * ratio;
        }
        else {
            // 時間切れなら削除リストへ
            finishedEntities.push_back(id);
        }

        // 3. GPUへパラメータを送る
        // ディゾルブで customParams.x と y を使ったので、今回は空いている「z」を使います！
        mat.customParams.z = intensity;
    });

    // 4. フラッシュが終わったエンティティからコンポーネントを剥がし、GPUの値を0に戻す
    for (auto id : finishedEntities) {
        _world->RequestRemoveComponent<HitFlashComponent>(id);

        if (auto *mat = _world->GetComponent<MaterialComponent>(id)) {
            mat->customParams.z = 0.0f;
        }
    }
}

// 演出系なので Resolution (ダメージ計算の直後) に登録します
REGISTER_LOGIC_SYSTEM(HitFlashSystem, Priority::LogicStage::Resolution);