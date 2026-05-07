#pragma once
#include <variant>
#include "SimpleMath.h"

// --- ステート定義 ---
struct StateIdle {};
struct StateMove {};

struct StateDodge
{
    struct Config
    {
        // 加速・ブリンク設定
        float maxTurnAngle = DirectX::XM_PIDIV4;
        float dashSpeed = 45.0f;        // 突進時の速度
        float dashFriction = 15.0f;     // 加速終了後の減速力（慣性の残り具合
        float duration = 0.25f;
        float usedStamina = 15;
    } config;
    bool isDashing = false;         // 現在、急加速中かどうかのフラグ
    DirectX::XMFLOAT3 dashDir = {}; // 加速する方向
    DirectX::XMFLOAT3 baseDashDir; // 最初のフレームで決めた基準方向（固定）
    bool isInitialized = false; // ← これを追加
    // 旋回制限 45度
};

struct StateAttack
{
    struct Config
    {
        float lungeSpeed = 10.0f;     // 踏み込み速度
        float lungerange= 0.3f;     // 踏み込みの最大距離
        float rotationSpeed = 10.0f;  // 攻撃中の敵への振り向き速度
        int maxComboCount = 3;
    } config;
    float currentLungeDist = 0.0f;
    int comboCount = 0;
    bool hasNextComboBuffered = false;
    bool hasDodgeBuffered = false;

    // ★ 追加：空中攻撃かどうか
    bool isAirAttack = false;
};

// ロックオン（エイム）状態：ターゲットを蓄積する
struct StateTargeting
{
    struct Config
    {
        float staminaCostPerSec = 5.0f; // ロックオン中の秒間スタミナ消費
        float maxRange = 50.0f;         // 初期最大射程
    } config;

    // 蓄積されたターゲット情報
    static constexpr int MAX_TARGETS = 20;
    CCL::ECS::EntityID targets[MAX_TARGETS] = {};
    uint32_t targetCount = 0;
    float remainingRange = 0.0f;
};

// チェインアタック状態：蓄積したターゲットを順番にワープ攻撃
struct StateChainAttack
{
    struct Config
    {
        float warpInterval = 0.6f;   // 次の敵へ飛ぶ間隔
        float pauseDuration = 0.2f;  //敵に到達した後の待機時間
        float finishDuration = 0.5f; // 全員斬った後の硬直
    } config;

    CCL::ECS::EntityID targets[StateTargeting::MAX_TARGETS] = {};
    uint32_t targetCount = 0;
    uint32_t currentIndex = 0;
    float warpTimer = 0.0f;
    float waitTimer = 0.0f;           //待機用タイマー
    bool isWaiting = false;           //待機中かどうかのフラグ
    DirectX::XMFLOAT3 startPosition = {}; // 移動開始時の座標
};

// タグコンポーネント：システムがこれを見て処理を分岐させる
namespace PlayerStateTag
{
    struct IsDashingTag {};
    struct IsAttackingTag {};
    struct IsLockOnTag {};
    struct IsChainAttackTag {};
    // プレイヤーに付与する「反撃の猶予」コンポーネント
    struct CounterAttackOpportunity {
        float remainingTime = 0.5f; // 回避後、0.5秒間はいつでも反撃可能にする
    };

}


/**
 * プレイヤーの状態を保持するPODコンポーネント
 */
struct TPSPlayerStateComponent 
{
    // 現在のアクティブなステート
    std::variant<StateIdle, StateMove, StateDodge, StateAttack,StateTargeting, StateChainAttack> activeState = StateIdle{};
    // これなら GUI で「Dodge設定」「Attack設定」と並べて表示しやすい
    struct {
        StateDodge::Config dodge;
        StateAttack::Config attack;
        StateAttack::Config airAttack;
        StateAttack::Config counterAttack;
        StateTargeting::Config Targeting;
        StateChainAttack::Config chain;
        // 他のステートの設定もここに追加
    } configs;
    float stateTimer = 0.0f;
    bool isFinished = false; // SystemからStateSystemへ終了を伝えるフラグ
    //ステートに関わらず保証される「最低限の移動重み」
    // 1.0 = 入力通りに動く, 0.0 = ステートの動きに完全に従う
    float mobilityWeight = 1.0f;
};