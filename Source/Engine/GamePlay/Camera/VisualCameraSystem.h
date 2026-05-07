#pragma once
#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Camera/VirtualCameraComponents.h"
#include "Engine/GamePlay/Transform/TransformComponent.h"

// ※ Input.h や SimpleMath.h などはここではインクルード不要になります

// ---------------------------------------------------------
// Body: 追従ロジック
// ---------------------------------------------------------
class CameraFollowSystem : public CCL::ECS::IfSystem<CameraFollowSystem,
                               CCL::ECS::Write<VirtualCamera>,
                               CCL::ECS::Read<CameraBodyFollow>> {
  public:
    CameraFollowSystem() : IfSystem("CameraFollowSystem") {}
    void Update(float dt) override; // 中身は書かない
};

// ---------------------------------------------------------
// Aim: 注視ロジック
// ---------------------------------------------------------
class CameraLookAtSystem : public CCL::ECS::IfSystem<CameraLookAtSystem,
                               CCL::ECS::Write<VirtualCamera>,
                               CCL::ECS::Read<CameraAimLookAt>> {
  public:
    CameraLookAtSystem() : IfSystem("CameraLookAtSystem") {}
    void Update(float dt) override;
};

// ---------------------------------------------------------
// Shake: カメラ揺れ
// ---------------------------------------------------------
class CameraShakeSystem : public CCL::ECS::IfSystem<CameraShakeSystem,
                              CCL::ECS::Write<VirtualCamera>,
                              CCL::ECS::Write<CameraShake>> {
  public:
    CameraShakeSystem() : IfSystem("CameraShakeSystem") {}
    void Update(float dt) override;
};

// ---------------------------------------------------------
// Free: 自由移動カメラ
// ---------------------------------------------------------
class CameraFreeControlSystem : public CCL::ECS::IfSystem<CameraFreeControlSystem,
                                    CCL::ECS::Write<VirtualCamera>,
                                    CCL::ECS::Write<CameraBodyFree>,
                                    CCL::ECS::Write<TransformComponent>> {
  public:
    CameraFreeControlSystem() : IfSystem("CameraFreeControlSystem") {}
    void Update(float dt) override;
};

// ---------------------------------------------------------
// TPS: 三人称視点カメラ
// ---------------------------------------------------------
class CameraTPSControlSystem : public CCL::ECS::IfSystem<CameraTPSControlSystem,
                                    CCL::ECS::Write<VirtualCamera>,
                                    CCL::ECS::Write<CameraBodyTPS>,
                                    CCL::ECS::Write<TransformComponent>> {
  public:
    CameraTPSControlSystem() : IfSystem("CameraTPSControlSystem") {}
    void Update(float dt) override;
};

class CameraFPSControlSystem : public CCL::ECS::IfSystem<CameraFPSControlSystem,
    CCL::ECS::Write<VirtualCamera>,
    CCL::ECS::Write<CameraBodyFPS>,
    CCL::ECS::Write<TransformComponent>> {
public:
    CameraFPSControlSystem() : IfSystem("CameraFPSControlSystem") {}
    void Update(float dt) override;
};


// ---------------------------------------------------------
// LockOn: ロックオン制御ロジック TPS用
// ---------------------------------------------------------
class CameraLockOnSystem : public CCL::ECS::IfSystem<CameraLockOnSystem,
    CCL::ECS::Write<VirtualCamera>,
    CCL::ECS::Write<CameraBodyTPS>,
    CCL::ECS::Write<CameraLockOn>,
    CCL::ECS::Write<TransformComponent>>{
public:
    CameraLockOnSystem() : IfSystem("CameraLockOnSystem") {}
    void Update(float dt) override;
};