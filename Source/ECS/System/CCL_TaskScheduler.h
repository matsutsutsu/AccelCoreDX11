#pragma once
#include "CCL_System.h"
#include <algorithm>
#include <memory>
#include <vector>
#include <string>
#include "ECS/Core/CCL_Chunk.h"
#include "Engine/Platform/Logger.h"

namespace CCL::ECS {
    // システムの依存関係を解析し、並列実行可能なバッチを構築するクラス
    class TaskScheduler {
      public:
         //  外部（ImGui）からバッチ構成を読み取るためのAPI
         const std::vector<std::vector<SystemBase*>>& GetBatches() const {
             return _executionBatches;
         }

        // システムのリストを解析して実行計画（グラフ）を作る
        void BuildGraph(const std::vector<std::unique_ptr<SystemBase>> &systems)
        {
            _executionBatches.clear();
            if (systems.empty()) return;

            // 各システムが「どのバッチに入るか」を記録する
            std::vector<int> systemBatchIndex(systems.size(), 0);

            // 依存関係（トポロジカルソートの簡易版）
            // 全システムを走査し、自分より前のシステムとデータ競合があるか調べる
            for (size_t i = 0; i < systems.size(); ++i) {
                int maxDependencyBatch = -1; // 自分がどこのバッチに入るべきか

                // システム i（今調べているシステム）が、何を読み書きするかを取得
                auto readsI  = systems[i]->GetReadTypes();
                auto writesI = systems[i]->GetWriteTypes();

                // 自分より前にあるシステム j と「競合」しないか全部チェックする！
                for (size_t j = 0; j < i; ++j) {
                    bool conflict = false;

                    // ==========================================================
                    // ★ 究極の修正: ステージ（優先度）の境界線を「絶対の壁」にする
                    // 優先度（Enumの値）が違うシステム同士は、データに関係なく必ず順番を待つ！
                    // ==========================================================
                    if (systems[i]->GetPriority() != systems[j]->GetPriority()) {
                        conflict = true;
                    }
                    else {
                        // --- 優先度（ステージ）が同じ場合のみ、データ競合をチェックして並列化を試みる ---
                        auto readsJ = systems[j]->GetReadTypes();
                        auto writesJ = systems[j]->GetWriteTypes();

                        bool typeConflict = false;
                        for (auto w : writesI) {
                            if (Contains(readsJ, w) || Contains(writesJ, w)) typeConflict = true;
                        }
                        for (auto r : readsI) {
                            if (Contains(writesJ, r)) typeConflict = true;
                        }

                        if (typeConflict) {
                            const auto& chunksI = systems[i]->GetTargetChunks();
                            const auto& chunksJ = systems[j]->GetTargetChunks();

                            for (CCL::ECS::Core::Chunk* cI : chunksI) {
                                for (CCL::ECS::Core::Chunk* cJ : chunksJ) {
                                    if (cI == cJ) {
                                        conflict = true;
                                        break;
                                    }
                                }
                                if (conflict) break;
                            }
                        }
                    }

                    // 競合（またはステージの違い）があった場合、その先輩のバッチ以降に配置する
                    if (conflict) {
                        maxDependencyBatch = (std::max)(maxDependencyBatch, systemBatchIndex[j]);
                    }
                }



                // 依存するシステムの次のバッチに配置する
                systemBatchIndex[i] = maxDependencyBatch + 1;

                // 自分のバッチ番号は、「競合した相手のバッチ番号 + 1 (つまり次のターン)」にする！
                // 誰とも競合しなかったら、-1 + 1 = 0 で、最初のターンに組み込まれる。
                if (systemBatchIndex[i] >= _executionBatches.size()) {
                    _executionBatches.resize(systemBatchIndex[i] + 1);
                }

                // バッチにシステムを登録
                _executionBatches[systemBatchIndex[i]].push_back(systems[i].get());
            }

            // 構築されたバッチ（実行順序）をLoggerに出力する
            CCL_LOG_INFO(LogCategory::ECS, "=== TaskScheduler: Execution Graph Built ===");
            for (size_t i = 0; i < _executionBatches.size(); ++i) {
                std::string batchInfo = "Batch " + std::to_string(i) + ": [";
                for (size_t j = 0; j < _executionBatches[i].size(); ++j) {
                    batchInfo += _executionBatches[i][j]->GetName();
                    if (j < _executionBatches[i].size() - 1) batchInfo += ", ";
                }
                batchInfo += "]";
                CCL_LOG_INFO(LogCategory::ECS, "%s", batchInfo.c_str());
            }
            CCL_LOG_INFO(LogCategory::ECS, "============================================");
        }

        // 構築されたバッチ順に実行する
        void Execute(float dt, Core::JobSystem *jobSystem)
        {
            // バッチ（シフト表のまとまり）ごとに順番に見ていく
            for (auto &batch : _executionBatches) {
                if (batch.empty()) continue;

                 // ① バッチの中にシステムが1個しかない場合
                if (batch.size() == 1) {   
                    // バッチ内に1つしかないなら直列実行
                    batch[0]->UpdateImpl(dt);
                }
                // ② バッチの中にシステムが複数（並列可能）ある場合
                else {      
                    // バッチ内に複数システムがある = 完全にデータが独立している = 超並列実行可能！
                    Core::JobCounter counter;

                    // システムの数だけ、JobSystem（厨房）に仕事を投げる
                    for (SystemBase *sys : batch) {
                        // 【バトンの作成】
                        // [sys, dt] をキャプチャ（メモリ上にコピー）し、
                        // 「sys->UpdateImpl(dt) を実行する」という関数ポインタ（ラムダ式）を作る。
                        auto jobTask = [sys, dt]() { sys->UpdateImpl(dt); };

                        // 【バトンの投下】
                        // そのラムダ式と、カウンターのポインタを JobSystem に渡す。
                        jobSystem->Execute(jobTask, &counter);
                    }
                    // このバッチの全システムが終わるまで待つ
                    jobSystem->WaitForCounter(&counter);
                }
            }
        }

      private:
        // あるTypeIDのリストに、特定のTypeIDが含まれているか
        bool Contains(const std::vector<TypeID> &list, TypeID val) const
        {
            return std::find(list.begin(), list.end(), val) != list.end();
        }

        // 外側のvector: 順番に実行されるフェーズ（バッチ）
        // 内側のvector: そのフェーズ内で「同時に並列実行してよい」システムのリスト
        std::vector<std::vector<SystemBase *>> _executionBatches;
    };
} // namespace CCL::ECS