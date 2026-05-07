#pragma once

#include "ECS/System/CCL_System.h"
#include "Engine/GamePlay/Graphics/Lighting/FogComponent.h"
#include <wrl.h>

// 前方宣言
struct ID3D11Buffer;
struct ID3D11Device;

class FogSystem : public CCL::ECS::IfSystem<FogSystem, CCL::ECS::Read<FogComponent>> {
  private:
    // 定数バッファのデータ構造 (HLSLのパッキング規則に合わせる: 16byte境界)
    struct CbFogData {
        DirectX::XMFLOAT4 color;
        DirectX::XMFLOAT4 params; // x:start, y:end, z:heightStart, w:heightDensity
        DirectX::XMFLOAT4 center;
        DirectX::XMFLOAT4 noiseParams; // x:noiseScale, y:noiseStrength, z:speedX, w:speedY
        DirectX::XMFLOAT4 rimColor;    // rgb: color, a: padding
        DirectX::XMFLOAT4 rimParams;   // x: power, y: strength, zw: padding
    };

    Microsoft::WRL::ComPtr<ID3D11Buffer> _constantBuffer;
    bool                                 _isInitialized = false;

  public:
    FogSystem();
    virtual ~FogSystem() = default;

    void Update(float dt) override;

  private:
    //void Initialize(ID3D11Device *device);
};