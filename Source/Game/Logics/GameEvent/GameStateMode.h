#pragma once

enum class GameStateMode {
    Title,      // タイトル画面（操作不可・演出カメラ）
    Transition, // 遷移中（操作不可・カメラ移動中）
    Gameplay    // ゲーム中（操作可能・追従カメラ）
};

struct GameState {
    GameStateMode mode = GameStateMode::Title;
};