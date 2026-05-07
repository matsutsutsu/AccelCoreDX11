#include "Input.h"

// 初期化
void Input::Initialize(HWND hWnd)
{
	gamePad = std::make_unique<GamePad>();
	mouse = std::make_unique<Mouse>(hWnd);
    keyboard = std::make_unique<Keyboard>();
}

// 更新処理
void Input::Update()
{
	gamePad->Update();
	mouse->Update();
    keyboard->Update(); 

	// 2. Logic用の入力バッファに「蓄積」する
	//    |= (OR演算) を使うことで、Clearされるまでの間に一度でも押されれば
	//    フラグが立ったままになります。
	_logicPadButtonDown |= gamePad->GetButtonDown();
	_logicMouseButtonDown |= mouse->GetButtonDown();
}


// --- リセット処理 ---
void Input::ClearLogicInput()
{
	_logicPadButtonDown = 0;
	_logicMouseButtonDown = 0;
}