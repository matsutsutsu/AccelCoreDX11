#pragma once
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

// ===================================================================================
// ファイル名: CCL_JobSystem.h
// クラス名: JobSystem
//
// 【概要】
//   マルチスレッド処理を行うための「スレッドプール」管理クラスです。
//   CPUのコア数分だけ事前にスレッド（作業員）を作成し、
//   投げられたタスク（ジョブ）を並列で消化させます。
//
// 【イメージ：レストランの厨房】
//   - JobSystem     : 厨房そのもの
//   - Worker        : 雇われたシェフたち（スレッド）
//   - JobQueue      : 注文票が貼られるボード（仕事リスト）
//   - Mutex         : ボードの前の「立ち入り禁止テープ」（排他制御の鍵）
//   - ConditionVar  : シェフを起こす「呼び出しベル」
//   - JobCounter    : 料理が全部出たか確認する「伝票の残り枚数」
// ===================================================================================


namespace CCL::ECS::Core {

    // Job = 「引数なし・戻り値なし」の関数（ラムダ式など）
    // 例：「野菜を切る」「皿を洗う」「Chunk0番を更新する」といった具体的な作業内容
    using Job = std::function<void()>;

    // -----------------------------------------------------------------------
    // JobCounter: 仕事の完了待ちをするためのカウンター
    // -----------------------------------------------------------------------
    // 普通の int だと、複数のシェフが同時に書き込むと数字が壊れてしまいますが、
    // std::atomic は「どんなに忙しくても、必ず1ずつ正確に増減すること」を保証する魔法の型です。
    struct JobCounter {
        std::atomic<int> count{0};
    };

    class JobSystem {
      public:
        // コンストラクタ：厨房を開き、シェフを雇う
        JobSystem();

        // デストラクタ：店じまいし、シェフを帰宅させる
        ~JobSystem();

        // コピー禁止（厨房は世界に一つだけ。シングルトンや管理クラスで持つため）
        JobSystem(const JobSystem &)            = delete;
        JobSystem &operator=(const JobSystem &) = delete;

        /**
         * @brief 仕事をキューに追加する（店長が注文を入れる）
         * @param job 実行したい処理（ラムダ式など）
         * @param counter 完了を追跡するためのカウンター（nullptrなら追跡しない）
         */
        void Execute(Job job, JobCounter *counter = nullptr);

        /**
         * @brief 指定したカウンターが0になるまで、メインスレッドを待機させる
         * （料理が全部出るまで、店長は足止めを食らう）
         * @param counter 監視対象のカウンター
         */
        void WaitForCounter(JobCounter *counter);

      private:
        // シェフ（ワーカースレッド）が実行する業務マニュアル
        // 生まれた瞬間から死ぬまで、このループの中で働き続けます。
        void WorkerLoop();

      private:
        // 内部で扱うジョブ構造体
        struct InternalJob {
            Job         task;    // やるべき仕事
            JobCounter *counter; // 終わったら減らすカウンター
        };

        std::vector<std::thread> _workers;  // 働くシェフたち
        std::queue<InternalJob>  _jobQueue; // 仕事の注文票ボード

        std::mutex _queueMutex; // ボードを守る「鍵」（排他制御）
                                // これがないと複数のシェフが同時にチケットを取ろうとして破れます。

        std::condition_variable _condition; // 寝ているシェフを起こす「ベル」

        bool _shutdown = false; // 「閉店」フラグ
    };

} // namespace CCL::ECS::Core