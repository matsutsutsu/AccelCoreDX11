#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <DirectXMath.h>

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace ShaderReflect
{
    // 定数バッファ内の変数情報
    struct VariableInfo
    {
        std::string name;
        UINT offset = 0;                        // バッファ内のオフセット
        UINT size = 0;                          // 変数のサイズ(バイト)
        // 規定値として最も一般的な float を指定、または 0 で初期化
        D3D_SHADER_VARIABLE_TYPE type = D3D_SVT_VOID;
        UINT rows = 0;                          // 行数（行列用）
        UINT columns = 0;                       // 列数（行列用）
    };

    // 定数バッファ情報
    struct ConstantBufferInfo
    {
        std::string name;
        UINT bindPoint = 0;                             // レジスタ番号 (b0, b1, ...)
        UINT size = 0;                                  // バッファサイズ(バイト)
        std::vector<VariableInfo> variables;        // 含まれる変数のリスト

        // 名前で変数を検索
        const VariableInfo* FindVariable(const std::string& varName) const
        {
            for (const auto& var : variables)
            {
                if (var.name == varName) return &var;
            }
            return nullptr;
        }
    };

    // UAV情報（RWStructuredBufferなど）
    struct UnorderedAccessViewInfo {
        std::string name;
        UINT        bindPoint = 0; // u0, u1...
        UINT        size      = 0; // 定義されている場合
    };

    // テクスチャ情報
    struct TextureInfo
    {
        std::string name;
        UINT bindPoint = 0;                     // レジスタ番号 (t0, t1, ...)
        // シェーダーリソースビュー(SRV)をデフォルトとして想定
        D3D_SHADER_INPUT_TYPE type = D3D_SIT_CBUFFER;
        // 不明な次元として初期化
        D3D_SRV_DIMENSION dimension = D3D_SRV_DIMENSION_UNKNOWN;
    };

    // サンプラー情報
    struct SamplerInfo
    {
        std::string name;
        UINT bindPoint = 0;                 // レジスタ番号 (s0, s1, ...)
    };

    // リフレクション結果を保持するクラス
    class ReflectionData
    {
    public:
        ReflectionData() = default;

        // .csoファイルからリフレクション情報を取得
        // HLSLをコンパイルした,csoファイルを解析してConstantBufferInfoを生成する
        bool LoadFromCompiledFile(const char* filename);

        // バイトコードからリフレクション情報を取得
        bool LoadFromBytecode(const void* bytecode, size_t bytecodeSize);

        // 情報取得
        const std::vector<ConstantBufferInfo>& GetConstantBuffers() const { return constantBuffers; }
        const std::vector<TextureInfo>& GetTextures() const { return textures; }
        const std::vector<SamplerInfo>& GetSamplers() const { return samplers; }

        // 名前で検索
        const ConstantBufferInfo* FindConstantBuffer(const std::string& name) const;
        const ConstantBufferInfo* FindConstantBufferBySlot(UINT slot) const;
        const TextureInfo* FindTexture(const std::string& name) const;
        const SamplerInfo* FindSampler(const std::string& name) const;

        // テクスチャの名前からスロット番号(BindPoint)を取得
        int FindTextureSlot(const std::string& name) const
        {
            for (const auto& tex : textures)
            {
                if (tex.name == name) return (int)tex.bindPoint;
            }
            return -1; // 見つからない場合
        }

        // サンプラーの名前からスロット番号(BindPoint)を取得
        int FindSamplerSlot(const std::string& name) const
        {
            for (const auto& samp : samplers)
            {
                if (samp.name == name) return (int)samp.bindPoint;
            }
            return -1;
        }

        // デバッグ出力
        void PrintInfo() const;

        const std::vector<UnorderedAccessViewInfo> &GetUAVs() const { return uavs; }
        const UnorderedAccessViewInfo              *FindUAV(const std::string &name) const;

    private:
        void ExtractReflectionInfo(ID3D11ShaderReflection* reflection);

        std::vector<ConstantBufferInfo> constantBuffers;
        std::vector<TextureInfo> textures;
        std::vector<SamplerInfo> samplers;

        std::vector<UnorderedAccessViewInfo> uavs;
    };

    // 自動定数バッファ管理クラス
    class AutoConstantBuffer
    {
    public:
        AutoConstantBuffer() = default;

        // 定数バッファを作成
        bool Create(ID3D11Device* device, const ConstantBufferInfo& info);

        // 変数に値を設定（名前ベース）
        template<typename T>
        bool SetValue(const std::string& varName, const T& value)
        {
            // ★1: 変数の名前から、バッファ内の「位置(offset)」を探す
            const VariableInfo *varInfo = bufferInfo.FindVariable(varName);
            if (!varInfo) return false;
            if (sizeof(T) > varInfo->size) return false;

            // ★2: CPU側の作業台(cpuBuffer)の適切な位置に、データをメモリコピーする
            memcpy(cpuBuffer.data() + varInfo->offset, &value, sizeof(T));
            isDirty = true; // 「更新されたよ」というフラグを立てる
            return true;
        }

        // GPU側のバッファを更新
        void UpdateBuffer(ID3D11DeviceContext* dc);

        // バッファをバインド
        void BindVS(ID3D11DeviceContext* dc) const;
        void BindPS(ID3D11DeviceContext* dc) const;
        void BindGS(ID3D11DeviceContext* dc) const;
        void BindCS(ID3D11DeviceContext *dc) const;

        // 情報取得
        ID3D11Buffer* GetBuffer() const { return buffer.Get(); }
        UINT GetBindPoint() const { return bufferInfo.bindPoint; }
        const ConstantBufferInfo& GetInfo() const { return bufferInfo; }

        // ★追加: 構造体などの生データを直接セットする
        void SetRawData(const void *data, size_t size);

    private:
        Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
        ConstantBufferInfo bufferInfo;
        std::vector<BYTE> cpuBuffer;    // CPU側のバッファ
        bool isDirty = false;           // 更新が必要かどうか
    };
}