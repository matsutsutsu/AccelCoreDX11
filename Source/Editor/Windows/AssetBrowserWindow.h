#pragma once
#include "Editor/Core/EditorWindow.h"
#include "Editor/Utils/AssetBrowser.h"       
#include "Engine/Graphics/Core/Graphics.h"   
#include <memory>

class AssetBrowserWindow : public EditorWindow {
private:
    // ウィンドウ自身がアセットブラウザの実体を所有する
    std::unique_ptr<AssetBrowser> _assetBrowser;

public:
    AssetBrowserWindow() : EditorWindow("Asset Browser") 
    {
        // ウィンドウが作られた時に、1回だけ初期化する
        // ※ フォルダパスはプロジェクト構造に合わせて "./Assets" または "./Data" などに変更する
        _assetBrowser = std::make_unique<AssetBrowser>("./Assets", Graphics::Instance().GetDevice());
    }

protected:
    void DrawContents(EditorContext& context) override 
    {
        // 自分の持っているアセットブラウザを描画するだけ！
        if (_assetBrowser) {
            _assetBrowser->Draw(); 
        }
    }
};