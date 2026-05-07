#pragma once
#include "Engine/Graphics/Core/RenderContext.h"

// 各パスをTracyで追跡できるようにインクルード
#include "tracy/Tracy.hpp"


class ModelRenderer; // 循環参照を防ぐための前方宣言

class IRenderPass {
public:
    virtual ~IRenderPass() = default;

    virtual void Initialize(ID3D11Device* device) {}

    virtual void Execute(const RenderContext& rc) = 0;
};