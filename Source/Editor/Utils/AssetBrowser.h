#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <d3d11.h>
#include <DirectXTex.h>
#include <wrl/client.h>

namespace fs = std::filesystem;

class AssetBrowser
{
public:
    AssetBrowser(const std::string& rootPath, ID3D11Device* device);

    void Draw();

private:
    struct AssetNode 
    {
        std::string name;         // ファイル/フォルダ名
        fs::path path;            // 実際のファイルパス
        bool isDirectory = false; // フォルダかどうか
        std::vector<AssetNode> children; // 子要素（フォルダ内のファイル/フォルダ）
    };

    struct IconTexture 
    {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
        int width = 0;
        int height = 0;
    };

    // ディレクトリを走査してノードを作成
    void LoadDirectory(const fs::path& dirPath, AssetNode& node);

    // 左ペイン（フォルダツリー）
    void DrawAssetNode(AssetNode& node);

    // 右ペイン（グリッドビュー）
    void DrawGridView(AssetNode& node);

    // アイコンロード系
    IconTexture LoadIcon(ID3D11Device* device, const std::wstring& path);
    void LoadIcons(ID3D11Device* device);

    // ユーティリティ
    AssetNode* FindNodeByPath(AssetNode& node, const fs::path& path);
    void DrawSearchBar();
    bool FilterAsset(const AssetNode& node);

private:
    AssetNode rootNode;                  // ルートノード
	AssetNode* selectedFolder = nullptr; // 選択中のフォルダ
    std::string rootPath;                // ルートパス
    std::string selectedFilePath;        // 選択中のファイル

    std::unordered_map<std::string, IconTexture> iconMap;       // カテゴリ→アイコン
    std::unordered_map<std::string, std::string> extensionMap;  // 拡張子→カテゴリ

    std::string searchQuery;                          // 検索クエリ
    std::unordered_map<std::string, bool> typeFilters; // 種別フィルタ

	ID3D11Device* device;            // D3D11デバイス（アイコン作成用）
};
