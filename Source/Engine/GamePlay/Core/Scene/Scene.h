#pragma once
#include <vector>
#include "Engine/Graphics/RenderPass/RenderPassDesc.h"

class DX12System;
class CommandList;
class SystemDataContext;
class ResourceManager;

// ---------------------------------------------------------
// 【第1階層】Scene (純粋なインターフェース)
// ---------------------------------------------------------
class Scene
{
public:
    Scene() = default;
    virtual ~Scene() = default;

    // 初期化と終了
    virtual void Initialize(DX12System* dx12System, SystemDataContext* systemDataContext, ResourceManager* resourceManager) = 0;
    virtual void Start() {} // 初期化直後に1回だけ呼ばれるフック
    virtual void Finalize() = 0;

    // 更新処理
    virtual void FixedUpdate(float fixedTime) = 0;
    virtual void Update(float elapsedTime, int frameIndex) = 0;

    // 描画処理
    virtual void Render(DX12System* dx12System, CommandList* commandList, int frameIndex) = 0;

    // 状態管理
    bool IsReady() const { return ready; }
    void SetReady() { ready = true; }

private:
    bool ready = false;
};