#pragma once
#include <DirectXMath.h>
#include <SimpleMath.h>

// ===================================================================================
// 【 AI データ構造 (Components) 定義 】
//
// [ アーキテクチャの基本思想 (SoA: Structure of Arrays) ]
// AIを1つの巨大なクラス（Monsterクラス等）にするのではなく、役割ごとに純粋なデータ（POD）の塊に解体しています。
// これによりCPUキャッシュヒット率が極限まで高まり、1万体のAIでも処理落ちしない基盤を実現しています。
//
// 🧠 AIStateComponent      : 「今何をしているか」（FSMの現在状態とタイマー）
// 👁️ AIPerceptionComponent : 「感覚器のスペック」（視界の広さ、耳の良さ）
// 📚 AIMemoryComponent     : 「過去の記憶と適応」（プレイヤーを最後に見た場所、騙された回数など、メタAI用データ）
// ===================================================================================

// ============================================================================
// [脳の状態] 現在AIが何を実行しているか (FSM: Finite State Machine)
// ============================================================================
enum class AIState {
    Idle,         // 待機
    Patrol,       // 巡回（Day1のメイン）
    Investigate,  // 調査（音や不審な場所へ向かう）
    Chase,        // 追跡（視界に入ったプレイヤーを追う）
    AttackDoor,   // 閉じ込められた際に扉を破壊する
    AmbushDuct    // ダクト出口での待ち伏せ
};

struct AIStateComponent {
    AIState currentState = AIState::Patrol;
    float timeInState = 0.0f;

    // パラメータ (待機・索敵用)
    float patrolWaitTime = 2.0f;
    float patrolRadius = 15.0f;

    // 各ステートにおける固有の移動速度 (m/s)
    float patrolSpeed = 1.5f;       // 巡回時の歩行速度
    float investigateSpeed = 2.5f;  // 調査時の警戒歩行速度
    float chaseSpeed = 5.0f;        // 追跡時のダッシュ速度
};

// ============================================================================
// [知覚能力] AIの感覚器の強さ（Day進行やディレクターAIによって強化される）
// ============================================================================
struct AIPerceptionComponent {
    float visionRange = 0.0f;   // 視界の距離 (m)
    float visionAngle = 45.0f;  // 視界の広さ (度)
};

// ============================================================================
// [記憶と適応] プレイヤーの行動履歴と感情パラメータ (Utility AIの評価基準)
// ============================================================================
struct AIMemoryComponent {
    // 最後にプレイヤー（または音源）を確認した座標
    DirectX::SimpleMath::Vector3 lastKnownPos = { 0.0f, 0.0f, 0.0f };

    // プレイヤーへの怒り・警戒度 (0.0〜100.0)。移動速度や扉破壊速度に影響する。
    float currentAngerLevel = 0.0f;

    // --- ダイエジェティック適応システム用 (プレイヤーの癖の学習) ---
    int trickedBySoundCount = 0; // 音源アイテム(タイマー等)に騙された回数
    int ductUsageObserved = 0;   // ダクト利用を目撃した回数
};