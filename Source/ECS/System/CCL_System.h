#pragma once
// CCL_System.h
// SystemBase / System 用インターフェース

#include <vector>
#include <memory>
#include <type_traits>
#include "ECS/Common/CCL_Common.h"
#include "ECS/Core/CCL_World.h"
#include "ECS/Core/CCL_JobSystem.h"

#include "CCL_SystemAccess.h"

// スレッドに名前を付ける
#include "tracy/Tracy.hpp"


#include <tuple> // 追加

// forward-declare Core::Chunk so System layer can reference it without including core headers
namespace CCL::ECS::Core { class Chunk; }


namespace CCL::ECS
{

    //--------------------------------------------
    // SystemBase: 全てのSystemの基底
    //--------------------------------------------
    class SystemBase
    {
    public:
        SystemBase(const std::string& name) : _name(name) // コンストラクタに名前を追加
        { 
            _systemID = ID_Emitter::EmitSystemID();
        }
        virtual ~SystemBase() = default;

        // ID_Emitterから発行されたSystemを一意に識別するIDを返す
        SystemID GetSystemID() const { return _systemID; }

        // アーキタイプフィルタ（必要なコンポーネント構成）
        const Archetype& GetFilter() const { return _filter; }

        // 更新対象ChunkリストをWorld側が設定
        void AddTargetChunk(CCL::ECS::Core::Chunk* chunk)
        {
            _targetChunks.push_back(chunk);
        }

        void ClearTargetChunks()
        {
            _targetChunks.clear();
        }

        // システム登録時に一度だけ呼ばれる初期化関数
        // (EventBusの購読や、事前計算などに使う)
        virtual void Initialize() {}

        // 派生側が実装する更新処理
        virtual void Update(float dt) = 0;

        virtual void UpdateImpl(float dt)
        {

            ZoneScoped;
            ZoneName(_name.c_str(), _name.size());


            auto start = std::chrono::high_resolution_clock::now();

            Update(dt);

            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<float, std::milli> elapsed = end - start;
            _lastUpdateTime                                  = elapsed.count();
            _displayUpdateTime = _displayUpdateTime * 0.9f + _lastUpdateTime * 0.1f;
        }


        // デバッグ描画の有効/無効を切り替えるフラグ
        bool isDebugVisible = false;

        bool IsDebugVisible() const { return isDebugVisible; }

		// ImGuiによるデバッグGUI描画 (必要なら派生側でオーバーライド)
        virtual void OnGui() {}

        const std::vector<CCL::ECS::Core::Chunk*>& GetTargetChunks() const { return _targetChunks; }

        // Worldをセットするメソッドを追加
        void SetWorld(Core::World* world) { _world = world; }

        // --- 優先度の取得 ---
        // 優先度をセットする（Managerから呼ばれる）
        void SetPriority(int priority) { _priority = priority; }

        // 保存された優先度を返す（オーバーライド不要！）
        virtual int GetPriority() const { return _priority; }

		// システム名取得
        const std::string& GetName() const { return _name; }
        float GetLastUpdateTime() const { return _lastUpdateTime; }
        void SetLastUpdateTime(float ms) { _lastUpdateTime = ms; }

		float GetDisplayUpdateTime() const { return _displayUpdateTime; }
		void SetDisplayUpdateTime(float ms) { _displayUpdateTime = ms; }

        // ジョブシステムを受け取る関数
        void SetJobSystem(Core::JobSystem *js) { _jobSystem = js; }

        // ChunkManagerのmutexを設定（SystemManagerから呼ばれる）
        void SetStructureMutex(std::shared_mutex *mutex) { _structureMutex = mutex; }

        // マルチスレッドを有効にするかどうかのフラグ
        // デフォルトは true (有効)
        bool enableMultiThread = true;


        // このシステムがRead / Writeするコンポーネントの型IDリストを返す
        virtual std::vector<TypeID> GetReadTypes() const { return {}; }
        virtual std::vector<TypeID> GetWriteTypes() const { return {}; }
    protected:
        // Update時にこのSystemが実際に処理を行うチャンクのリスト
        std::vector<CCL::ECS::Core::Chunk*> _targetChunks;
        Archetype _filter;
        SystemID _systemID = InvalidSystemID;
        Core::World* _world = nullptr; // これで各Systemからアクセス可能になる

        // デフォルト優先度（適当な中間値）
        int _priority = 1000;

        // ジョブシステムへのポインタ (使いたいときにここを見る)
        Core::JobSystem *_jobSystem = nullptr;

        // ChunkManagerの構造変更用mutex（読み取りロック用）
        std::shared_mutex *_structureMutex = nullptr;
    


        // システムの経過時間計測用変数
        std::string _name;       // システムの名前
        float _lastUpdateTime = 0.0f; // ミリ秒単位
        float _displayUpdateTime = 0.0f; // 表示用に滑らかにした値
    };

    //--------------------------------------------
    // IfSystem<T...>: Filterを自動生成するSystem
    //--------------------------------------------
    template <class Derived, class... AccessTypes>
    class IfSystem : public SystemBase
    {
    public:
        // コンストラクタでテンプレート引数<Components...>で指定された
        // コンポーネント型群をもとに、ArchetypeHelperのGenerateを呼び出すことで
        // SystemBase::_filterを初期化する
        // 例: class MovementSystem : public IfSystem<MovementSystem, Position, Velocity>
        // と定義するだけで、_filter に {Position, Velocity} が自動的に設定されます
        IfSystem(const std::string& name) : SystemBase(name)
        {
            // Systemが必要なコンポーネントを宣言
            _filter = ArchetypeHelper::Generate<typename AccessTypes::RawType...>();
        }

        // システム破棄時に、箱の中の解除コマンドを全部自動で実行する！
        virtual ~IfSystem() override {
            for (auto& cleanup : _eventCleanups) {
                cleanup();
            }
            _eventCleanups.clear();
        }

    private:
        // 自分が登録したイベントの「購読解除コマンド」をしまっておく箱
        // std::function にすることで、「何のイベント型か」という情報を隠蔽（型消去）できる
        std::vector<std::function<void()>> _eventCleanups;

    protected:

        // 派生クラスから呼ぶ「超・簡単イベント登録API」
        template <typename EventType>
        void ListenEvent(std::function<void(const EventType&)> callback) {
            if (!_world) return;

            // 1. 普通に EventBus に登録して、チケットIDを受け取る
            auto ticketID = _world->GetEventBus().Subscribe<EventType>(callback);

            // 2. 「このイベントを解除する」という処理そのものを、ラムダ式でカプセル化して箱にしまう
            // ※ EventType が何であったかを、このラムダ式の中に焼き付けて保存できるのがC++の強力な点です
            _eventCleanups.push_back([this, ticketID]() {
                if (this->_world) {
                    this->_world->GetEventBus().Unsubscribe<EventType>(ticketID);
                }
                });
        }

        /**
         * @brief 仲間が使うための型安全なループヘルパー
         * @param func ラムダ式 (引数に Components&... を受け取る)
         */
        template<typename Func>
        void ForEach(Func&& func) {
            using namespace CCL::ECS::Core;

            for (Chunk* chunk : _targetChunks)
            {
                const size_t count = chunk->GetEntityCount();
                if (count == 0) continue;

               // 1. 各コンポーネントの生ポインタを一括取得
                auto componentArrays = std::make_tuple(
                    static_cast<typename AccessTypes::RawType*>(
                        chunk->GetComponentPtrByType(TypeInfo<typename AccessTypes::RawType>::ID(), 0)
                    )...
                );

                const bool hasDestroyed = chunk->HasDestroyedEntities();

                // 2. チャンク内のエンティティをループ
                for (size_t i = 0; i < count; ++i)
                {
                    // 根本修正: 死んでいる（削除予約済み）エンティティなら処理せずスキップ！
                    // MarkDestroyEntityで InvalidEntityID (-1) が埋め込まれているのを確認する
                    // Chunk自身の正確な削除フラグをチェックする
                    // 短絡評価による劇的最適化
                    if (hasDestroyed && chunk->IsEntityDestroyed(i)) continue;

                    // インデックスシーケンスを使って展開を確実にする
                    ForEachImpl(func, componentArrays, i, std::make_index_sequence<sizeof...(AccessTypes)>{});
                }
            }
        }

        // ※ ForEachParallel や ForEachWithID も同様に
        // componentArrays の型キャスト部分を typename AccessTypes::RawType* に変更してください。

        // --------------------------------------------------------
        // ★最適化版: 並列実行版 ForEachParallel
        // チャンクを「まとめ出し（バッチ）」することでジョブ発行回数を劇的に減らす
        // --------------------------------------------------------
        template <typename Func> void ForEachParallel(Func &&func)
        {
            using namespace CCL::ECS::Core;

            // フラグが false なら、強制的にシングルスレッド版を呼ぶ
            if (!_jobSystem || !this->enableMultiThread) {
                ForEach(func);
                return;
            }


            size_t totalChunks = _targetChunks.size();
            if (totalChunks == 0) return;

            // コア数を取得 (例: 6コア12スレッドなら12)
            unsigned int threadCount = std::thread::hardware_concurrency();
            if (threadCount == 0) threadCount = 4;

            // チャンク数が少なすぎる場合（スレッド数の2倍以下など）は
            // 並列化の準備コストの方が高いので、メインスレッドで直列処理して終わる
            if (totalChunks < threadCount * 2) {
                ForEach(func);
                return;
            }

            Core::JobCounter counter;

            // 1つのジョブで処理するチャンク数を計算（均等割り）
            // 例: 300チャンクを12スレッドで割る -> 1ジョブあたり25チャンク
            size_t chunksPerJob = (totalChunks + threadCount - 1) / threadCount;

            // バッチごとにジョブを発行
            for (size_t i = 0; i < totalChunks; i += chunksPerJob) {
                // このジョブが担当する範囲 [start, end)
                size_t end = (std::min)(i + chunksPerJob, totalChunks);

                // [this, i, end, func] をキャプチャしてジョブ生成
                _jobSystem->Execute(
                    [this, i, end, func]() {

                        // 計測タグ（Tracy）
                        ZoneScopedN("System Batch Task");

                        // --- ここはワーカースレッド ---
                        // 担当範囲のチャンクを全部ループで処理する
                        for (size_t k = i; k < end; ++k) {
                            Chunk       *chunk = _targetChunks[k];
                            const size_t count = chunk->GetEntityCount();
                            if (count == 0) continue;

                            auto componentArrays =
                                std::make_tuple(static_cast<typename AccessTypes::RawType *>(
                                    chunk->GetComponentPtrByType(
                                    TypeInfo<typename AccessTypes::RawType>::ID(), 0))...);

                            const bool hasDestroyed = chunk->HasDestroyedEntities();

                            for (size_t entIdx = 0; entIdx < count; ++entIdx) {
                                // ★短絡評価
                                if (hasDestroyed && chunk->IsEntityDestroyed(entIdx)) continue;

                                this->ForEachImpl(func,
                                    componentArrays,
                                    entIdx,
                                    std::make_index_sequence<sizeof...(AccessTypes)>{});
                            }
                        }
                    },
                    &counter);
            }

            // 待機（メインスレッドも暇なら手伝う）
            {
                ZoneScopedN("MainThread Wait");

                _jobSystem->WaitForCounter(&counter);
            }
        }


        // EntityIDも必要な場合用のオーバーロード
        // 相互作用・状態変化の処理に使う、自分の状態や誰かを探すときにIDをもらってやる
        template<typename Func>
        void ForEachWithID(Func&& func) {
            using namespace CCL::ECS::Core;

            for (Chunk* chunk : _targetChunks)
            {
                const size_t count = chunk->GetEntityCount();
                if (count == 0) continue;

                auto componentArrays = std::make_tuple(
                    static_cast<typename AccessTypes::RawType *>(chunk->GetComponentPtrByType(
                        TypeInfo<typename AccessTypes::RawType>::ID(), 0))...
                );

                const EntityID *entityIDs    = chunk->GetEntityIDs();
                const bool      hasDestroyed = chunk->HasDestroyedEntities();

                for (size_t i = 0; i < count; ++i)
                {
                    // ★短絡評価による劇的最適化
                    if (hasDestroyed && chunk->IsEntityDestroyed(i)) continue;

                    // メソッド呼び出しを避け、生ポインタから直接IDを取得
                    EntityID id = entityIDs[i];

                    // ID付きの展開ヘルパーを呼ぶ
                    ForEachWithIDImpl(func,
                        id,
                        componentArrays,
                        i,
                        std::make_index_sequence<sizeof...(AccessTypes)>{});
                }
            }
        }

        // --------------------------------------------------------
        // ★最適化版: 並列実行版 ForEachWithIDParallel
        // --------------------------------------------------------
        template <typename Func> void ForEachWithIDParallel(Func &&func)
        {
            using namespace CCL::ECS::Core;

            // フラグが false なら、強制的にシングルスレッド版を呼ぶ
            if (!_jobSystem || !this->enableMultiThread) {
                ForEachWithID(func);
                return;
            }


            size_t totalChunks = _targetChunks.size();
            if (totalChunks == 0) return;

            unsigned int threadCount = std::thread::hardware_concurrency();
            if (threadCount == 0) threadCount = 4;

            // 少なければ直列
            if (totalChunks < threadCount * 2) {
                ForEachWithID(func);
                return;
            }

            Core::JobCounter counter;

            // 均等割り計算
            size_t chunksPerJob = (totalChunks + threadCount - 1) / threadCount;

            // [1番外側のループ]: スレッドに仕事を割り当てるためのループ
            for (size_t i = 0; i < totalChunks; i += chunksPerJob) {
                size_t end = (std::min)(i + chunksPerJob, totalChunks);

                _jobSystem->Execute(
                    [this, i, end, func]() {

                        ZoneScopedN("System Batch Task (ID)");

                        // [真ん中のループ]: 割り当てられた複数のチャンク（体育館）を1つずつ処理する
                        for (size_t k = i; k < end; ++k) {
                            Chunk       *chunk = _targetChunks[k];
                            const size_t count = chunk->GetEntityCount();
                            if (count == 0) continue;

                            // ★ 鉄則：ループに入る「前」に、全ての生ポインタと状態を抽出する
                            auto componentArrays =
                                std::make_tuple(static_cast<typename AccessTypes::RawType *>(
                                    chunk->GetComponentPtrByType(
                                    TypeInfo<typename AccessTypes::RawType>::ID(), 0))...);

                            const EntityID* entityIDs = chunk->GetEntityIDs(); // 生のID配列のポインタ
                            const bool hasDestroyed = chunk->HasDestroyedEntities(); // 削除予約があるか？

                            // [1番内側のループ]: 1つの体育館の中の、全座席（エンティティ）を処理する
                            for (size_t entIdx = 0; entIdx < count; ++entIdx) {
                                // ★ 劇的最適化: hasDestroyedがfalse(大半はこれ)なら、右側の重い関数は一切呼ばれない！
                                if (hasDestroyed && chunk->IsEntityDestroyed(entIdx)) continue;

                                // ★ 関数呼び出しを避け、ポインタから直接IDを取得
                                EntityID id = entityIDs[entIdx];

                                this->ForEachWithIDImpl(func,
                                    id,
                                    componentArrays,
                                    entIdx,
                                    std::make_index_sequence<sizeof...(AccessTypes)>{});
                            }
                        }
                    },
                    &counter);
            }

            {
                ZoneScopedN("MainThread Wait");

                _jobSystem->WaitForCounter(&counter);
            }
        }


        std::vector<TypeID> GetReadTypes() const override
        {
            std::vector<TypeID> res;
            // IsWriteがfalseならReadリストに追加
            (...,
                (AccessTypes::IsWrite
                        ? void()
                        : res.push_back(TypeInfo<typename AccessTypes::RawType>::ID())));
            return res;
        }

        std::vector<TypeID> GetWriteTypes() const override
        {
            std::vector<TypeID> res;
            // IsWriteがtrueならWriteリストに追加
            (...,
                (AccessTypes::IsWrite ? res.push_back(TypeInfo<typename AccessTypes::RawType>::ID())
                                      : void()));
            return res;
        }

    private:
        // --- 内部ヘルパー: タプルから安全に引数を展開する ---

        template<typename Func, typename Tuple, size_t... Is>
        void ForEachImpl(Func&& func, Tuple& arrays, size_t i, std::index_sequence<Is...>) {
            // std::get<Is>(arrays)[i] で生データを取り出し、
            // AccessTypes::ParamType にキャストして func に渡す。
            // これにより、Read<T> を指定したものは強制的に const T& になり、書き込み不可となる！
            func(static_cast<
                typename std::tuple_element_t<Is, std::tuple<AccessTypes...>>::ParamType>(
                std::get<Is>(arrays)[i])...);
        }

    
        template<typename Func, typename Tuple, size_t... Is>
        void ForEachWithIDImpl(Func&& func, EntityID id, Tuple& arrays, size_t i, std::index_sequence<Is...>) {
            func(id, static_cast<
                typename std::tuple_element_t<Is, std::tuple<AccessTypes...>>::ParamType>(
                std::get<Is>(arrays)[i])...);
        }
    };
}