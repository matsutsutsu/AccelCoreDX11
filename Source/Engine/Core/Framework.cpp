#include <memory>
#include <sstream>
#include <imgui.h>
#include <algorithm> // min/max用
#include <string> // std::string, std::to_string用
#include <windowsx.h>

#include "Framework.h"
#include "Engine/Graphics/Core/Graphics.h"
#include "Engine/Graphics/Renderer/ImGuiRenderer.h"
#include "Engine/Platform/Input/Input.h"
#include "Engine/GamePlay/Core/Scene/SceneManager.h"

#include "Engine/Serialization/Factory/Prefab.h"                 
#include "Engine/Platform/Dialog.h"


#include "tracy/Tracy.hpp"


void InitializeDebugLayer(ID3D11Device *device)
{
    ID3D11InfoQueue *infoQueue = nullptr;
    device->QueryInterface(__uuidof(ID3D11InfoQueue), (void **)&infoQueue);

    if (infoQueue) {
        // 警告が出たらブレークポイントで止まるように設定
        infoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_WARNING, TRUE);
        infoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, TRUE);
        infoQueue->Release();
    }
}

// 垂直同期間隔設定
static const int syncInterval = 1;

// コンストラクタ
Framework::Framework(HWND hWnd)
	: hWnd(hWnd)
{

	ZoneScopedN("Framework::Constructor");


    // 音声初期化
   

	// グラフィックス初期化
	Graphics::Instance().Initialize(hWnd);
	// IMGUI初期化
	ImGuiRenderer::Initialize(hWnd, Graphics::Instance().GetDevice(), Graphics::Instance().GetDeviceContext());

	// ダイアログのファイル履歴をロード
	Dialog::LoadHistory();

	// シーンマネージャー初期化
	SceneManager::Instance().Initialize();	
	// Input 初期化
	Input::Instance().Initialize(hWnd);



}

// デストラクタ
Framework::~Framework()
{

	ZoneScopedN("Framework::Destrucor");


	// シーン終了化
	SceneManager::Instance().Clear();
	// IMGUI終了化
	ImGuiRenderer::Finalize();

	// ダイアログのファイル履歴を保存
	Dialog::SaveHistory();

	// 2. その後に、静的（static）なキャッシュを片付ける
    // これらが「デバイスへの参照」を握っている最後の犯人たちです。
    Prefab::Cleanup();

	// 音声終了化

}

// ---------------------------------------------------------
// 固定更新：物理演算などの「結果が変わってはいけない処理」
// ---------------------------------------------------------
void Framework::FixedUpdate(float fixedTime)
{
	// ここでは「物理・ロジック系」の更新のみを行う
	
    // 固定フレームごとの更新時間を計測
    ZoneScopedN("Framework::FixedUpdate");


	// SceneManager に FixedUpdate を追加して呼ぶのが理想
	// まだ実装していない場合は、一旦コメントアウトかエラー回避しておく
	 SceneManager::Instance().FixedUpdate(fixedTime); 


	 // 使い終わった入力フラグをここでリセット
	 Input::Instance().ClearLogicInput();
}

// ---------------------------------------------------------
// 可変更新：描画準備、入力、UIなど「毎フレーム行いたい処理」
// ---------------------------------------------------------
void Framework::Update(float elapsedTime)
{
    // 毎フレームのロジック更新時間を計測
    ZoneScopedN("Framework::Update");


	{
		ZoneScopedN("Framework::ImGuiRenderer::NewFrame");

		// IMGUIフレーム開始処理	
		ImGuiRenderer::NewFrame();
	}

	{

		ZoneScopedN("Framework::Input::Update");

		// インプット更新処理
		Input::Instance().Update();

	}

	{

		ZoneScopedN("Framework::SceneManager::Update");

		// シーン更新処理
		SceneManager::Instance().Update(elapsedTime);
	}
}

// 描画処理
void Framework::Render(float elapsedTime)
{
    // 描画コマンド発行全体を計測
    ZoneScopedN("Framework::Render");


	ID3D11DeviceContext* dc = Graphics::Instance().GetDeviceContext();

	// 画面クリア
	Graphics::Instance().Clear(0.5f, 0.5f, 0.5f, 1,false);

	// レンダーターゲット設定
	Graphics::Instance().SetRenderTargets(false);

	// シーンマネージャーを介して現在のシーンを描画
    {
            // シーンの描画（不透明、半透明、パーティクルなど）を細分化して計測
            ZoneScopedN("SceneManager::Render");

            SceneManager::Instance().Render();
    }


	// ImGuiの描画を計測
	{

		ZoneScopedN("FrameWork::ImGui");

		{
			ZoneScopedN("FrameWork::SceneManager::DrawGUI");
			SceneManager::Instance().DrawGUI();

		}


		{
			ZoneScopedN("FrameWork::ImGuiRenderer::Render");
			// IMGUI描画
			ImGuiRenderer::Render(dc);
		}

	}

	{
            // ★超重要: GPUの描画待ち（VSync同期）の時間を計測
            ZoneScopedN("Graphics::Present (VSync Wait)");

            // 画面表示
            Graphics::Instance().Present(syncInterval);
    }
	
}


// フレームレート計算
void Framework::CalculateFrameStats()
{
	// Code computes the average frames per second, and also the 
	// average time it takes to render one frame.  These stats 
	// are appended to the window caption bar.
	static int frames = 0;
	static float time_tlapsed = 0.0f;

	frames++;

	// Compute averages over one second period.
	if ((timer.TimeStamp() - time_tlapsed) >= 1.0f)
	{
		float fps = static_cast<float>(frames); // fps = frameCnt / 1
		float mspf = 1000.0f / fps;
		std::ostringstream outs;
		outs.precision(6);
		outs << "FPS : " << fps << " / " << "Frame Time : " << mspf << " (ms)";
		//SetWindowTextA(hWnd, outs.str().c_str());

		// Reset for next average.
		frames = 0;
		time_tlapsed += 1.0f;
	}
}

// ---------------------------------------------------------
// メインループ（Accumulator パターンの実装）
// ---------------------------------------------------------
int Framework::Run()
{
	MSG msg = {};

	// 時間の貯金箱
	double accumulator = 0.0;

	// 初回の時間計測
	timer.Tick();

	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			timer.Tick();
			CalculateFrameStats();

			// 1. 経過時間を取得
			float frameTime = timer.TimeInterval();

			// メモリリーク検出を有効化 (すでにあるはず)
			_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

			// ★追加: この番号のメモリ確保の瞬間にブレークする
			// ダンプに出ている番号（例: 2575）を指定します
			// ※実行するたびに番号が変わる場合は、この方法は使えません

			// TODO:シャドウマップでメモリリーク
			//_CrtSetBreakAlloc(121715);
			//_CrtSetBreakAlloc(121457);
			//_CrtSetBreakAlloc(121196);
			 
			//_CrtSetBreakAlloc(2275584);

			// 2. 「死の螺旋」防止（Safety Cap）
			// どんなに重くても、現実時間が 0.25秒 進んだことにして計算を打ち切る
			// これがないと、処理落ち→計算量増える→さらに処理落ち... で停止する
			if (frameTime > 0.1f) frameTime = 0.1f;

			// 3. 時間を貯める
			accumulator += frameTime;

			// 1秒ごとの計測処理
			m_debugTimer += frameTime;
			if (m_debugTimer >= 1.0f)
			{
				// ウィンドウタイトルにFPSとFixedUpdate回数を表示
				std::string title = "FPS: " + std::to_string(ImGui::GetIO().Framerate) +
					" | Fixed Updates: " + std::to_string(m_fixedUpdateCounter);
				SetWindowTextA(hWnd, title.c_str());

				// カウンタをリセット
				m_fixedUpdateCounter = 0;
				m_debugTimer = 0.0f;
			}

			// ループ回数制限用のカウンタ
			int updateCount = 0;
			const int MAX_STEPS = 4; // 1フレームに許容する最大更新回数


			while (accumulator >= FIXED_DT && updateCount < MAX_STEPS)
			{
				FixedUpdate(FIXED_DT);

				accumulator -= FIXED_DT;
				updateCount++;

				// カウントアップ
				m_fixedUpdateCounter++;
			}



			// もし最大回数を超えてまだ時間が余っている場合、
			// 無理に消化せず捨てる（処理落ち時にゲーム速度がスローになるが、カクつきは防げる）
			if (accumulator >= FIXED_DT)
			{
				// 残った時間を捨てる、あるいは次のフレームに持ち越すが
				// ここでは大きく溜まりすぎた分をカットしてリセットする例
				// accumulator = 0.0; // 完全に時間を捨てる場合

				// または、FIXED_DT未満になるように余りだけ残す（推奨）
				accumulator = fmod(accumulator, FIXED_DT);
			}



			// 5. 残った時間 (accumulator) を使って「補間」を行うのが理想だが、
			//    まずは可変フレームで描画更新を行う
			Update(frameTime);
			Render(frameTime);

			// ここで1フレーム終了をTracyに通知
            FrameMark;

		}
	}
	return static_cast<int>(msg.wParam);
}

// メッセージハンドラ
LRESULT CALLBACK Framework::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGuiRenderer::HandleMessage(hWnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_MOUSEWHEEL:
	{
		// 1カリッ（120）を WHEEL_DELTA（120）で割って、+1 または -1 にする
		int scroll = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
		Input::Instance().GetMouse().SetWheel(scroll);
		return 0;
	}
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc;
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;
	}
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_CREATE:
		break;
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE) PostMessage(hWnd, WM_CLOSE, 0, 0);
		break;
	case WM_ENTERSIZEMOVE:
		// WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
		timer.Stop();
		break;
	case WM_EXITSIZEMOVE:
		// WM_EXITSIZEMOVE is sent when the user releases the resize bars.
		// Here we reset everything based on the new window dimensions.
		timer.Start();
		break;
	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}
