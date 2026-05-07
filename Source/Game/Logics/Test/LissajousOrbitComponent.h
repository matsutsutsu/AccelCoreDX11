#pragma once
#include <SimpleMath.h>

// リサージュ図形を描くためのパラメータ群
struct LissajousOrbitComponent {
    DirectX::SimpleMath::Vector3 amplitude; // X,Y,Z軸ごとの振幅（広がり）
    DirectX::SimpleMath::Vector3 frequency; // X,Y,Z軸ごとの周波数（速度）
    DirectX::SimpleMath::Vector3 phase;     // X,Y,Z軸ごとの初期位相（ズレ）
    float timeAcc;                          // 個体ごとの経過時間
    float speed;                            // 時間の進行速度
    DirectX::SimpleMath::Vector3 centerPos; // 軌道の中心座標
};