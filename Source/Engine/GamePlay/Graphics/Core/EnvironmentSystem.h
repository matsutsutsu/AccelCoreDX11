#pragma once
#include "ECS/System/CCL_System.h"
#include <d3d11.h>
#include <vector>
#include <wrl.h>
#include <DirectXMath.h>

// 【舞台監督】カメラ、フォグ、グローバルノイズなど、環境全体のセットアップを行うシステム
class EnvironmentSystem : public CCL::ECS::SystemBase {
  public:
    EnvironmentSystem();
    virtual ~EnvironmentSystem() = default;

    std::vector<CCL::ECS::TypeID> GetReadTypes() const override;
    void                          Update(float dt) override;

    // 描画ループの直前で呼び出し、テクスチャ等をGPUにバインドする
    void BindGlobalResources(ID3D11DeviceContext *dc);

  private:

      // シーン用定数バッファのデータ構造
      struct CbSceneData {
          DirectX::XMFLOAT4X4 viewProjection;
          DirectX::XMFLOAT4   lightDirection;
          DirectX::XMFLOAT4   lightColor;
          DirectX::XMFLOAT4   cameraPosition;
          DirectX::XMFLOAT4X4 view;
          DirectX::XMFLOAT4X4 projection;
      };
      CbSceneData _sceneData = {}; // 計算結果をキャッシュする用

    // ★FogSystemから引っ越してきた定数バッファのデータ構造
    struct CbFogData {
        DirectX::XMFLOAT4 color;
        DirectX::XMFLOAT4 params; // x:start, y:end, z:heightStart, w:heightDensity
        DirectX::XMFLOAT4 center;
        DirectX::XMFLOAT4 noiseParams; // x:noiseScale, y:noiseStrength, z:speedX, w:speedY
        DirectX::XMFLOAT4 rimColor;    // rgb: color, a: padding
        DirectX::XMFLOAT4 rimParams;   // x: power, y: strength, zw: padding
    };

    // フォグ用の定数バッファ
    Microsoft::WRL::ComPtr<ID3D11Buffer> _fogConstantBuffer;

    // Updateで見つけたフォグのテクスチャを保持しておく変数
    ID3D11ShaderResourceView *_currentFogNoiseSRV = nullptr;

    // エンジン全体で共有するディゾルブ用ノイズテクスチャ
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> _globalNoiseSRV;

    // =========================================================
    // IBL用のグローバルテクスチャ
    // =========================================================
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> _irradianceMapSRV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> _prefilterMapSRV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> _brdfLutSRV;
};