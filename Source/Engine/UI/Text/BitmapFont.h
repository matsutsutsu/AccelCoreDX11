#pragma once

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "Engine/Graphics/Renderer/Sprite.h"
#include "Engine/Graphics/Core/Graphics.h"


class BitmapFont : public Sprite {
  public:
    struct Glyph {
        int   id;
        float x, y;
        float width, height;
        float xoffset, yoffset;
        float xadvance;
    };

  private:
    std::unordered_map<int, Glyph> _glyphs;
    float                          _base       = 0;
    float                          _lineHeight = 0;

    // ★追加: バッチ描画用の頂点バッファ（CPU側の一時置き場）
    std::vector<Vertex> _batchVertices;

    // GPU側の頂点バッファ
    Microsoft::WRL::ComPtr<ID3D11Buffer> _batchBuffer;

    const size_t MAX_BATCH_CHARS = 2048;

  public:
    BitmapFont(ID3D11Device *device, const char *textureFilename, const char *fntFilename)
        : Sprite(device, textureFilename)
    {
        LoadFNT(fntFilename);
        _batchVertices.reserve(MAX_BATCH_CHARS * 6);
        CreateBatchBuffer(device);
    }

    // --------------------------------------------------------------------------
    // バッチ描画システム
    // --------------------------------------------------------------------------
    void Begin() { _batchVertices.clear(); }

    void AddString(const std::string &text,
        float                         x,
        float                         y,
        float                         w,
        float                         h,
        const DirectX::XMFLOAT4      &color,
        bool                          isCentered = false)
    {
        std::u32string utf32 = UTF8toUTF32(text);
        AddStringUTF32(utf32, x, y, w, h, color, isCentered);
    }

    void Flush(ID3D11DeviceContext *dc)
    {
        if (_batchVertices.empty()) return;
        if (!_batchBuffer) return;

        D3D11_MAPPED_SUBRESOURCE mapped;
        HRESULT hr = dc->Map(_batchBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (SUCCEEDED(hr)) {
            size_t vertexCount = _batchVertices.size();
            size_t copySize    = sizeof(Vertex) * vertexCount;
            size_t maxBytes    = sizeof(Vertex) * MAX_BATCH_CHARS * 6;
            if (copySize > maxBytes) {
                vertexCount = (MAX_BATCH_CHARS * 6);
                copySize    = maxBytes;
            }
            memcpy(mapped.pData, _batchVertices.data(), copySize);
            dc->Unmap(_batchBuffer.Get(), 0);
            RenderBatchInternal(dc, vertexCount);
        }
    }

    // --------------------------------------------------------------------------
    // ★互換用ラッパー関数 (UITextなどで使われている)
    // --------------------------------------------------------------------------
    void Textout(ID3D11DeviceContext *dc,
        const std::string            &text,
        float                         x,
        float                         y,
        float                         w,
        float                         h,
        float                         r,
        float                         g,
        float                         b,
        float                         a)
    {
        Begin();
        AddString(text, x, y, w, h, {r, g, b, a}, false);
        Flush(dc);
    }

    void TextoutCenter(ID3D11DeviceContext *dc,
        const std::string                  &text,
        float                               x,
        float                               y,
        float                               w,
        float                               h,
        float                               r,
        float                               g,
        float                               b,
        float                               a)
    {
        Begin();
        AddString(text, x, y, w, h, {r, g, b, a}, true);
        Flush(dc);
    }


    // どこからでも使える static な翻訳関数
    static std::u32string UTF8toUTF32(const std::string &utf8)
    {
        std::u32string utf32;
        size_t         i = 0;
        while (i < utf8.size()) {
            uint32_t      codepoint = 0;
            unsigned char c         = utf8[i];
            if (c <= 0x7F) {
                codepoint = c;
                i += 1;
            }
            else if ((c & 0xE0) == 0xC0) {
                codepoint = ((utf8[i] & 0x1F) << 6) | (utf8[i + 1] & 0x3F);
                i += 2;
            }
            else if ((c & 0xF0) == 0xE0) {
                codepoint =
                    ((utf8[i] & 0x0F) << 12) | ((utf8[i + 1] & 0x3F) << 6) | (utf8[i + 2] & 0x3F);
                i += 3;
            }
            else if ((c & 0xF8) == 0xF0) {
                codepoint = ((utf8[i] & 0x07) << 18) | ((utf8[i + 1] & 0x3F) << 12) |
                            ((utf8[i + 2] & 0x3F) << 6) | (utf8[i + 3] & 0x3F);
                i += 4;
            }
            else {
                i += 1;
            }
            utf32.push_back(static_cast<char32_t>(codepoint));
        }
        return utf32;
    }

    // 翻訳済み(UTF32)の文字列を直接受け取る関数を用意
    void AddStringUTF32(const std::u32string &utf32,
        float                                 x,
        float                                 y,
        float                                 w,
        float                                 h,
        const DirectX::XMFLOAT4              &color,
        bool                                  isCentered = false)
    {
        float          sy                 = (_lineHeight > 0) ? (h / _lineHeight) : 1.0f;
        float          sx                 = sy;
        float          fallbackSpaceWidth = 20.0f * sx;
        float          currentY           = y;
        std::u32string lineBuffer;

        auto ProcessLine = [&](const std::u32string &line) {
            if (line.empty()) return;

            float startX = x;
            if (isCentered) {
                float lineWidth = 0.0f;
                for (char32_t c : line) {
                    if (_glyphs.count(c))
                        lineWidth += _glyphs.at(c).xadvance * sx;
                    else
                        lineWidth += fallbackSpaceWidth;
                }
                startX = x - (lineWidth / 2.0f);
            }

            float cursorX = 0.0f;
            for (char32_t c : line) {
                if (_glyphs.count(c)) {
                    const Glyph &g     = _glyphs.at(c);
                    float        drawX = startX + cursorX + g.xoffset * sx;
                    float        drawY = currentY + (_base - g.yoffset) * sy;
                    float        drawW = g.width * sx;
                    float        drawH = g.height * sy;
                    PushQuad(drawX, drawY, drawW, drawH, g.x, g.y, g.width, g.height, color);
                    cursorX += g.xadvance * sx;
                }
                else {
                    cursorX += fallbackSpaceWidth;
                }
            }
        };

        for (char32_t c : utf32) {
            if (c == '\n') {
                ProcessLine(lineBuffer);
                lineBuffer.clear();
                currentY += _lineHeight * sy;
            }
            else {
                lineBuffer.push_back(c);
            }
        }
        if (!lineBuffer.empty()) ProcessLine(lineBuffer);
    }

  private:
    void CreateBatchBuffer(ID3D11Device *device)
    {
        D3D11_BUFFER_DESC buffer_desc = {};
        buffer_desc.ByteWidth         = sizeof(Vertex) * MAX_BATCH_CHARS * 6;
        buffer_desc.Usage             = D3D11_USAGE_DYNAMIC;
        buffer_desc.BindFlags         = D3D11_BIND_VERTEX_BUFFER;
        buffer_desc.CPUAccessFlags    = D3D11_CPU_ACCESS_WRITE;
        buffer_desc.MiscFlags         = 0;
        device->CreateBuffer(&buffer_desc, nullptr, _batchBuffer.GetAddressOf());
    }

    void PushQuad(float          dx,
        float                    dy,
        float                    dw,
        float                    dh,
        float                    sx,
        float                    sy,
        float                    sw,
        float                    sh,
        const DirectX::XMFLOAT4 &color)
    {
        float texW = GetWidth();
        float texH = GetHeight();
        float u0   = sx / texW;
        float v0   = sy / texH;
        float u1   = (sx + sw) / texW;
        float v1   = (sy + sh) / texH;

        // =======================================================
        // ピクセル座標を NDC（-1.0 ~ 1.0）空間に変換する
        // =======================================================
        float screenWidth  = (float)Graphics::Instance().GetScreenWidth();
        float screenHeight = (float)Graphics::Instance().GetScreenHeight();

        // クリップ空間（NDC）への変換計算式
        auto ToNDC_X = [&](float x) { return (x / screenWidth) * 2.0f - 1.0f; };
        auto ToNDC_Y = [&](float y) { return 1.0f - (y / screenHeight) * 2.0f; };

        float nx0 = ToNDC_X(dx);
        float ny0 = ToNDC_Y(dy);
        float nx1 = ToNDC_X(dx + dw);
        float ny1 = ToNDC_Y(dy + dh);

        // 変換した座標(nx, ny)を使って頂点を作成する
        Vertex v0_ = {{nx0, ny0, 0}, color, {u0, v0}};
        Vertex v1_ = {{nx1, ny0, 0}, color, {u1, v0}};
        Vertex v2_ = {{nx0, ny1, 0}, color, {u0, v1}};
        Vertex v3_ = {{nx1, ny1, 0}, color, {u1, v1}};

        _batchVertices.push_back(v0_);
        _batchVertices.push_back(v1_);
        _batchVertices.push_back(v2_);
        _batchVertices.push_back(v1_);
        _batchVertices.push_back(v3_);
        _batchVertices.push_back(v2_);
    }

    void RenderBatchInternal(ID3D11DeviceContext *dc, size_t vertexCount)
    {
        // Spriteクラスのメンバにアクセス (protectedに変更されている前提)
        UINT stride = sizeof(Vertex);
        UINT offset = 0;
        dc->IASetVertexBuffers(0, 1, _batchBuffer.GetAddressOf(), &stride, &offset);
        if (inputLayout) dc->IASetInputLayout(inputLayout.Get());
        dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        if (vertexShader) dc->VSSetShader(vertexShader.Get(), nullptr, 0);
        if (pixelShader) dc->PSSetShader(pixelShader.Get(), nullptr, 0);
        if (shaderResourceView) dc->PSSetShaderResources(0, 1, shaderResourceView.GetAddressOf());
        dc->Draw(static_cast<UINT>(vertexCount), 0);
    }

    void LoadFNT(const std::string &filename)
    {
        std::ifstream ifs(filename);
        if (!ifs.is_open()) return;
        std::string line;
        while (std::getline(ifs, line)) {
            if (line.rfind("common ", 0) == 0) {
                std::istringstream iss(line);
                std::string        token;
                while (iss >> token) {
                    auto pos = token.find('=');
                    if (pos == std::string::npos) continue;
                    std::string key   = token.substr(0, pos);
                    int         value = std::stoi(token.substr(pos + 1));
                    if (key == "base") _base = (float)value;
                    if (key == "lineHeight") _lineHeight = (float)value;
                }
            }
            if (line.rfind("char ", 0) == 0) {
                std::istringstream iss(line);
                std::string        token;
                Glyph              g{};
                while (iss >> token) {
                    auto pos = token.find('=');
                    if (pos == std::string::npos) continue;
                    std::string key   = token.substr(0, pos);
                    int         value = std::stoi(token.substr(pos + 1));
                    if (key == "id") g.id = value;
                    if (key == "x") g.x = (float)value;
                    if (key == "y") g.y = (float)value;
                    if (key == "width") g.width = (float)value;
                    if (key == "height") g.height = (float)value;
                    if (key == "xoffset") g.xoffset = (float)value;
                    if (key == "yoffset") g.yoffset = (float)value;
                    if (key == "xadvance") g.xadvance = (float)value;
                }
                if (g.id != 0) _glyphs[g.id] = g;
            }
        }
    }
};