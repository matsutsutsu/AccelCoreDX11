#pragma once
#include <cstdint>
#include "Engine/Graphics/Resource/Model.h"
#include "Engine/Graphics/Resource/ResourceManager.h"
#include "Engine/Core/Math/StringHash.h"

struct ModelComponent
{
    // =================================================================
    // [Data Layout]
    // 合計 12 Bytes の完全な連続メモリ (ヒープアロケーション 0)
    // =================================================================
    ModelInstanceHandle modelHandle; // 8 byte (Index 4 + Gen 4)
    uint32_t assetHash = 0;          // 4 byte (文字列パスのハッシュ)

    ModelComponent() = default;

    // パスから初期化
    ModelComponent(const char* path) {
        SetModel(path);
    }

    // =================================================================
    // [Rule of 5] ECS Chunk 移動に伴うライフサイクル管理
    // =================================================================

    ~ModelComponent() {
        if (modelHandle.IsValid()) {
            ResourceManager::Instance().UnloadModelInstance(modelHandle);
            modelHandle = ModelInstanceHandle{};
        }
    }

    // 2. コピーコンストラクタ（★重要：複製されたら必ず新しい部屋を作る！）
    ModelComponent(const ModelComponent& other) : assetHash(other.assetHash) {
        if (assetHash != 0) {
            modelHandle = ResourceManager::Instance().CreateModelInstanceFromHash(assetHash);
        }
    }

    // コピー代入
    ModelComponent& operator=(const ModelComponent& other) {
        if (this != &other) {
            if (modelHandle.IsValid()) {
                ResourceManager::Instance().UnloadModelInstance(modelHandle);
            }
            assetHash = other.assetHash;
            if (assetHash != 0) {
                modelHandle = ResourceManager::Instance().CreateModelInstanceFromHash(assetHash);
            }
            else {
                modelHandle = ModelInstanceHandle{};
            }
        }
        return *this;
    }

    // 4. ムーブコンストラクタ（★超重要：noexcept を付ける！）
        // std::vector等がメモリ拡張で引っ越しをする際、noexceptがあればコピーではなくこれが呼ばれます。
        // 古い部屋を解約せず、所有権だけを安全に移動させます。
    ModelComponent(ModelComponent&& other) noexcept
        : modelHandle(other.modelHandle), assetHash(other.assetHash) {
        other.modelHandle = ModelInstanceHandle{}; // 元の所有権を剥奪
        other.assetHash = 0;
    }

    // ムーブ代入
    ModelComponent& operator=(ModelComponent&& other) noexcept {
        if (this != &other) {
            if (modelHandle.IsValid()) {
                ResourceManager::Instance().UnloadModelInstance(modelHandle);
            }
            modelHandle = other.modelHandle;
            assetHash = other.assetHash;

            other.modelHandle = ModelInstanceHandle{};
            other.assetHash = 0;
        }
        return *this;
    }

    // =================================================================
    // [Accessors]
    // =================================================================

    Model* GetModel() const {
        return ResourceManager::Instance().GetModelInstance(modelHandle);
    }

    void SetModel(const char* path) {
        if (modelHandle.IsValid()) {
            ResourceManager::Instance().UnloadModelInstance(modelHandle);
            modelHandle = ModelInstanceHandle{};
        }

        if (path && path[0] != '\0') {
            assetHash = CCL::Utils::HashString(path);
            modelHandle = ResourceManager::Instance().CreateModelInstance(path);
        }
        else {
            assetHash = 0;
        }
    }
};