#pragma once
#include <cstdint>
#include <DirectXMath.h>

/**
 * @brief ワンショットの3D音響（銃声、足音、爆発など）を再生するためのイベント。
 * @note EventBus経由で発火させることで、ロジックとオーディオAPIを疎結合にする。
 */
struct PlaySoundEvent {
    uint32_t eventHash;
    DirectX::XMFLOAT3 position;
};