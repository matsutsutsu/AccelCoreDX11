#pragma once

#include <Windows.h>
#include <DirectXMath.h>


using MouseButton = unsigned int;

// マウス
class Mouse
{
public:
	static const MouseButton BTN_LEFT = (1 << 0);
	static const MouseButton BTN_MIDDLE = (1 << 1);
	static const MouseButton BTN_RIGHT = (1 << 2);

public:
	Mouse(HWND hWnd);
	~Mouse() {}

	// 更新
	void Update();

	// ボタン入力状態の取得
	MouseButton GetButton() const { return buttonState[0]; }

	// ボタン押下状態の取得
	MouseButton GetButtonDown() const { return buttonDown; }

	// ボタン押上状態の取得
	MouseButton GetButtonUp() const { return buttonUp; }

	// 押しっぱなし判定 (IsDown)
    bool IsDown(MouseButton button) const { 
        return (buttonState[0] & button) != 0; 
    }

    // 押した瞬間判定 (IsTriggered / IsPressed)
    bool IsTriggered(MouseButton button) const { 
        return (buttonDown & button) != 0; 
    }

    // 離した瞬間判定 (IsReleased)
    bool IsReleased(MouseButton button) const { 
        return (buttonUp & button) != 0; 
    }

	// ホイール値の設定
	void SetWheel(int wheel) { this->wheel[0] += wheel; }

	// ホイール値の取得
	int GetWheel() const { return wheel[1]; }

	// マウスホイールが上方向に回された瞬間
	bool IsWheelUp() const { return wheel[1] > 0; }

	// マウスホイールが下方向に回された瞬間
	bool IsWheelDown() const { return wheel[1] < 0; }

	// マウスカーソルの座標取得
	DirectX::XMFLOAT2 GetPosition() const {
		return DirectX::XMFLOAT2(static_cast<float>(positionX[0]), static_cast<float>(positionY[0]));
	}

	// 前回のマウスカーソルの座標取得
	DirectX::XMFLOAT2 GetOldPosition() const {
		return DirectX::XMFLOAT2(static_cast<float>(positionX[1]), static_cast<float>(positionY[1]));
	}

	// マウスカーソルX座標取得
	int GetPositionX() const { return positionX[0]; }

	// マウスカーソルY座標取得
	int GetPositionY() const { return positionY[0]; }

	// 前回のマウスカーソルX座標取得
	int GetOldPositionX() const { return positionX[1]; }

	// 前回のマウスカーソルY座標取得
	int GetOldPositionY() const { return positionY[1]; }

	// =========================================================
	// マウスカーソルの移動量（速度）を取得
	// =========================================================
	int GetVelocityX() const { return positionX[0] - positionX[1]; }
	int GetVelocityY() const { return positionY[0] - positionY[1]; }

	// スクリーン幅設定
	void SetScreenWidth(int width) { screenWidth = width; }

	// スクリーン高さ設定
	void SetScreenHeight(int height) { screenHeight = height; }

	// スクリーン幅取得
	int GetScreenWidth() const { return screenWidth; }

	// スクリーン高さ取得
	int GetScreenHeight() const { return screenHeight; }

private:
	MouseButton		buttonState[2] = { 0 };
	MouseButton		buttonDown = 0;
	MouseButton		buttonUp = 0;
	int				positionX[2];
	int				positionY[2];
	int				wheel[2];
	int				screenWidth = 0;
	int				screenHeight = 0;
	HWND			hWnd = nullptr;
};
