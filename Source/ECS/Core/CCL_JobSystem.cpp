#include "CCL_JobSystem.h"


#include "tracy/Tracy.hpp"



namespace CCL::ECS::Core {

    // =======================================================================
    // 開店準備（コンストラクタ）
    // =======================================================================
    JobSystem::JobSystem()
    {
        // 1. パソコンの脳みそ（物理コア数）を調べる
        unsigned int coreCount = std::thread::hardware_concurrency();
        if (coreCount == 0) coreCount = 4; // 分からなければとりあえず4人雇う

        // 2. コアの数だけシェフ（スレッド）を作成
        // _workers.emplace_back した瞬間に、スレッドが生成され
        // WorkerLoop() という関数が別世界（別スレッド）で動き出します。
        for (unsigned int i = 0; i < coreCount; ++i) {
            _workers.emplace_back([this] { WorkerLoop(); });
        }
    }

    // =======================================================================
    // 閉店作業（デストラクタ）
    // =======================================================================
    JobSystem::~JobSystem()
    {
        // 1. 「もう閉店ですよ」という看板（フラグ）を立てる
        // ★重要: ここで Mutex ロックをする理由
        // シェフが「フラグ確認」→「寝る準備」→「実際に寝る」までのわずかな隙間に
        // この書き換えが発生すると、シェフが「閉店に気づかずに永遠に寝てしまう」事故が起きるため。
        // ロックすることで「シェフが寝る準備を完了するまで」待ってから書き換える。
        {
            std::unique_lock<std::mutex> lock(_queueMutex);
            _shutdown = true;
        }

        // 2. 非常ベルを鳴らして全員叩き起こす
        // 寝ているシェフ全員が `wait` から目覚め、`_shutdown` フラグを見て帰宅準備に入る。
        _condition.notify_all();

        // 3. 全員が更衣室から出てくる（スレッドが終了する）のを待つ
        for (std::thread &worker : _workers) {
            if (worker.joinable()) {
                worker.join(); // シェフが完全に帰るまでここで待機
            }
        }
    }

    // =======================================================================
    // 仕事の依頼（Execute）
    // =======================================================================
    void JobSystem::Execute(Job job, JobCounter *counter)
    {
        // 1. カウンターを「先に」増やしておく（★超重要）
        // なぜ？：もし「キューに入れてから」増やすと、シェフが爆速すぎて
        // 「店長が増やす前に、シェフが終わらせて減らしてしまう」可能性があるから。
        // 0 -> -1 になってバグるのを防ぐため、仕事を入れる前に「予約」として増やしておく。
        if (counter) {
            counter->count.fetch_add(1);
        }

        {
            // 2. 鍵をかけてボード（キュー）を独占する
            std::unique_lock<std::mutex> lock(_queueMutex);

            // 仕事のボードにjob（システム処理function）を貼り付ける
            _jobQueue.push({std::move(job), counter});
        }
        // ここでロック解除（スコープ抜け）

        // 3. 休憩室にベルを鳴らす。「1人起きてー！」
        // 寝ているシェフの中から1人だけを起こす。
        _condition.notify_one();
    }

    // =======================================================================
    // 完了待ち（WaitForCounter）
    // =======================================================================
    void JobSystem::WaitForCounter(JobCounter *counter)
    {
        if (!counter) return;

        // カウンターが0になるまでループ
        while (counter->count.load() > 0) {

            // --- 待ってるだけじゃなくて手伝う！ ---
            InternalJob job;
            bool        foundJob = false;

            {
                // ロックして仕事があるか確認
                std::unique_lock<std::mutex> lock(_queueMutex);
                if (!_jobQueue.empty()) {
                    job = std::move(_jobQueue.front());
                    _jobQueue.pop();
                    foundJob = true;
                }
            }

            if (foundJob) {
                // 仕事があったら実行する（店長自ら調理場へ！）
                if (job.task) {
                    job.task();
                }
                // 終わったらカウンターを減らす
                if (job.counter) {
                    job.counter->count.fetch_sub(1);
                }
            }
            else {

                // 仕事もないなら、仕方ないのでYieldして他のスレッドにCPUを譲る
                // (まだカウンターが0じゃない＝誰かが実行中)
                std::this_thread::yield();
            }
        }
    }

    // =======================================================================
    // シェフの業務マニュアル（WorkerLoop）
    // =======================================================================
    void JobSystem::WorkerLoop()
    {
        // スレッドに名前を付ける
        tracy::SetThreadName("Worker Thread");



        while (true) {              // 基本的に死ぬまで働く無限ループ
            InternalJob currentJob; // 自分の手元に持ってくる仕事入れ（お盆）

            // ---【エリアA：注文ボードの前（立ち入り禁止区域）】---
            {
                // 1. 鍵（Mutex）をかける。
                // これで、他のシェフや店長はこの { } の中に入れない。
                // 注文ボード(_jobQueue)を触れるのは、世界で自分一人だけ！
                std::unique_lock<std::mutex> lock(_queueMutex);

                // 2. 「仕事が来る」または「閉店(_shutdown)」まで寝て待つ
                // wait関数は賢いので、寝ている間は「鍵を一時的に返却」してくれる。
                // ベル(_condition)が鳴ったら飛び起きて、再び「鍵を奪い取って」から処理を再開する。
                _condition.wait(lock, [this] { return _shutdown || !_jobQueue.empty(); });

                // 起きた時に「閉店」かつ「仕事も空っぽ」なら、帰宅(return)する
                if (_shutdown && _jobQueue.empty()) {
                    return; // スレッド終了
                }

                // 3. ボードから一番前の注文票を1枚剥がして、自分の手元(currentJob)に移す
                currentJob = std::move(_jobQueue.front()); // コピー・移動
                _jobQueue.pop();                           // ボードから削除（破り捨てる）
            }
            // ---【エリアA終了：ロック解除】---
            // ここで `lock` 変数が消滅するので、自動的に鍵が開く。
            // これで他のシェフもボードに近づけるようになる。

            // ---【エリアB：調理場（並列作業エリア）】---
            // ここはロックしていないので、全シェフが同時に作業できる！

            // 4. 仕事を実行！
            if (currentJob.task) {

                // 【バトンの開封と実行】
                // 先ほどカプセル化された [sys, dt]() { sys->UpdateImpl(dt); } がここで発動！
                currentJob.task(); // ここで重い処理（物理演算など）が走る
            }

            // 5. 完了報告
            // 「一丁あがり！」カウンターを1減らす。
            // 0になったら、待っている店長（WaitForCounter）が解放される。
            if (currentJob.counter) {
                currentJob.counter->count.fetch_sub(1);
            }
        }
    }

} // namespace CCL::ECS::Core