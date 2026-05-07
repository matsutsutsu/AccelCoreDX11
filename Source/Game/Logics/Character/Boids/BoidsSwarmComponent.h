#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <vector>
#include "Engine/Graphics/Resource/Model.h"

// HLSLと一致させる
struct BoidData {
    DirectX::XMFLOAT3 position;
    float             scale;
    DirectX::XMFLOAT3 velocity;
    float             padding;
};

struct CbBoidsParams {
    DirectX::XMFLOAT3 targetPosition;
    float             targetWeight;
    float             separationWeight;
    float             alignmentWeight;
    float             cohesionWeight;
    float             perceptionRadius;
    float             maxSpeed;
    float             maxForce;
    int               boidsCount;
    float             deltaTime;
};

struct BoidsSwarmComponent {
    // GPUバッファ
    Microsoft::WRL::ComPtr<ID3D11Buffer>              computeBuffer;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> computeUAV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  computeSRV;
    Microsoft::WRL::ComPtr<ID3D11Buffer>              constantBuffer;

    // 描画するモデル（魚やゾンビなど）
    std::shared_ptr<Model> model;

    CbBoidsParams params;
    bool isInitialized = false;
};