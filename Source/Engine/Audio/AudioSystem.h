#pragma once
#include "ECS/System/CCL_System.h"
#include "ECS/System/CCL_SystemAccess.h"
#include "Engine/Audio/AudioEmitterComponent.h"
#include "Engine/GamePlay/Transform/TransformComponent.h" 

class IAudioAPI; // 前方宣言

/**
 * @brief ゲーム内のオーディオロジックを統合管理し、FMODエンジンを駆動するシステム。
 * @note
 * - `IfSystem` のテンプレート引数により、Transform(Read)とAudioEmitter(Write)を持つChunkのみを対象とする。
 * - 内部で並列処理（ForEachParallel）を用いて、キャッシュラインに優しいDODバッチ更新を行う。
 */
class AudioSystem : public CCL::ECS::IfSystem<AudioSystem, 
                           CCL::ECS::Read<TransformComponent>, 
                           CCL::ECS::Write<AudioEmitterComponent>> 
{
public:
    AudioSystem() : IfSystem("AudioSystem") {}
    virtual ~AudioSystem() = default;

    void Initialize() override;

    void Update(float dt) override;
};