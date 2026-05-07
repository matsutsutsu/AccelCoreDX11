#pragma once
#include "SimpleMath.h"

/**
 * @brief 空間内で発生した「論理的な音」のイベント。
 * @note FMODで実際に鳴る音とは別に、AIの知覚判定用として使用する。
 */
struct AISoundEvent {
    DirectX::SimpleMath::Vector3 position;     ///< 音の発生源
    float                        volumeRadius; ///< 音が届く物理的な限界距離
};