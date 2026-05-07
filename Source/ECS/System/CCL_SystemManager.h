#pragma once
// CCL_SystemManager.h
// System登録 & Update管理

#include <vector>
#include <memory>
#include <algorithm>
#include <chrono>
#include "../Common/CCL_Common.h"
#include "CCL_System.h"
#include "ECS/Core/CCL_ChunkManager.h"
#include "ECS/Core/CCL_JobSystem.h"

// Priority定義を知る必要がある
#include "Game/Core/SystemPriority.h"
#include "CCL_TaskScheduler.h"

#include "tracy/Tracy.hpp"


namespace CCL::ECS
{
    // システムの種類を定義
    enum class SystemGroup {
        Logic,  // 固定時間で動く（物理、AI、入力、移動）
        Render  // 毎フレーム動く（描画、UI、カメラ補間）
    };

    class SystemManager
    {
    public:
        SystemManager() = default;
        ~SystemManager() = default;

        // Worldをセットする（EditorScene::Initializeなどで呼ぶ）
        void SetWorld(Core::World* world) { _world = world; }


        // ------------------------------------------------------------------
        // ★改良: ステージEnumを使った登録関数 (推奨)
        // ------------------------------------------------------------------

        // Logic用: SystemGroup::Logic を自動指定
        template <class T, class... Args>
        T *RegisterSystem(Priority::LogicStage stage, Args &&...args)
        {
            // Enumをintにキャストして内部登録へ
            return RegisterSystemInternal<T>(
                SystemGroup::Logic, static_cast<int>(stage), std::forward<Args>(args)...);
        }


        // ------------------------------------------------------------------
        // 旧登録関数 / 内部実装
        // ------------------------------------------------------------------
        template <class T, class... Args>
        T *RegisterSystem(Priority::RenderStage stage, Args &&...args)
        {
            return RegisterSystemInternal<T>(
                SystemGroup::Render, static_cast<int>(stage), std::forward<Args>(args)...);
        }


        // 登録関数：グループを指定できるようにする
        template <class T, class... Args>
        T *RegisterSystemInternal(SystemGroup group, int priority, Args &&...args) 
        {
            // 追加しようとしている型Tが必ずSystemBaseの派生クラスであることを
            // コンパイル時に強制している
            static_assert(std::is_base_of<SystemBase, T>::value, "T must be derived from SystemBase");

            // Systemオブジェクトをヒープ上に構築する
            auto sys = std::make_unique<T>(std::forward<Args>(args)...);

            // 生ポインタを取得（戻り値用）
            T* ptr = sys.get();

            // --- 重要：初期化処理 ---
            // 生成時にWorldを渡す
            if (_world) ptr->SetWorld(_world);

            // 優先度もセット
            ptr->SetPriority(priority);

            // ChunkManagerのmutexを設定
            if (_world) {
                ptr->SetStructureMutex(&_world->GetChunkManager().GetStructureMutex());
            }

            // 生成したシステムにジョブシステムを渡しておく
            // これで各システムは自動的に並列処理機能を使えるようになる
            sys->SetJobSystem(&_jobSystem);

            // =======================================================
            // 全ての準備が整ったので、システムの初期化を自動実行する
            // =======================================================
            ptr->Initialize();


            // グループに応じてリストに振り分け
            if (group == SystemGroup::Logic) {
                _logicSystems.push_back(std::move(sys));
            }
            else {
                _renderSystems.push_back(std::move(sys));
            }

            // システムが増えたので依存関係グラフを再構築する必要がある
            _isGraphDirty = true;

            // --- 重要：ソート処理 ---
            // 登録されたグループのリストだけソートすればOK
            SortSystems(group);

            return ptr;
        }


        // 登録済みの全システムを破棄する
        void SystemClear() {
            _logicSystems.clear();
            _renderSystems.clear();

            // システムが空になったので、グラフもリセットさせる
            _isGraphDirty = true;
        }

        // クエリ解決とデータ割り当て
        // 全てのSystemの_targetChunksリストを、そのSystemのArchetypeフィルタに
        // 合致するデータ（Chunk）で更新することです
        void ResolveTargetChunks(Core::ChunkManager* cm);

        // 物理・ロジック系の更新（FixedUpdateから呼ばれる）
        void UpdateLogic(float dt) {


            // グラフが古ければ再構築（ゲーム起動時やシステム動的追加時に1回だけ走る）
            if (_isGraphDirty) {
                _logicScheduler.BuildGraph(_logicSystems);
                _renderScheduler.BuildGraph(_renderSystems);
                _isGraphDirty = false;
            }

            // スケジューラによる超並列実行！
            _logicScheduler.Execute(dt, &_jobSystem);
        }


        // 描画系の更新（Renderから呼ばれる）
        void UpdateRender(float dt) {     

            // Render側のシステムもスケジューラで実行
            _renderScheduler.Execute(dt, &_jobSystem);
        }

		// ImGuiによるデバッグGUI描画
        void OnGui();

     
        // 外部（Worldクラスなど）からシステムリストを取得できる関数を追加
        const std::vector<std::unique_ptr<SystemBase>>& GetLogicSystems() const { return _logicSystems; }
        const std::vector<std::unique_ptr<SystemBase>>& GetRenderSystems() const { return _renderSystems; }

        // 外部（エディタ）からタスクスケジューラを読み取るための関数
        const TaskScheduler& GetLogicScheduler() const { return _logicScheduler; }
        const TaskScheduler& GetRenderScheduler() const { return _renderScheduler; }

        // ★追加: 指定した型のシステムを取得する関数
        // 使用例: auto* animSys = systemManager->GetSystem<AnimationSystem>();
        template <typename T> T *GetSystem() const
        {
            // 1. Logic Systems から検索
            for (const auto &sys : _logicSystems) {
                // dynamic_cast で型チェックを行う
                if (T *casted = dynamic_cast<T *>(sys.get())) {
                    return casted;
                }
            }

            // 2. Render Systems から検索
            for (const auto &sys : _renderSystems) {
                if (T *casted = dynamic_cast<T *>(sys.get())) {
                    return casted;
                }
            }



            // 見つからなければ nullptr
            return nullptr;
        }


        // 全システムのマルチスレッドを一括で強制OFFにするフラグ
        // (デバッグ時に「とりあえず全部シングルで動かしてみる」用)
        bool globalMultiThreadEnabled = true;

    private:

        // 優先度（昇順）でシステムを並び替える
        void SortSystems(SystemGroup group) {
            auto& targetList = (group == SystemGroup::Logic) ? _logicSystems : _renderSystems;

            std::sort(targetList.begin(), targetList.end(),
                [](const std::unique_ptr<SystemBase>& a, const std::unique_ptr<SystemBase>& b) {
                    return a->GetPriority() < b->GetPriority();
                }
            );
        }

        // ジョブシステム
        Core::JobSystem _jobSystem;

        // 2つのリストに分離
        std::vector<std::unique_ptr<SystemBase>> _logicSystems;
        std::vector<std::unique_ptr<SystemBase>> _renderSystems;


        Core::World* _world = nullptr;


        TaskScheduler _logicScheduler;
        TaskScheduler _renderScheduler;
        bool          _isGraphDirty = true; // システムが追加されたらtrueにする

        // ==========================================
        //  キャッシュ用: 前回分配したチャンクのポインタ配列
        // ==========================================
        std::vector<void*> _cachedChunkPointers;
    };

} // namespace CCL::ECS
