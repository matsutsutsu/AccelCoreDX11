/**
 * @file DroneComponent.h
 * @brief ドローンの物理状態・フォーメーション状態を管理するコンポーネント
 * * @note 【アーキテクチャ上の注意】
 * このコンポーネントはPOD（Plain Old Data）であり、自身のAIロジックを持たない。
 * 振付師（FormationSystem）が目標座標を書き込み、
 * 実行者（MovementSystem）がそれに向かって物理移動を行うための「共有メモリ」として機能する。
 */
#pragma once
#include <cstdint>
#include "SimpleMath.h"
#include "Engine/Serialization/Meta/ComponentMeta.h"
#include "ECS/Common/CCL_Common.h"

 // ボスからドローン群への指示（※BossCommandComponent側でもこれを利用する）
enum class DroneFormationType : uint8_t {
    Hidden,          ///< ボスに収納されている状態
    OrbitCircle,     ///< ボスを中心に円陣を描く
    SequentialAttack,///< 順番にプレイヤーへ突撃する
    DeathRing,       ///< プレイヤーの周囲を囲む「処刑の輪」
    AegisShield      ///< ボスの前面に壁を作る「絶対防衛陣形」
};

// ドローン個体の具体的な物理ステート
enum class DroneState : uint8_t {
    /// 【有機的追従】スプリング・ダンパー制御でフワフワと目標に追従する。基本となる待機・陣形維持状態。
    Idle,

    /// 【機械的移動】Lerpによる直線的で滑らかな移動。素早く指定位置にビシッと整列したい場合（盾など）に使う。
    MoveToTarget,

    /// 【タメ・静止】一切移動せずその場に留まる。突撃前のエネルギー充填や、時間差攻撃の待機状態。
    LockOn,

    /// 【高速突撃】現在の目標座標へ向かって等速直線運動でカッ飛んでいく。攻撃状態。
    FireCharge
};

/**
 * @struct DroneComponent
 * @brief ドローン1体ごとの状態と目標座標を保持する構造体
 */
struct DroneComponent {
    // =========================================================
    // [1] 識別・階層データ (Identity & Hierarchy)
    // 自身が誰の部下で、何番目の機体かを示す不変的なデータ
    // =========================================================
    CCL::ECS::EntityID ownerBossId = CCL::ECS::InvalidEntityID;
    uint16_t localIndex = 0;
    uint16_t totalDrones = 1;

    // =========================================================
    // [2] 論理ステート (AI State)
    //  FormationSystem が管理・更新するAI的な状態
    // =========================================================
    DroneFormationType currentFormation = DroneFormationType::Hidden;
    DroneState currentState = DroneState::Idle;
    float stateTimer = 0.0f; // 突撃のディレイなどに使うタイマー

    // =========================================================
    // [3] 出力データ (Command Output)
    // FormationSystem が計算して書き込み、実行部隊（MovementSystem）が読み取る「伝言」
    // =========================================================
    DirectX::SimpleMath::Vector3 targetPosition = DirectX::SimpleMath::Vector3::Zero;

    // =========================================================
    // [4] 物理シミュレーション用・内部状態 (Physics Internal)
    // 実行部隊（MovementSystem）だけが読み書きする、物理挙動の記憶
    // =========================================================
    DirectX::SimpleMath::Vector3 currentVelocity = { 0,0,0 };
    float hoverTimeOffset = 0.0f; // 浮遊感の個体差を生むためのノイズシード

    // =========================================================
    // [5] 個体パラメータ (Settings)
    // インスペクタから設定される動作パラメータ
    // =========================================================
    float moveSpeed = 30.0f;
    float orbitRadius = 8.0f;
};