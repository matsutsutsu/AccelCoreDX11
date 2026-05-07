#include "Shader.h"
#include "Engine/Graphics/Resource/ResourceManager.h" // ★ここでManagerを知る
#include "Engine/Graphics/Shader/Pass/ShadowMap.h"
#include "Engine/Graphics/Shader/ShaderResources.h"
#include <algorithm>

bool Shader::InitializeReflection(ID3D11Device* device, const std::string& csoPath)
{
    if (!_reflection.LoadFromCompiledFile(csoPath.c_str())) return false;

    const auto& cbs = _reflection.GetConstantBuffers();
    for (const auto& cbInfo : cbs)
    {
        if (cbInfo.bindPoint >= SLOT_CB_SCENE) {
            continue; // システム予約領域は無視
        }
        _constantBuffers[cbInfo.name].Create(device, cbInfo);
    }
    return true;
}

int Shader::FetchTextureSlot(const std::string& name)
{
    int slot = _reflection.FindTextureSlot(name);
    if (slot != -1) {
        _textureSlots[name] = slot;
        _usedTextureSlots.push_back(slot);
    }
    return slot;
}

int Shader::FetchSamplerSlot(const std::string& name)
{
    int slot = _reflection.FindSamplerSlot(name);
    if (slot != -1) {
        _samplerSlots[name] = slot;
    }
    return slot;
}

void Shader::ApplyShaderPipeline(ID3D11DeviceContext* dc, ID3D11VertexShader* vs, ID3D11PixelShader* ps, ID3D11InputLayout* layout)
{
    dc->IASetInputLayout(layout);
    dc->VSSetShader(vs, nullptr, 0);
    dc->PSSetShader(ps, nullptr, 0);

    for (auto& pair : _constantBuffers) {
        pair.second.BindVS(dc);
        pair.second.BindPS(dc);
    }
}

void Shader::UnbindResources(ID3D11DeviceContext* dc)
{
    ID3D11ShaderResourceView* nullSRV = nullptr;
    ID3D11Buffer* nullCB = nullptr;

    for (auto& pair : _constantBuffers) {
        UINT slot = (UINT)pair.second.GetBindPoint();
        dc->VSSetConstantBuffers(slot, 1, &nullCB);
        dc->PSSetConstantBuffers(slot, 1, &nullCB);
    }

    for (int slot : _usedTextureSlots) {
        dc->PSSetShaderResources((UINT)slot, 1, &nullSRV);
    }

    dc->VSSetShader(nullptr, nullptr, 0);
    dc->PSSetShader(nullptr, nullptr, 0);
    dc->IASetInputLayout(nullptr);
}

void Shader::UpdateMaterialProperties(ID3D11DeviceContext* dc, const MaterialData& mat)
{
    for (auto& cbPair : _constantBuffers) {
        auto& cb = cbPair.second;
        bool isUpdated = false;

        for (const auto& prop : mat.scalars) {
            if (cb.SetValue(prop.first, prop.second)) isUpdated = true;
        }

        for (const auto& prop : mat.colors) {
            if (cb.SetValue(prop.first, prop.second)) isUpdated = true;
        }

        if (isUpdated) {
            cb.UpdateBuffer(dc);
        }
    }

    const auto& shaderTextures = _reflection.GetTextures();

    for (const auto& texInfo : shaderTextures) {
        if (texInfo.bindPoint >= SLOT_SRV_SHADOW) continue;
        if (texInfo.bindPoint >= SLOT_SRV_FOG_NOISE) continue;

        auto it = mat.textures.find(texInfo.name);
        UINT slot = texInfo.bindPoint;

        if (it != mat.textures.end() && it->second.IsValid()) {
            // ★cppに分けたことで、ここでResourceManagerを安全に呼び出せる
            ID3D11ShaderResourceView* srv = ResourceManager::Instance().GetTexture(it->second);
            dc->PSSetShaderResources(slot, 1, &srv);

            if (std::find(_usedTextureSlots.begin(), _usedTextureSlots.end(), slot) == _usedTextureSlots.end()) {
                _usedTextureSlots.push_back(slot);
            }
        }
        else {
            ID3D11ShaderResourceView* nullSRV = nullptr;
            dc->PSSetShaderResources(slot, 1, &nullSRV);
        }
    }
}

std::filesystem::file_time_type Shader::GetLastWriteTime(const std::string& filepath)
{
    if (std::filesystem::exists(filepath)) {
        return std::filesystem::last_write_time(filepath);
    }
    return (std::filesystem::file_time_type::min)();
}