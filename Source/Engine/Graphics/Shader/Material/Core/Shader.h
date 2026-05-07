#pragma once

#include "Engine/Graphics/Core/RenderContext.h"
#include "Engine/Graphics/Resource/Model.h"
#include "Engine/Graphics/Shader/ShaderReflection.h"
#include "Engine/Graphics/Resource/MaterialData.h"

#include <d3d11.h>
#include <map>
#include <string>
#include <vector>
#include <filesystem>

class Shader
{
public:
    Shader() {}
    virtual ~Shader() {}

    virtual void Begin(const RenderContext& rc) = 0;
    virtual void Update(const RenderContext& rc, const Model::Mesh& mesh) = 0;
    virtual void End(const RenderContext& rc) = 0;

    // === リフレクション情報へのアクセス ===
    const ShaderReflect::ReflectionData& GetReflection() const { return _reflection; }
    const std::map<std::string, ShaderReflect::AutoConstantBuffer>& GetConstantBuffers() const { return _constantBuffers; }

    // テンプレート関数はコンパイル時に展開されるため、ヘッダーに実装を残します
    template<typename T>
    bool SetConstantValue(const std::string& bufferName, const std::string& varName, const T& value)
    {
        auto it = _constantBuffers.find(bufferName);
        if (it == _constantBuffers.end()) return false;
        return it->second.SetValue(varName, value);
    }

    virtual void CheckAndReload(ID3D11Device* device) {}

protected:
    // --- 共通機能：データメンバ ---
    ShaderReflect::ReflectionData _reflection;
    std::map<std::string, ShaderReflect::AutoConstantBuffer> _constantBuffers;
    std::map<std::string, int> _textureSlots;
    std::map<std::string, int> _samplerSlots;
    std::vector<int> _usedTextureSlots;
    std::filesystem::file_time_type _lastWriteTime;

    // --- 共通機能：メソッド宣言（実装は .cpp へ） ---
    bool InitializeReflection(ID3D11Device* device, const std::string& csoPath);
    int FetchTextureSlot(const std::string& name);
    int FetchSamplerSlot(const std::string& name);
    void ApplyShaderPipeline(ID3D11DeviceContext* dc, ID3D11VertexShader* vs, ID3D11PixelShader* ps, ID3D11InputLayout* layout);
    void UnbindResources(ID3D11DeviceContext* dc);
    void UpdateMaterialProperties(ID3D11DeviceContext* dc, const MaterialData& mat);
    std::filesystem::file_time_type GetLastWriteTime(const std::string& filepath);
};