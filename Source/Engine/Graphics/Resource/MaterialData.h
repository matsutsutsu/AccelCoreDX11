#pragma once
#include <map>
#include <string>
#include <DirectXMath.h>
#include <wrl.h>
#include <d3d11.h> // ID3D11ShaderResourceView用

#include "Engine/Core/Math/StringHash.h"
#include "Engine/Graphics/Resource/GraphicsTypes.h"
#include "Engine/Graphics/Resource/ResourcePool.h"

// TextureHandle が何者であるかを、ここで直接定義する
using TextureResource = Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>;
using TextureHandle   = CCL::Handle<TextureResource>;

// 純粋なデータ構造体。ECSやModelのことは知らなくていい。
struct MaterialData
{
    std::string name = "Material"; // マテリアル名

    // 【変更】ShaderId を廃止し、32bitのハッシュ値で保持する
    uint32_t shaderHash = "Phong"_hash; // デフォルトはPhongのハッシュ

    AlphaMode alphaMode = AlphaMode::Opaque;
    float alphaCutoff = 0.5f;

    // 【警告（将来のリファクタリング布石）】
    // std::map はノードベースのコンテナであり、メモリがヒープ上で断片化するため
    // DOD(データ指向設計)の観点からはキャッシュミスを誘発する最悪の選択肢です。
    // 今回は触れませんが、将来的には「std::vector + 線形探索」か
    // 「固定長配列」に変更することを強く推奨します。
    std::map<std::string, DirectX::XMFLOAT4> colors;
    std::map<std::string, float> scalars;
    std::map<std::string, TextureHandle> textures;
};