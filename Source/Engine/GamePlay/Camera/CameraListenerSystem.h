#pragma once
#include "ECS/System/CCL_System.h"

// ===================================================================================
// 【 リスナー同期システム : CameraListenerSystem 】
// [ 役割 ]
// 現在のメインカメラ（プレイヤーの視点）の座標と向きを、オーディオAPI（FMOD）に伝える。
// これにより、FMOD側で設定した「3D距離減衰（Spatializer）」が正しく機能するようになる。
// ===================================================================================
class CameraListenerSystem : public CCL::ECS::SystemBase {
public:
    CameraListenerSystem();
    virtual ~CameraListenerSystem() = default;

    std::vector<CCL::ECS::TypeID> GetReadTypes() const override { return {}; }
    void Update(float dt) override;
};