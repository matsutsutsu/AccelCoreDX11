#pragma once
#include "../Common/CCL_Common.h"
#include "CCL_ChunkManager.h"
#include "CCL_PendingOps.h"
#include "CCL_FrameAllocator.h"
#include "CCL_EventBus.h"
#include <any>          //これを勉強
#include <atomic>
#include <cassert>      //これも勉強
#include <mutex>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <functional> // std::functionのために明示的にインクルード
#include <memory>

#include "Engine/GamePlay/Core/DestroyTag.h"


namespace CCL::ECS::Core {
    using namespace CCL::ECS;

    // 前方宣言 (ComponentHandleで使うため)
    class World;

    // ========================================================================
    // EntityRef (エンティティ操作用の超軽量ラッパー)
    // ========================================================================
    struct EntityRef {
      World *world;
      EntityID id;


      // ここでは宣言だけにして、中身はファイルの末尾（World定義の後）に書きます
      template <typename T> EntityRef &Set(T &&component);

      // すでに持っているコンポーネントの中身をラムダ式で書き換える
      template <typename T, typename Func> EntityRef &Patch(Func &&func);

      operator EntityID() const { return id; }
      EntityID ID() const { return id; }
    };

    // ========================================================================
    // リソース管理用の高速IDジェネレータと型安全ラッパー
    // ========================================================================
    class ResourceIDGenerator {
        static inline uint32_t counter = 0;

      public:
		// 型Tごとに、たった一つだけIDが割り当てられる魔法の関数
        template <typename T> static uint32_t GetID()
        {
            static uint32_t id = counter++;
            return id;
        }
    };

	// リソースの型消去用の基底クラス
	// ResourceBaseを継承したResourceWrapperが、任意の型Tのデータを保持できるようにして
	// それらをResourceBase*として配列の中に入れて扱うことで、型安全かつ柔軟なリソース管理を実現します。
    struct ResourceBase {
        virtual ~ResourceBase() = default;
    };

	// 型安全なリソースラッパー。ResourceBaseを継承し、任意の型Tのデータを保持できます。
    template <typename T> struct ResourceWrapper : public ResourceBase {
        T data;
        template <typename... Args>
        ResourceWrapper(Args &&...args) : data(std::forward<Args>(args)...)
        {
        }
    };



    // ========================================================================
    // ComponentHandle
    // 生ポインタ(T*)の代わりに保持する安全なチケット。
    // アクセスするたびにWorldに問い合わせて最新のアドレスを取得する。
    // ========================================================================
    template <typename T> class ComponentHandle {
      public:
        // 無効なハンドル
        ComponentHandle() : _world(nullptr), _id(InvalidEntityID) {}

        // 有効なハンドル (Worldから生成される)
        ComponentHandle(World *world, EntityID id) : _world(world), _id(id) {}

        // ハンドルが有効かチェック (エンティティが存在し、コンポーネントを持っているか)
        bool IsValid() const;

        // 明示的にポインタを取得
        T *Get() const;

        // 矢印演算子 (ポインタのように使える魔法)
        T *operator->() const
        {
            T *ptr = Get();
            assert(ptr && "Accessing invalid or destroyed component! (Handle expired)");
            return ptr;
        }

        // if (handle) { ... } と書けるようにする
        explicit operator bool() const { return IsValid(); }

        // 比較演算子
        bool operator==(const ComponentHandle<T> &other) const
        {
            return _world == other._world && _id == other._id;
        }

      private:
        World   *_world;
        EntityID _id;
    };


    // エンティティが「どこ」にいるかを示す住所録
    // EntityID をキーにして、この構造体を取得することで、
    // 「どのChunkの」「何番目」にデータがあるかが即座にわかります。
    struct EntityLocation {
        Chunk *chunk = nullptr; // どのチャンクか
        size_t index = 0;       // チャンク内の何番目か
    };

    class World {
      public:
        // Worldは唯一無二の存在であるべき。コピーは厳禁。
        World();
        ~World();
        World(const World &)            = delete;
        World &operator=(const World &) = delete;

        // 世界を初期化（全エンティティ削除）
        void Clear();

        // --------------------------------------------------------
        // Frame API (更新フロー)
        // --------------------------------------------------------

        // 遅延処理の適用（構造変更の確定）
        // PendingOpsに溜まった変更予約を実際にメモリに適用し、Chunkを整理します。
        void ScrutinyAndApply();

        // --------------------------------------------------------
        // Component Operations (Template API)
        // --------------------------------------------------------

        // ★【重要】コンポーネント追加（推奨API）
        // 型Tを指定して呼び出すだけで、コンストラクタとデストラクタを自動登録します。
        // これを使わないと、shared_ptrなどが正しく解放されずメモリリークの原因になります。
        template <class T> void AddComponent(EntityID entity)
        {
            // 1. コンストラクタの準備（ラムダ式）
            //    指定されたメモリ領域(ptr)に対して、配置new(placement new)を行い
            //    T型のオブジェクトを初期化します。
            auto ctor = [](void *ptr) { new (ptr) T(); };

            // 2. デストラクタ情報の作成（これが今回のキモです）
            //    T型のデストラクタを呼び出すための関数ポインタを持つオブジェクトを作ります。
            //    これをPendingOpに渡すことで、キャンセル時や削除時に正しく破棄できます。
            
            // ★ここが修正点
            // Destructorだけでなく、TypeDataの全機能を作る
            TypeData td;
            td.Create<T>(); // これで constructor, assigner, mover, destructor 全部入る

            // 3. 内部関数へリクエストを投げる
            RequestAddComponent(entity,
                TypeInfo<T>::ID(),
                sizeof(T),
                ctor,
                td.destructor,
                td.constructor, // Copy Construct
                td.assigner,    // Copy Assign
                td.mover,       // Move
                td.name
            );
        }

       
        // ========================================================================
        // ★万能版 AddComponent (統合済み)
        // 引数が0個でも、1個(インスタンス)でも、複数(コンストラクタ引数)でも
        // 全てこれを経由して安全に処理します。
        // ========================================================================
        template <class T, typename... Args> 
        void AddComponent(EntityID entity, Args &&...args)
        {
            // 1. ヒープではなく、超高速なフレームアロケータからメモリを借りる
            void *tempMem = _frameAllocator.Alloc(sizeof(T));

            // 2. 借りたメモリ上に placement new で直接構築する
            new (tempMem) T(std::forward<Args>(args)...);

            TypeData td;
            td.Create<T>();

            // 3. PendingOp には「一時メモリのポインタ」をそのまま渡す
            PendingOp op;
            op.kind        = PendingOpKind::AddComponent;
            op.entity      = entity;
            op.type        = TypeInfo<T>::ID();
            op.typeSize    = sizeof(T);
            op.data        = tempMem; // 生ポインタを渡す！
            op.destructor  = td.destructor;
            op.constructor = td.constructor;
            op.assigner    = td.assigner;
            op.mover       = td.mover;
            op.name = td.name;    // 名前をカプセルに詰める
            op.factory     = nullptr; // 遅い std::function は二度と使わない

            _pendingOps.Add(op);
        }

        // コンポーネント削除（テンプレート版）
        template <class T> void RequestRemoveComponent(EntityID entity)
        {
            RequestRemoveComponent(entity, TypeInfo<T>::ID());
        }

        // --------------------------------------------------------
        // Entity Operations
        // --------------------------------------------------------

        // Entityを生成する予約をします。IDは即座に発行されますが、
        // 実際のChunkへの追加は ScrutinyAndApply で行われます。
        EntityID RequestSpawnEntity(const Archetype &archetype);


        // ========================================================================
        // ★ アーキタイプから生成し、軽量ラッパーを返す純粋なSpawn関数
        // ========================================================================
        EntityRef Spawn(const Archetype &archetype);

      
        // ゲーム側で使う親子関係対応している破棄処理
        // DestroyTagをつけるだけで破棄処理自体はHierarchyCleanUpSystemで行う
        // もし親が死んでも子供を残したい場合はDestroyの直前にTransformUpdateSystemの
        // DetachChildを呼ぶ
        void Destroy(EntityID id)
        {
            if (!IsEntityValid(id)) return;

            // 既にタグが付いていなければ付ける
            if (!HasComponent<Tag::DestroyTag>(id)) {
                AddComponent<Tag::DestroyTag>(id);
            }
        }

        // システム側で使う強力なDestroy処理　親子関係無視
        void RequestDestroyEntity(EntityID id);

        // 型IDを直接指定してコンポーネント削除をリクエストする（動的削除用）
        void RequestRemoveComponent(EntityID entity, TypeID tid);

        // --------------------------------------------------------
        // Accessors & Systems
        // --------------------------------------------------------
        ChunkManager &GetChunkManager() { return _chunkManager; }
        PendingOps   &GetPendingOps() { return _pendingOps; }

        // --------------------------------------------------------
        // 世代管理・妥当性チェック
        // --------------------------------------------------------

        // 指定されたEntityIDが有効か（削除済みでないか、世代が古いIDでないか）をチェック
        // インライン展開されるため高速です。
        inline bool IsEntityValid(EntityID entity) const;

        inline bool ValidateEntityGeneration(EntityID entity) const
        {
            return IsEntityValid(entity);
        }

        // --------------------------------------------------------
        // Resource Management  (Singleton Component)　(O(1) Access)
        // --------------------------------------------------------
        // システム間で共有する Grid や DeltaTime などの管理に使用
        // 特定のEntityに属さない、世界に1つだけのデータを持たせる機能です。

        template <typename T, typename... Args> void AddResource(Args &&...args)
        {
            size_t id = ResourceIDGenerator::GetID<T>();
            if (id >= _resourcesArray.size()) {
                _resourcesArray.resize(id + 1);
            }
            _resourcesArray[id] = std::make_unique<ResourceWrapper<T>>(std::forward<Args>(args)...);
        }

        template <typename T> T &GetResource()
        {
			// ResourceをIDにしたものを取得して配列のインデックスにすることで
			// O(1)でリソースにアクセスできるようにします。
            size_t id = ResourceIDGenerator::GetID<T>();
            assert(id < _resourcesArray.size() && _resourcesArray[id] != nullptr &&
                   "Requested resource not found!");
            return static_cast<ResourceWrapper<T> *>(_resourcesArray[id].get())->data;
        }

        template <typename T> bool HasResource() const
        {
            size_t id = ResourceIDGenerator::GetID<T>();
            return id < _resourcesArray.size() && _resourcesArray[id] != nullptr;
        }

        // --------------------------------------------------------
        // Component Access
        // --------------------------------------------------------

        // 型安全かつ高速にコンポーネントを取得する関数
        // 1. キャッシュチェック → 2. なければ検索 → 3. ポインタキャストして返す
        template <typename T> inline T *GetComponent(EntityID entity);

        // 指定したコンポーネントを持っているか判定する関数
        template <typename T> bool HasComponent(EntityID entity)
        {
            // GetComponentは持っていなければnullptrを返す仕様なのでこれを利用
            return GetComponent<T>(entity) != nullptr;
        }

        // 指定したコンポーネントを持っているか判定する関数 (TypeID版)
        // エディタやシリアライザなど、動的に型を扱う場合に便利です。
        bool HasComponent(EntityID entity, TypeID tid);

        // ========================================================================
        // ハンドル取得関数
        // 生ポインタではなく、このハンドルを受け取って保持
        // ========================================================================
        template <typename T> ComponentHandle<T> GetHandle(EntityID entity)
        {
            // エンティティが存在し、コンポーネントを持っている場合のみ有効なハンドルを返す
            if (HasComponent<T>(entity)) {
                return ComponentHandle<T>(this, entity);
            }
            return ComponentHandle<T>(); // 無効なハンドル
        }

        // 低レベルアクセス（型IDで取得）
        void *GetComponentOfEntity(EntityID entity, TypeID tid);

       

        // --------------------------------------------------------
        // Editor / Inspection API
        // --------------------------------------------------------

        // エンティティが現在持っているコンポーネント構成（Archetype）を取得します。
        // これにより、外部（エディタ等）がChunkの内部構造を知らなくても
        // 「このエンティティは何のコンポーネントを持っているか」を知ることができます。
        const Archetype &GetEntityArchetype(EntityID entity) const;

        // --------------------------------------------------------
        // View / Query
        // --------------------------------------------------------

        // 指定したコンポーネントを持つエンティティ一覧を取得する
        // 例: auto enemies = world->View<Position, Enemy>();
        template <class... Components> inline std::vector<EntityID> View();

       

        // 内部実装用: 非テンプレート版 AddComponent
        // ここで実際のPendingOpを作成します       
        void RequestAddComponent(EntityID      entity,
            TypeID                             tid,
            size_t                             size,
            const std::function<void(void *)> &ctor,
            const Destructor                  &dtor,
            TypeData::ConstructFunc            copyCtor,
            TypeData::AssignFunc               assigner,
            TypeData::MoveFunc                 mover, 
            const char *name);

        // --------------------------------------------------------
        // Component Patching (Prefab生成後の部分修正用)
        // --------------------------------------------------------

        // 既存のコンポーネントの一部だけを書き換える予約を行います。
        // Prefab生成直後など、GetComponentがまだできないタイミングで有効です。
        // 使用例:
        // world.PatchComponent<TransformComponent>(entity, [pos](auto& trans){
        //     trans.position = pos; // positionだけ変更。scale等はPrefabのまま維持
        // });
        template <class T> void PatchComponent(EntityID entity, std::function<void(T &)> action)
        {
            if (!IsEntityValid(entity)) return;

            PendingOp op;
            op.kind   = PendingOpKind::PatchComponent;
            op.entity = entity;
            op.type   = TypeInfo<T>::ID();

            // 型安全なラムダ式を、汎用的な void* 型のラムダ式にラップして保存
            op.patcher = [action](void *ptr) { action(*reinterpret_cast<T *>(ptr)); };

            _pendingOps.Add(op);
        }


        // イベントバスへのアクセス窓口
        EventBus &GetEventBus() { return _eventBus; }

        // ★採用: ECSContext最適化のためのダーティフラグ
        bool IsStructureDirty() const { return _isStructureDirty; }
        void ResetStructureDirty() { _isStructureDirty = false; }

      private:

        // デバッグ用取得（キャッシュミス時などのフォールバック）
        template <class T> inline T *GetComponentDebug(EntityID entity);

        // 実際にエンティティが削除されたタイミングで呼ばれる関数
        // IDをフリーリストに戻し、世代を進めます
        void RecycleEntityID(EntityID id);

        // フレームアロケータの実体
        FrameAllocator _frameAllocator;



      private:
        ChunkManager             _chunkManager;
        PendingOps               _pendingOps;

        // World専属の郵便局
        EventBus _eventBus;
      

        // --- 世代管理用のデータ構造 ---
        std::vector<uint32_t> _entityGenerations;   // インデックスごとの現在の世代
        std::vector<uint32_t> _freeIndices;         // 再利用可能なインデックス
        uint32_t              _nextUnusedIndex = 1; // まだ一度も使っていないインデックス


        bool _isStructureDirty = true; // ★追加: 構造変更があったかフラグ


        std::vector<std::unique_ptr<ResourceBase>> _resourcesArray;

    };

    // ===========================================================
    // Inline Implementations (ヘッダー実装)
    // ===========================================================

    // ComponentHandleの実装
    // Worldクラスの定義が終わった後でないと GetComponent を呼べないためここに記述
    template <typename T> inline T *ComponentHandle<T>::Get() const
    {
        if (!_world) return nullptr;
        // World::GetComponent 内で IsEntityValid チェックが行われるので安全
        return _world->GetComponent<T>(_id);
    }

    template <typename T> inline bool ComponentHandle<T>::IsValid() const
    {
        return Get() != nullptr;
    }


    inline bool World::IsEntityValid(EntityID entity) const
    {
        uint32_t index = EntityHandle::GetIndex(entity);
        uint32_t gen   = EntityHandle::GetGeneration(entity);

        // インデックスが範囲外の場合
        if (index >= _entityGenerations.size()) return false;

        // 世代が一致しない場合（古いIDまたは未来のID）
        // 例: プレイヤーが持っているIDの世代が「1」なのに、
        //     World側の世代が「2」になっていたら、そのEntityは既に削除・再利用されている
        if (_entityGenerations[index] != gen) return false;

        return true;
    }

    template <class T> inline T *World::GetComponentDebug(EntityID entity)
    {
        // 世代チェック
        if (!IsEntityValid(entity)) return nullptr;

        size_t entityIdxInChunk = 0;
        size_t chunkIdx         = _chunkManager.SearchEntityIn(entity, &entityIdxInChunk);

        if (chunkIdx == InvalidIndex) return nullptr;

        auto &chunk = _chunkManager.GetChunks()[chunkIdx];

        // 削除マークのチェックも追加
        if (chunk->IsEntityDestroyed(entityIdxInChunk)) return nullptr;

        return static_cast<T *>(chunk->GetComponentPtrByType(TypeInfo<T>::ID(), entityIdxInChunk));
    }

    template <class T> inline T *World::GetComponent(EntityID entity)
    {
        // 世代チェック（不正なIDなら即終了）
        if (!IsEntityValid(entity)) return nullptr;

        return static_cast<T*>(GetComponentOfEntity(entity, TypeInfo<T>::ID()));

    }

    template <class... Components> inline std::vector<EntityID> World::View()
    {
        std::vector<EntityID> entities;

        // 1. 欲しいコンポーネント構成（フィルタ）を作る
        Archetype filter = ArchetypeHelper::Generate<Components...>();

        // 2. ChunkManagerから全チャンクを取得
        auto &chunks = _chunkManager.GetChunks();

        // 3. 全チャンクを走査
        for (const auto &upChunk : chunks) {
            if (!upChunk) continue;
            Chunk *chunk = upChunk.get();

            // 4. そのチャンクがフィルタ条件を満たしているか確認
            // LhasR (Left has Right) : Chunkのアーキタイプ >= フィルタ
            if (ArchetypeHelper::LhasR(chunk->GetArchetype(), filter)) {
                // 5. 条件に合うチャンクなら、中にいる全エンティティをリストに追加
                size_t          count = chunk->GetEntityCount();
                const EntityID *ids   = chunk->GetEntityIDs(); // Chunk内のID配列の先頭

                // vectorに一括コピー（高速）
                entities.insert(entities.end(), ids, ids + count);
            }
        }
        return entities;
    }

    // ★ EntityRef::Set の実装
    template <typename T> inline EntityRef &EntityRef::Set(T &&component) {
      // Worldの完全な定義がわかっているので AddComponent が呼べる
      world->AddComponent<std::decay_t<T>>(id, std::forward<T>(component));
      return *this;
    }

    template <typename T, typename Func>
    inline EntityRef &EntityRef::Patch(Func &&func) 
    {
      world->PatchComponent<T>(id, std::forward<Func>(func));
      return *this;
    }

    // ★ World::Spawn の実装
    inline EntityRef World::Spawn(const Archetype &archetype) {
      EntityID id = RequestSpawnEntity(archetype);
      return EntityRef{this, id};
    }
   

} // namespace CCL::ECS::Core