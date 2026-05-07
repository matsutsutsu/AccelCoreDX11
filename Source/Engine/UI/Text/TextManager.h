#pragma once

#include "BitmapFont.h"
#include "Engine/Graphics/Core/Graphics.h"
#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include "Engine/Graphics/Core/Camera.h"

struct TextParams {
    std::string       text = "";
    float             x = 0, y = 0;
    float             width = 32, height = 32;
    DirectX::XMFLOAT4 color       = {1, 1, 1, 1};
    bool              centerAlign = false;
};

// 投げっぱなしエフェクト用
struct FloatingText {
    TextParams        params;
    DirectX::XMFLOAT3 worldPos;
    DirectX::XMFLOAT3 velocity;
    float             lifetime    = 1.0f;
    float             maxLifetime = 1.0f;
    bool              isDead      = false;
};

// 持続する3Dテキスト用 (FloatingTextSystemで使用)
struct WorldText {
    std::string       text;
    DirectX::XMFLOAT3 worldPos;
    float             width = 32, height = 32;
    DirectX::XMFLOAT4 color       = {1, 1, 1, 1};
    bool              centerAlign = true;
};

class TextManager {
  private:
    std::shared_ptr<BitmapFont> _font;
    Camera                     *_camera = nullptr;

    // 1. UI用テキスト（2D, ID管理）
    std::unordered_map<std::string, TextParams> _uiTexts;

    // 2. エフェクト用テキスト（3D, 投げっぱなし）
    std::vector<FloatingText> _floatingTexts;

    // 3. 持続する3Dテキスト（3D, ID管理） ★追加
    std::unordered_map<std::string, WorldText> _worldTexts;

  public:
    TextManager()
    {
        auto &g = Graphics::Instance();
        _font =
            std::make_shared<BitmapFont>(g.GetDevice(), "Assets/Fonts/font.png", "Assets/Fonts/font.fnt");
    }

    void Initialize(Camera *camera)
    {
        _camera = camera;
        _uiTexts.clear();
        _floatingTexts.clear();
        _worldTexts.clear();
    }

    BitmapFont *GetFontSprite() const { return _font.get(); }

    // --- 2D UI操作 ---
    void AddText(const std::string &name, const TextParams &text) { _uiTexts[name] = text; }

    TextParams *GetText(const std::string &name)
    {
        auto it = _uiTexts.find(name);
        return (it != _uiTexts.end()) ? &it->second : nullptr;
    }

    // ★追加: HasText (ShopSceneなどで使用)
    bool HasText(const std::string &name) const { return _uiTexts.find(name) != _uiTexts.end(); }

    void RemoveText(const std::string &name) { _uiTexts.erase(name); }

    // --- 3D 持続テキスト操作 (FloatingTextSystem用) ---
    void AddWorldText(
        const std::string &name, const std::string &text, const DirectX::XMFLOAT3 &pos)
    {
        WorldText wt;
        wt.text           = text;
        wt.worldPos       = pos;
        _worldTexts[name] = wt;
    }

    void UpdateWorldTextPos(const std::string &name, const DirectX::XMFLOAT3 &pos)
    {
        auto it = _worldTexts.find(name);
        if (it != _worldTexts.end()) {
            it->second.worldPos = pos;
        }
    }

    void RemoveWorldText(const std::string &name) { _worldTexts.erase(name); }

    bool HasWorldText(const std::string &name) const
    {
        return _worldTexts.find(name) != _worldTexts.end();
    }

    // --- 3D エフェクト操作 (ダメージ用) ---
    void SpawnFloatingText(const std::string &str,
        const DirectX::XMFLOAT3              &pos,
        const DirectX::XMFLOAT3              &vel,
        float                                 life)
    {
        FloatingText ft;
        ft.params.text        = str;
        ft.params.centerAlign = true;
        ft.worldPos           = pos;
        ft.velocity           = vel;
        ft.lifetime           = life;
        ft.maxLifetime        = life;
        _floatingTexts.push_back(ft);
    }

    // --- 更新 & 描画 ---
    void Update(float dt)
    {
        for (auto &ft : _floatingTexts) {
            ft.lifetime -= dt;
            if (ft.lifetime <= 0) {
                ft.isDead = true;
                continue;
            }
            ft.worldPos.x += ft.velocity.x * dt;
            ft.worldPos.y += ft.velocity.y * dt;
            ft.worldPos.z += ft.velocity.z * dt;
            float alpha       = ft.lifetime / ft.maxLifetime;
            ft.params.color.w = alpha;
        }
        _floatingTexts.erase(std::remove_if(_floatingTexts.begin(),
                                 _floatingTexts.end(),
                                 [](const FloatingText &t) { return t.isDead; }),
            _floatingTexts.end());
    }

    void Render(bool renderWorldSpace)
    {
        auto                &g  = Graphics::Instance();
        ID3D11DeviceContext *dc = g.GetDeviceContext();

        _font->Begin();

        // 2D UI描画
        if (!renderWorldSpace) {
            for (const auto &[name, t] : _uiTexts) {
                _font->AddString(t.text, t.x, t.y, t.width, t.height, t.color, t.centerAlign);
            }
        }

        // 3D テキスト描画 (FloatingText + WorldText)
        if (renderWorldSpace && _camera) {
            float       screenW = g.GetScreenWidth();
            float       screenH = g.GetScreenHeight();
            const auto &view    = _camera->GetView();
            const auto &proj    = _camera->GetProjection();

            // エフェクト
            for (const auto &ft : _floatingTexts) {
                auto sPos = WorldToScreen(ft.worldPos, view, proj, screenW, screenH);
                if (sPos.x < -100 || sPos.x > screenW + 100) continue;
                _font->AddString(ft.params.text,
                    sPos.x,
                    sPos.y,
                    ft.params.width,
                    ft.params.height,
                    ft.params.color,
                    ft.params.centerAlign);
            }

            // 持続テキスト (Nameplate等)
            for (const auto &[name, wt] : _worldTexts) {
                auto sPos = WorldToScreen(wt.worldPos, view, proj, screenW, screenH);
                if (sPos.x < -100 || sPos.x > screenW + 100) continue;
                _font->AddString(
                    wt.text, sPos.x, sPos.y, wt.width, wt.height, wt.color, wt.centerAlign);
            }
        }
        _font->Flush(dc);
    }

  private:
    DirectX::XMFLOAT2 WorldToScreen(const DirectX::XMFLOAT3 &worldPos,
        const DirectX::XMFLOAT4X4                           &view,
        const DirectX::XMFLOAT4X4                           &proj,
        float                                                screenWidth,
        float                                                screenHeight)
    {
        DirectX::XMMATRIX viewMatrix = DirectX::XMLoadFloat4x4(&view);
        DirectX::XMMATRIX projMatrix = DirectX::XMLoadFloat4x4(&proj);
        DirectX::XMVECTOR pos        = DirectX::XMLoadFloat3(&worldPos);
        DirectX::XMVECTOR clip       = DirectX::XMVector3Transform(pos, viewMatrix * projMatrix);
        float             x          = DirectX::XMVectorGetX(clip);
        float             y          = DirectX::XMVectorGetY(clip);
        float             w          = DirectX::XMVectorGetW(clip);
        if (w <= 0.0f) return {-9999, -9999};
        x /= w;
        y /= w;
        float screenX = (x * 0.5f + 0.5f) * screenWidth;
        float screenY = (1.0f - (y * 0.5f + 0.5f)) * screenHeight;
        return {screenX, screenY};
    }
};