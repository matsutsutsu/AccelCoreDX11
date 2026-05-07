#pragma once
#include "ECS/System/CCL_System.h"
#include <DirectXMath.h>
#include <vector>

class LightSystem : public CCL::ECS::SystemBase {
  public:
    LightSystem();
    virtual ~LightSystem() = default;

    std::vector<CCL::ECS::TypeID> GetReadTypes() const override;
    void                          Update(float dt) override;

  private:
    // クォータニオンから前方ベクトル(Z+)を取り出すヘルパー
    DirectX::XMFLOAT3 CalculateDirection(const DirectX::XMFLOAT4 &rotation);
};