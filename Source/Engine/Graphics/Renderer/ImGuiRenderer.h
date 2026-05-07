#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <imgui.h> // ImFont のために必要

class ImGuiRenderer
{
public:
	// 初期化
	static void Initialize(HWND hWnd, ID3D11Device* device, ID3D11DeviceContext* dc);
	
	// 終了化
	static void Finalize();

	// フレーム開始処理
	static void NewFrame();

	// 描画実行
	static void Render(ID3D11DeviceContext* context);

	// WIN32メッセージハンドラー
	static LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	// エディタ用フォントを取得する関数  
    static ImFont *GetEditorFont() { return s_editorFont; }

	private:
    static ImFont *s_editorFont; // 保持用ポインタ
};
