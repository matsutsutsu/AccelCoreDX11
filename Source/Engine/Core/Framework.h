#pragma once

#include <windows.h>
#include "Engine/Core/Time/HighResolutionTimer.h"
//#include "Scene.h"


class Framework
{
public:
	Framework(HWND hWnd);
	~Framework();

private:
	// 固定更新（物理・ロジック用）：1/60秒ごとに呼ばれる [新規]
	void FixedUpdate(float fixedTime);

	// 可変更新（描画準備・入力用）：毎フレーム呼ばれる [既存]
	void Update(float elapsedTime);

	// 描画実行：毎フレーム呼ばれる [既存]
	void Render(float elapsedTime);

	void CalculateFrameStats();

public:
	int Run();
	LRESULT CALLBACK HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
	const HWND				hWnd;
	HighResolutionTimer		timer;
	
	// 固定タイムステップ設定 (60FPS = 0.01666...秒)
	static constexpr float FIXED_DT = 1.0f / 60.0f;

	// デバッグ用：計測用変数
	int m_fixedUpdateCounter = 0;      // 1秒間に何回回ったか数える
	float m_debugTimer = 0.0f;         // 1秒経過計測用
	int m_displayFixedCount = 0;       // 表示用の確定値
};

