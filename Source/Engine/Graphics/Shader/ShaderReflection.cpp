#include "ShaderReflection.h"
#include <fstream>
#include <iostream>

namespace ShaderReflect
{
    // .csoファイルから読み込み
    bool ReflectionData::LoadFromCompiledFile(const char* filename)
    {
        // ファイルを読み込む
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            OutputDebugStringA("Failed to open compiled shader file: ");
            OutputDebugStringA(filename);
            OutputDebugStringA("\n");
            return false;
        }

        size_t fileSize = static_cast<size_t>(file.tellg());
        file.seekg(0, std::ios::beg);

        std::vector<char> bytecode(fileSize);
        file.read(bytecode.data(), fileSize);
        file.close();

        return LoadFromBytecode(bytecode.data(), bytecode.size());
    }

    // バイトコードから読み込み
    bool ReflectionData::LoadFromBytecode(const void* bytecode, size_t bytecodeSize)
    {
        Microsoft::WRL::ComPtr<ID3D11ShaderReflection> reflection;

        // D3DReflectで読み込んだ.csoファイルを解析（リフレクション）する
        // これでシェーダーが何を要求しているか全情報を取得できる
        HRESULT hr = D3DReflect(
            bytecode,
            bytecodeSize,
            IID_ID3D11ShaderReflection,
            (void**)reflection.GetAddressOf()
        );

        if (FAILED(hr))
        {
            OutputDebugStringA("Failed to create shader reflection\n");
            return false;
        }

        // この関数の中でその情報を取り出す
        ExtractReflectionInfo(reflection.Get());
        return true;
    }

    // リフレクション情報を抽出
    void ReflectionData::ExtractReflectionInfo(ID3D11ShaderReflection* reflection)
    {
        D3D11_SHADER_DESC shaderDesc;
        reflection->GetDesc(&shaderDesc);

        // === 定数バッファの抽出 ===
        constantBuffers.clear();
        for (UINT i = 0; i < shaderDesc.ConstantBuffers; ++i)
        {
            ID3D11ShaderReflectionConstantBuffer* cb = reflection->GetConstantBufferByIndex(i);
            D3D11_SHADER_BUFFER_DESC bufferDesc;
            cb->GetDesc(&bufferDesc);

            ConstantBufferInfo cbInfo;
            cbInfo.name = bufferDesc.Name;
            cbInfo.size = bufferDesc.Size;

            // バインドポイントを取得
            for (UINT j = 0; j < shaderDesc.BoundResources; ++j)
            {
                D3D11_SHADER_INPUT_BIND_DESC bindDesc;
                reflection->GetResourceBindingDesc(j, &bindDesc);

                if (bindDesc.Type == D3D_SIT_CBUFFER &&
                    strcmp(bindDesc.Name, bufferDesc.Name) == 0)
                {
                    // レジスタ番号（スロット）が入る
                    cbInfo.bindPoint = bindDesc.BindPoint;
                    break;
                }
            }

            // 変数情報を取得
            for (UINT j = 0; j < bufferDesc.Variables; ++j)
            {
                ID3D11ShaderReflectionVariable* var = cb->GetVariableByIndex(j);
                D3D11_SHADER_VARIABLE_DESC varDesc;
                var->GetDesc(&varDesc);

                ID3D11ShaderReflectionType* type = var->GetType();
                D3D11_SHADER_TYPE_DESC typeDesc;
                type->GetDesc(&typeDesc);

                VariableInfo varInfo;
                varInfo.name = varDesc.Name;
                varInfo.offset = varDesc.StartOffset;
                varInfo.size = varDesc.Size;
                varInfo.type = typeDesc.Type;
                varInfo.rows = typeDesc.Rows;
                varInfo.columns = typeDesc.Columns;

                cbInfo.variables.push_back(varInfo);
            }

            constantBuffers.push_back(cbInfo);
        }

        // === UAVの抽出 ===
        uavs.clear();
        for (UINT i = 0; i < shaderDesc.BoundResources; ++i) {
            D3D11_SHADER_INPUT_BIND_DESC bindDesc;
            reflection->GetResourceBindingDesc(i, &bindDesc);

            // UAV (RWStructuredBuffer または RWTexture) かどうか判定
            if (bindDesc.Type == D3D_SIT_UAV_RWTYPED || bindDesc.Type == D3D_SIT_UAV_RWSTRUCTURED ||
                bindDesc.Type == D3D_SIT_UAV_APPEND_STRUCTURED ||
                bindDesc.Type == D3D_SIT_UAV_CONSUME_STRUCTURED ||
                bindDesc.Type == D3D_SIT_UAV_RWBYTEADDRESS) {
                UnorderedAccessViewInfo uavInfo;
                uavInfo.name      = bindDesc.Name;
                uavInfo.bindPoint = bindDesc.BindPoint;
                // サイズ等は必要なら bindDesc.NumSamples
                // などから推測しますが、基本はBindPointが重要
                uavs.push_back(uavInfo);
            }
        }

        // === テクスチャの抽出 ===
        textures.clear();
        for (UINT i = 0; i < shaderDesc.BoundResources; ++i)
        {
            D3D11_SHADER_INPUT_BIND_DESC bindDesc;
            reflection->GetResourceBindingDesc(i, &bindDesc);

            if (bindDesc.Type == D3D_SIT_TEXTURE)
            {
                TextureInfo texInfo;
                texInfo.name = bindDesc.Name;
                texInfo.bindPoint = bindDesc.BindPoint;
                texInfo.type = bindDesc.Type;
                texInfo.dimension = bindDesc.Dimension;

                textures.push_back(texInfo);
            }
        }

        // === サンプラーの抽出 ===
        samplers.clear();
        for (UINT i = 0; i < shaderDesc.BoundResources; ++i)
        {
            D3D11_SHADER_INPUT_BIND_DESC bindDesc;
            reflection->GetResourceBindingDesc(i, &bindDesc);

            if (bindDesc.Type == D3D_SIT_SAMPLER)
            {
                SamplerInfo sampInfo;
                sampInfo.name = bindDesc.Name;
                sampInfo.bindPoint = bindDesc.BindPoint;

                samplers.push_back(sampInfo);
            }
        }
    }

    const ConstantBufferInfo* ReflectionData::FindConstantBuffer(const std::string& name) const
    {
        for (const auto& cb : constantBuffers)
        {
            if (cb.name == name) return &cb;
        }
        return nullptr;
    }

    const ConstantBufferInfo* ReflectionData::FindConstantBufferBySlot(UINT slot) const
    {
        for (const auto& cb : constantBuffers)
        {
            if (cb.bindPoint == slot) return &cb;
        }
        return nullptr;
    }

    const TextureInfo* ReflectionData::FindTexture(const std::string& name) const
    {
        for (const auto& tex : textures)
        {
            if (tex.name == name) return &tex;
        }
        return nullptr;
    }

    const SamplerInfo* ReflectionData::FindSampler(const std::string& name) const
    {
        for (const auto& samp : samplers)
        {
            if (samp.name == name) return &samp;
        }
        return nullptr;
    }

    void ReflectionData::PrintInfo() const
    {
        OutputDebugStringA("\n=== Shader Reflection Info ===\n");

        OutputDebugStringA("Constant Buffers:\n");
        for (const auto& cb : constantBuffers)
        {
            char buf[256];
            sprintf_s(buf, "  [%s] Slot: b%d, Size: %d bytes\n",
                cb.name.c_str(), cb.bindPoint, cb.size);
            OutputDebugStringA(buf);

            for (const auto& var : cb.variables)
            {
                sprintf_s(buf, "    - %s (Offset: %d, Size: %d)\n",
                    var.name.c_str(), var.offset, var.size);
                OutputDebugStringA(buf);
            }
        }

        OutputDebugStringA("\nTextures:\n");
        for (const auto& tex : textures)
        {
            char buf[256];
            sprintf_s(buf, "  [%s] Slot: t%d\n", tex.name.c_str(), tex.bindPoint);
            OutputDebugStringA(buf);
        }

        OutputDebugStringA("\nSamplers:\n");
        for (const auto& samp : samplers)
        {
            char buf[256];
            sprintf_s(buf, "  [%s] Slot: s%d\n", samp.name.c_str(), samp.bindPoint);
            OutputDebugStringA(buf);
        }
        OutputDebugStringA("===============================\n\n");
    }

    const UnorderedAccessViewInfo *ReflectionData::FindUAV(const std::string &name) const
    {
        for (const auto &uav : uavs) {
            if (uav.name == name) return &uav;
        }
        return nullptr;
    }

    // === AutoConstantBuffer の実装 ===

    bool AutoConstantBuffer::Create(ID3D11Device* device, const ConstantBufferInfo& info)
    {
        bufferInfo = info;
        cpuBuffer.resize(info.size, 0); // ★1: CPU側のデータ置き場を作る

        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth         = info.size; // ★2: シェーダーが要求した通りのサイズ
        desc.Usage             = D3D11_USAGE_DEFAULT;
        desc.BindFlags         = D3D11_BIND_CONSTANT_BUFFER;

        // ★3: GPUへの小包を作る
        HRESULT hr = device->CreateBuffer(&desc, nullptr, buffer.GetAddressOf());
        return SUCCEEDED(hr);
    }



    void AutoConstantBuffer::UpdateBuffer(ID3D11DeviceContext* dc)
    {
        if (isDirty && buffer)
        {
            // ★3: CPUの作業台(cpuBuffer)の中身を、GPUの小包(buffer)へ一気に転送する
            dc->UpdateSubresource(buffer.Get(), 0, nullptr, cpuBuffer.data(), 0, 0);
            isDirty = false;
        }
    }

    void AutoConstantBuffer::BindVS(ID3D11DeviceContext* dc) const
    {
        if (buffer)
        {
            dc->VSSetConstantBuffers(bufferInfo.bindPoint, 1, buffer.GetAddressOf());
        }
    }

    void AutoConstantBuffer::BindPS(ID3D11DeviceContext* dc) const
    {
        if (buffer)
        {
            // ★ステップ1で取得したスロット番号(bindPoint)に、
            // 　小包(buffer)をガチャンと挿し込む
            dc->PSSetConstantBuffers(bufferInfo.bindPoint, 1, buffer.GetAddressOf());
        }
    }

    void AutoConstantBuffer::BindGS(ID3D11DeviceContext* dc) const
    {
        if (buffer)
        {
            dc->GSSetConstantBuffers(bufferInfo.bindPoint, 1, buffer.GetAddressOf());
        }
    }

    void AutoConstantBuffer::BindCS(ID3D11DeviceContext *dc) const
    {
        if (buffer) {
            dc->CSSetConstantBuffers(bufferInfo.bindPoint, 1, buffer.GetAddressOf());
        }
    }

    // ★追加実装
    void AutoConstantBuffer::SetRawData(const void *data, size_t size)
    {
        if (cpuBuffer.empty()) return;

        // バッファサイズを超えないように安全策
        size_t copySize = (size < cpuBuffer.size()) ? size : cpuBuffer.size();

        // メモリコピー
        std::memcpy(cpuBuffer.data(), data, copySize);
        isDirty = true;
    }
}