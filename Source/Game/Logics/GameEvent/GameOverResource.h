#pragma once

// ゲームオーバーの状態をシーンに伝えるためのリソース
struct GameOverResource {
    bool showScoreUI = false; // これがtrueになったらUIを出す
    int  finalScore  = 0;     // 必要ならスコアもここに渡す
};