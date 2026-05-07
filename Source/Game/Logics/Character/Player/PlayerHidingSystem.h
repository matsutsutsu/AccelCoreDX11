#pragma once
#include "ECS/System/CCL_System.h"
#include <vector>
#include "FPSPlayerComponent.h"
#include "PlayerTag.h"

class TransformComponent;
class HidingSpotComponent;
class HidingSpotComponent;


namespace HidingMath {
   // Under: 潜り込み用の高度とピッチを計算
    void CalculateUnderSequence(float t, float depth, float lookDownAngle, float& outHeight, float& outPitch) {
        // --- 1. 高度の計算 ---
        // 前半30%で一気に沈み込み、残りの70%はその高さを維持
        float downT = (std::min)(t / 0.3f, 1.0f);
        outHeight = -depth * downT;

        // --- 2. ピッチ(視線)の計算 ---
        if (t < 0.3f) {
            // 最初の30%は視線は動かさない（沈み込みに専念）
            outPitch = 0.0f;
        }
        else {
            // 残りの70%でサインカーブを描く (t: 0.3 -> 1.0 を 0.0 -> 1.0 に正規化)
            float lookT = (t - 0.3f) / 0.7f;
            outPitch = lookDownAngle * sinf(lookT * 3.14159f);
        }
    }

    // TopIn: 上昇して覗き込み、中に沈む
    void CalculateTopInSequence(float t, float riseHeight, float lookDownAngle, float sinkDepth, float& outHeight, float& outPitch) {
        if (t < 0.3f) {
            // 1. 上昇フェーズ (t: 0.0 -> 0.3)
            float phaseT = t / 0.3f;
            outHeight = riseHeight * phaseT;
            outPitch = 0.0f;
        }
        else if (t < 0.7f) {
            // 2. 覗き込みフェーズ (t: 0.3 -> 0.7)
            float phaseT = (t - 0.3f) / 0.4f;
            outHeight = riseHeight;
            outPitch = lookDownAngle * phaseT;
        }
        else {
            // 3. 下降フェーズ (t: 0.7 -> 1.0)
            float phaseT = (t - 0.7f) / 0.3f;
            outHeight = riseHeight - ((riseHeight + sinkDepth) * phaseT);
            outPitch = lookDownAngle * (1.0f - phaseT);
        }
    }
}

class PlayerHidingSystem
    : public CCL::ECS::IfSystem<PlayerHidingSystem,
    CCL::ECS::Write<TransformComponent>,
    CCL::ECS::Write<FPSPlayerComponent >>
{
public:
    PlayerHidingSystem() : IfSystem("PlayerHidingSystem") {}
    virtual ~PlayerHidingSystem() = default;

    void Update(float dt) override;
};