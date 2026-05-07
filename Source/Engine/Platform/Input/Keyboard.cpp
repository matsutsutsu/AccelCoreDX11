#include "Keyboard.h"

void Keyboard::Update()
{
    // 現在の状態を過去の状態として保存
    memcpy(oldState, currentState, sizeof(currentState));

    // 修正点：OSから全キーの状態を「1回の呼び出し」で配列として取得
    BYTE tempState[256];
    if (GetKeyboardState(tempState)) {
        for (int i = 0; i < 256; i++) {
            // GetKeyboardStateは最上位ビット(0x80)が立っていれば押下状態
            currentState[i] = (tempState[i] & 0x80) ? 1 : 0;
        }
    }
}

bool Keyboard::IsDown(int key) const
{
    // 範囲チェック推奨ですが、速度優先で省略する場合もあります
    if (key < 0 || key >= 256) return false;
    return currentState[key] != 0;
}

bool Keyboard::IsTriggered(int key) const
{
    if (key < 0 || key >= 256) return false;
    // 今押されていて、かつ 前は押されていなかった
    return (currentState[key] != 0) && (oldState[key] == 0);
}

bool Keyboard::IsReleased(int key) const
{
    if (key < 0 || key >= 256) return false;
    // 今押されていなくて、かつ 前は押されていた
    return (currentState[key] == 0) && (oldState[key] != 0);
}