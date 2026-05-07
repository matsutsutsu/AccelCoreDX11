#pragma once
#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Camera/VirtualCameraComponents.h"
#include <DirectXMath.h>
#include <vector>

// 前方宣言 (インクルード削減のため)
class Camera;

class CameraBrainSystem : public CCL::ECS::SystemBase 
{
  private:
    // ブレンド管理用
    CCL::ECS::EntityID _currentCamID = CCL::ECS::InvalidEntityID; // 現在のメイン
    CCL::ECS::EntityID _prevCamID    = CCL::ECS::InvalidEntityID; // 遷移元

    float _blendDuration = 0.3f; // デフォルトブレンド時間
    float _blendTimer    = 0.0f;

    // 前回のカメラ状態（遷移元が消滅した場合のバックアップ用）
    DirectX::XMFLOAT3 _lastPos  = {0, 0, -10};
    DirectX::XMFLOAT3 _lastLook = {0, 0, 0};
    float             _lastFov  = 45.0f;
    // Near/Farのバックアップ
    float _lastNear = 0.1f;
    float _lastFar  = 1000.0f;

  public:
    CameraBrainSystem();
    virtual ~CameraBrainSystem() = default;

    // どのデータにアクセスするかをスケジューラに自己申告する
    std::vector<CCL::ECS::TypeID> GetReadTypes() const override;

    void Update(float dt) override;
};