#pragma once
#include <Windows.h>

class Keyboard {
  public:
    Keyboard()  = default;
    ~Keyboard() = default;

    // 更新処理
    void Update();

    // キーが押されているか（押しっぱなし）
    bool IsDown(int key) const;

    // キーが押された瞬間か
    bool IsTriggered(int key) const;

    // キーが離された瞬間か
    bool IsReleased(int key) const;

  private:
    // 現在のフレームのキー状態 (0: 押されていない, 1: 押されている)
    BYTE currentState[256] = {0};

    // 1フレーム前のキー状態
    BYTE oldState[256] = {0};
};