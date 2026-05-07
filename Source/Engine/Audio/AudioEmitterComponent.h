#pragma once
#include <cstdint>
#include <DirectXMath.h>

/**
 * @brief 継続して再生され、エンティティの座標に追従するオーディオエミッター。
 * @note BGM、エンジン音、燃え盛る炎の環境音などに使用する。
 */
struct AudioEmitterComponent {
    uint32_t eventHash = 0;   ///< 再生するFMODイベントのハッシュ値
    uint32_t playingId = 0;   ///< IAudioAPI が発行した制御用ID (0は停止・未再生)
    bool     isPlaying = false; ///< 現在再生中かどうかのフラグ
    bool     autoPlay = true;   ///< 生成時に自動再生するかどうか
};