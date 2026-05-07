#pragma once
#include <memory>
#include "Engine/Graphics/Resource/ResourceManager.h" // ★ハンドルを使うために追加

struct MaterialComponent
{
    MaterialComponent() = default;

    // shared_ptrを完全に排除し、8バイトのチケットの配列にする
    std::vector<MaterialHandle> overrideMaterials;

    // シェーダーの instanceData.customParams にそのまま直結するデータ
    DirectX::XMFLOAT4 customParams = {0.0f, 0.0f, 0.0f, 0.0f};

    // ハンドル（チケット）を受け取ってセットする
    void SetData(size_t index, MaterialHandle sourceHandle)
    {
        if (index >= overrideMaterials.size()) {
            // 足りない分は「無効なチケット(中身が空のHandle)」で埋める
            overrideMaterials.resize(index + 1, MaterialHandle{}); 
        }
        // チケットをコピーするだけなので処理速度は実質ゼロ
        overrideMaterials[index] = sourceHandle;
    }
};