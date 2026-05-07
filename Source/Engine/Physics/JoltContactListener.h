/**
 * @file JoltContactListener.h
 * @brief Jolt物理空間とECS空間を繋ぐ、スレッドセーフな衝突イベントの蓄積バッファ（ポスト）
 *
 * @note Joltのワーカースレッドから非同期に呼ばれるため、内部状態の変更には必ずミューテックスによる保護が必要。
 * @warning このクラス内で直接ECSのWorldやコンポーネントにアクセスしてはならない（データ競合によるクラッシュを招く）。
 */
#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <mutex>
#include <vector>
#include "Engine/GamePlay/Physics/Collision/JoltCollisionEvent.h"

class JoltContactListener : public JPH::ContactListener {
public:
    /**
     * @brief 物理的な接触が新たに検出された瞬間にJoltのワーカースレッドから呼ばれるコールバック
     * @param inBody1 衝突した剛体1
     * @param inBody2 衝突した剛体2
     * @param inManifold 衝突の接触点や法線情報を持つ多様体
     * @param ioSettings 接触に対する反発係数や摩擦を上書きするための設定値
     * * @note 実行コンテキスト: Jolt Worker Thread (マルチスレッド)
     */
    virtual void OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2,
        const JPH::ContactManifold& inManifold,
        JPH::ContactSettings& ioSettings) override;

    /**
     * @brief 蓄積された全ての衝突イベントを一括で取得し、内部バッファをクリアする
     * @return 蓄積されていた JoltCollisionEvent の配列
     * * @note 実行コンテキスト: ECS Main Thread (またはTask Thread)
     * @warning 内部でロックを取得するため、1フレームに1回だけ（バッチ処理として）呼び出すこと。
     */
    std::vector<JoltCollisionEvent> GetAndClearEvents();

private:
    std::mutex                      m_mutex;  ///< スレッド間競合を防ぐための排他ロック
    std::vector<JoltCollisionEvent> m_events; ///< 蓄積される衝突イベントのバッファ
};