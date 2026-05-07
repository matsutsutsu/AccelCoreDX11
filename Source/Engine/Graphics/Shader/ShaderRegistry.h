#pragma once
#include <unordered_map>
#include <functional>
#include <memory>
#include "Engine/Graphics/Shader/Material/Core/Shader.h"
#include "Engine/Core/Math/StringHash.h"

class ShaderRegistry {
public:
    static ShaderRegistry& Instance() { static ShaderRegistry instance; return instance; }

    // エンジン起動時等にシェーダーを登録する関数
    template<typename T>
    void RegisterShader(uint32_t shaderHash) {
        _factories[shaderHash] = [](ID3D11Device* device) {
            return std::make_unique<T>(device);
            };
        // 新しいシェーダーが登録されるたびに、0から順にソート用の連番IDを割り当てる
        _shaderSortIDs[shaderHash] = _nextSortID++;
    }

    // 必要な時に生成、あるいはキャッシュを返す
    Shader* GetShader(uint32_t shaderHash, ID3D11Device* device) {
        // すでにインスタンスがあればそれを返す
        auto it = _shaders.find(shaderHash);
        if (it != _shaders.end()) return it->second.get();

        // なければファクトリから生成
        auto factoryIt = _factories.find(shaderHash);
        if (factoryIt != _factories.end()) {
            _shaders[shaderHash] = factoryIt->second(device);
            return _shaders[shaderHash].get();
        }
        return nullptr; // 見つからなかった場合（紫の警告シェーダー等を返しても良い）
    }

    // ハッシュ値から、8bitに収まる小さな連番IDを取得する
    uint8_t GetSortID(uint32_t shaderHash) const {
        auto it = _shaderSortIDs.find(shaderHash);
        return it != _shaderSortIDs.end() ? it->second : 0;
    }

private:
    std::unordered_map<uint32_t, std::function<std::unique_ptr<Shader>(ID3D11Device*)>> _factories;
    std::unordered_map<uint32_t, std::unique_ptr<Shader>> _shaders;

    // ソートID管理用
    uint8_t _nextSortID = 0;
    std::unordered_map<uint32_t, uint8_t> _shaderSortIDs;

};