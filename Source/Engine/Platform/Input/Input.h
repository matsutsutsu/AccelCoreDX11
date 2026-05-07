#pragma once

#include <memory>
#include "GamePad.h"
#include "Mouse.h"
#include "Keyboard.h"

// インプット
class Input
{
private:
	Input() = default;
	~Input() = default;

public:
	// インスタンス取得
	static Input& Instance()
	{
		static Input instance;
		return instance;
	}

	// 初期化
	void Initialize(HWND hWnd);

	// 更新処理
	void Update();

	// ゲームパッド取得
	GamePad& GetGamePad() { return *gamePad; }

	// マウス取得
	Mouse& GetMouse() { return *mouse; }

	// キーボード取得 (追加)
    Keyboard &GetKeyboard() { return *keyboard; }

	// --- Logic更新用（固定フレーム用）の入力取得関数 ---
	// 瞬間的な入力（GetButtonDown）を取りこぼさないために、
	// ClearLogicInput() が呼ばれるまでフラグを保持し続けます。
	unsigned int GetPadButtonDownLogic() const { return _logicPadButtonDown; }
	unsigned int GetMouseButtonDownLogic() const { return _logicMouseButtonDown; }

	// Logic更新が終わったら呼ぶリセット関数
	void ClearLogicInput();

private:
	std::unique_ptr<GamePad>	gamePad;
	std::unique_ptr<Mouse>		mouse;
    std::unique_ptr<Keyboard>   keyboard;

	// --- 入力蓄積用変数 ---
	unsigned int _logicPadButtonDown = 0;
	unsigned int _logicMouseButtonDown = 0;

};
