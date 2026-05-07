#pragma once
#include <d3d11.h>
#include <wrl/client.h>

class RenderTarget
{
public:
    RenderTarget() = default;
    // デストラクタで確実に解放
    ~RenderTarget() { Release(); }

    // 中に残っている古いGPUリソースを手動で空にする関数
    void Release()
    {
        _texture.Reset();
        _rtv.Reset();
        _srv.Reset();
        _width  = 0;
        _height = 0;
    }

    // 作成 (ShadowMapのコンストラクタで行っている処理の汎用版)
    bool Create(ID3D11Device* device, UINT width, UINT height, DXGI_FORMAT format)
    {
        // 新しく作る前に、確実に古いデータを消し去る
        Release();

        _width = width;
        _height = height;
        _format = format;

        HRESULT hr;

        // 1. テクスチャ作成
        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = width;
        texDesc.Height = height;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = format;
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        // ReleaseAndGetAddressOf() を使って、確実に空にしてからアドレスを渡す
        hr = device->CreateTexture2D(&texDesc, nullptr, _texture.ReleaseAndGetAddressOf());
        if (FAILED(hr)) {
            // 失敗理由をデバッグ出力に出す
            char buf[256];
            sprintf_s(buf, "RenderTarget Create Texture Failed: HRESULT=0x%08X, W=%d, H=%d\n", hr, width, height);
            OutputDebugStringA(buf);
            return false;
        }

        // 2. RTV (書き込み用)
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = format;
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = 0;

        hr = device->CreateRenderTargetView(_texture.Get(), &rtvDesc, _rtv.ReleaseAndGetAddressOf());
        if (FAILED(hr)) {
            OutputDebugStringA("RenderTarget Create RTV Failed\n");
            return false;
        }

        // 3. SRV (読み込み用)
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;

        hr = device->CreateShaderResourceView(_texture.Get(), &srvDesc, _srv.ReleaseAndGetAddressOf());
        if (FAILED(hr)) {
            OutputDebugStringA("RenderTarget Create SRV Failed\n");
            return false;
        }

        // ビューポート設定
        _viewport.Width = (float)width;
        _viewport.Height = (float)height;
        _viewport.MinDepth = 0.0f;
        _viewport.MaxDepth = 1.0f;
        _viewport.TopLeftX = 0.0f;
        _viewport.TopLeftY = 0.0f;

        return true;
    }

    // クリア処理
    void Clear(ID3D11DeviceContext* dc, const float color[4])
    {
        dc->ClearRenderTargetView(_rtv.Get(), color);
    }

    // 書き込み先に設定
    void Activate(ID3D11DeviceContext* dc)
    {
        dc->RSSetViewports(1, &_viewport);
        dc->OMSetRenderTargets(1, _rtv.GetAddressOf(), nullptr); // 深度なし
    }

    // ゲッター
    ID3D11ShaderResourceView* GetSRV() const { return _srv.Get(); }
    ID3D11RenderTargetView* GetRTV() const { return _rtv.Get(); }
    UINT GetWidth() const { return _width; }
    UINT GetHeight() const { return _height; }

private:
    Microsoft::WRL::ComPtr<ID3D11Texture2D> _texture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> _rtv;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> _srv;
    D3D11_VIEWPORT _viewport = {};
    UINT _width = 0;
    UINT _height = 0;
    DXGI_FORMAT _format = DXGI_FORMAT_UNKNOWN;
};