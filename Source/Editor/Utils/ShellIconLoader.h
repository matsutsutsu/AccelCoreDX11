#pragma once
#include <string>
#include <unordered_map>
#include <d3d11.h>
#include <wrl/client.h>

class ShellIconLoader
{
public:
    // ファイル/フォルダどちらも対応
    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetIconSRV(
        const std::wstring& filePath, ID3D11Device* device);



private:
    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> LoadShellIconSRV(
        const std::wstring& filePath, ID3D11Device* device);

    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> LoadFolderIconSRV(
        const std::wstring& folderPath, ID3D11Device* device);

    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateSRVFromHICON(
        ID3D11Device* device, HICON hIcon);

    static std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> cache;
    static std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> folderCache;
};
