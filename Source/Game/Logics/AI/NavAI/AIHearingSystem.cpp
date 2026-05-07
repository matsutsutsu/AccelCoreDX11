#include "AIHearingSystem.h"
#include "ECS/Core/CCL_World.h"
#include "Engine/Platform/Logger.h"
#include "SimpleMath.h"

#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

using namespace DirectX::SimpleMath;
using namespace CCL::ECS;

// ===================================================================================
// 【 AI感覚器 (耳) : AIHearingSystem 】
//
// [ 役割 ]
// 空間で発生した「音の波紋(Event)」を検知し、AIを不審な場所へ向かわせる。
//
// [ ECSデータパイプライン ]
// 📥 READ  : TransformComponent      (AI自身の位置)
// 📥 READ  : AIPerceptionComponent   (AIの聴力：聞き取れる半径)
// 📤 WRITE : AIMemoryComponent       (音が鳴った座標を記憶、疑心暗鬼レベルの更新)
// 📤 WRITE : AIStateComponent        (音を聞いた時に Investigate(調査) へ状態遷移)
// 📤 WRITE : NavAgentComponent       (目標座標を音が鳴った位置へ更新)
//
// [ 内部挙動の直感的な解説 ]
// プレイヤーや環境が発した AISoundEvent を EventBus 経由で受け取ります。
// AI自身の座標から音源までの距離を測り、「音の大きさ」と「AIの聴力」の範囲内であれば検知します。
// 視覚(Chase)が最も優先されるため、既にプレイヤーを追いかけている最中は音を無視します。
// ===================================================================================

void AIHearingSystem::Initialize() {
    // 空間で発生した音をすべて集約する
    // 【解説】 EventBus(郵便局)に対して、
    // 「AISoundEventという手紙が来たら、この処理(ラムダ式)を実行してね」と契約を結びます。
    ListenEvent<AISoundEvent>([this](const AISoundEvent& e) {
        // 【解説】 ここは「メインスレッド」ではなく、物理衝突などを処理している「別のワーカースレッド」から突然呼ばれる可能性があります。
        // そのため、郵便受け（m_frameEvents）を同時に触ってデータが壊れないよう、一時的に鍵（Mutex）をかけます。
        std::lock_guard<std::mutex> lock(m_eventMutex);

        // 【解説】 受け取った手紙(e)を、自身の郵便受けリストの末尾に追加します。
        // 追加し終わったら関数を抜け、自動的に鍵(Mutex)が解除されます。
        m_frameEvents.push_back(e);
        });
}

void AIHearingSystem::Update(float dt) {
    // 今フレームで処理する手紙を入れる「作業用カバン」を用意します。
    std::vector<AISoundEvent> currentEvents;
    {
        // 郵便受けを開けるために鍵をかけます。
        std::lock_guard<std::mutex> lock(m_eventMutex);

        // 【解説】 std::move は「中身の所有権の移動（ポインタのすげ替え）」です。
        // m_frameEvents の中身を、たった数ナノ秒で currentEvents にごっそり移し替えます。
        currentEvents = std::move(m_frameEvents);

        // 空になった郵便受けをリセットします。
        m_frameEvents.clear();

    } // ← ここでカッコを抜けるため、一瞬で鍵(Mutex)が解除されます！

    // もし手紙が1通も来ていなければ、AIの思考処理そのものをスキップ（超最適化）
    if (currentEvents.empty()) return;

    // 2. AIごとに聞こえたかどうかを判定 
    ForEachWithID([&](EntityID aiEntity,
        const TransformComponent& transform,
        const AIPerceptionComponent& perception,
        AIMemoryComponent& memory,
        AIStateComponent& state,
        NavAgentComponent& navAgent) {

            // 視覚ですでにプレイヤーを追跡中(Chase)なら、音の調査は行わない
            if (state.currentState == AIState::Chase) return;

            Vector3 aiPos = transform.position;
            bool heardSound = false;
            Vector3 bestSoundPos = Vector3::Zero;
            float highestPriority = -1.0f;

            // 回収したすべての手紙(音)をチェックする
            for (const auto& ev : currentEvents) {
                float dist = Vector3::Distance(aiPos, ev.position);

                // =========================================================================
                // ★究極のシンプル化: AIと音源の距離が、音の届く範囲(volumeRadius)以下なら聞こえる！
                // =========================================================================
                if (dist <= ev.volumeRadius) {

                    // 音源に近いほど優先して向かう
                    float priority = ev.volumeRadius - dist;
                    if (priority > highestPriority) {
                        highestPriority = priority;
                        bestSoundPos = ev.position;
                        heardSound = true;
                    }
                }
            }

            // ---------------------------------------------------------
            // 🧠 リアクション: 調査(Investigate)への遷移
            // ---------------------------------------------------------
           // もし「聞こえた音」が1つでもあった場合のリアクション
            if (heardSound) {
                // 記憶を更新し、怒りゲージを強制的に引き上げる
                memory.lastKnownPos = bestSoundPos;
                memory.currentAngerLevel = (std::max)(memory.currentAngerLevel, 50.0f);

                // 【解説】 脳のモードを「調査(Investigate)」に切り替え、
                // 神経(NavAgent)に「この音源座標までのルートを計算して歩け」と命令を下す。
                state.currentState = AIState::Investigate;
                state.timeInState = 0.0f;

                navAgent.targetPosition = bestSoundPos;
                navAgent.isPathRequested = true;          

                CCL_LOG_INFO(LogCategory::Core, "AI[%llu] Heard something. Moving to Investigate.", aiEntity);
            }
        });
}

// 規約に基づき、論理システムとして登録
REGISTER_LOGIC_SYSTEM(AIHearingSystem, Priority::LogicStage::L02_AI_Sense)