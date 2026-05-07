#include "AudioSystem.h"
#include "Engine/Audio/IAudioAPI.h"
#include "Engine/Audio/AudioEvents.h"
#include "ECS/System/CCL_SystemRegistry.h"
#include "Engine/Platform/Logger.h" // 厳格なロギング仕様に従う

// 指示書通り、cppファイルのインクルード直後で using namespace を使用する
using namespace CCL::ECS;

void AudioSystem::Initialize() {
    // ---------------------------------------------------------
    // イベント購読 (ListenEvent)
    // 理由: 基底クラスの _eventCleanups に自動登録され、
    // システム破棄時に安全に Unsubscribe されるため。
    // ---------------------------------------------------------
    ListenEvent<PlaySoundEvent>([this](const PlaySoundEvent& e) {
        if (!_world) return;

        // リソースへのアクセス前に必ず HasResource でNullチェックを行う (指示書準拠)
        if (!_world->HasResource<std::shared_ptr<IAudioAPI>>()) return;

        auto& api = _world->GetResource<std::shared_ptr<IAudioAPI>>();
        if (api) {
            api->PlayOneShot3D(e.eventHash, e.position);
        }
        });
}

void AudioSystem::Update(float dt) {
    if (!_world) return;

    // リソースの生存確認
    if (!_world->HasResource<std::shared_ptr<IAudioAPI>>()) return;
    auto& api = _world->GetResource<std::shared_ptr<IAudioAPI>>();
    if (!api) return;

    // ---------------------------------------------------------
    // エミッターの座標同期 (並列処理)
    // 理由: チャンク単位のバッチ処理 (ForEachParallel) を用いることで、
    // メモリの連続性を活かし(SoA)、マルチコアでの高速処理を実現する。
    // ---------------------------------------------------------
    ForEachParallel([&api](const TransformComponent& transform, AudioEmitterComponent& emitter) {

        // 未再生で自動再生フラグが立っている場合のトリガー
        if (emitter.autoPlay && !emitter.isPlaying && emitter.eventHash != 0) {
            emitter.playingId = api->PlayEvent3D(emitter.eventHash, transform.position);
            emitter.isPlaying = (emitter.playingId != 0);
        }

        // 再生中のインスタンスの座標を毎フレーム追従させる
        if (emitter.isPlaying && emitter.playingId != 0) {
            api->SetEvent3DAttributes(emitter.playingId, transform.position);
        }
        });

    // ---------------------------------------------------------
    // FMOD内部の更新とメモリのお掃除
    // ---------------------------------------------------------
    api->Update();
}

// ---------------------------------------------------------
// システムの自動登録マクロ (cppファイルの末尾)
// 理由: Transformが更新された後（L04_Physics）に音源位置を同期させるため、
// LogicStageの後半である L05 または L06 以降がアーキテクチャ上適切。
// ---------------------------------------------------------
REGISTER_LOGIC_SYSTEM(AudioSystem, Priority::LogicStage::L06_Resolution);