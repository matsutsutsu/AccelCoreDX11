#include "CCL_World.h"
#include "../System/CCL_SystemManager.h"
//#include "Engine/Graphics/Renderer/ShapeRenderer.h"
#include <algorithm>
#include <cassert>
#include <cstring>

// Tracyのインクルード
#include "tracy/Tracy.hpp"


namespace CCL::ECS::Core {
    using namespace CCL::ECS;

    World::World()
    {
        _entityGenerations.resize(1024, 1); // 初期サイズ確保
        _nextUnusedIndex = 1;               // インデックス0は予約
    }

    World::~World()
    {
        _chunkManager.FinalizeAll();
        _pendingOps.Clear();
    }

    void World::Clear()
    {
        // 1. 保留中の操作を破棄
        _pendingOps.Clear();
        _chunkManager.FinalizeAll();

        // イベントの購読（Subscribe）も全て白紙に戻す
        _eventBus.Clear();

        // 3. キャッシュとID管理情報をリセット
        _entityGenerations.clear();
        _freeIndices.clear();

        // インデックスを初期状態(1)に戻す
        _nextUnusedIndex = 1;

        // ※ _resources (シングルトン) は残す設計にするか、消す設計にするかはお好みで。
        //   通常、Gridなどのリソースは再登録が必要になるため、ここで消しても良いですが、
        //   Initialize() で一度しか登録しない設計なら消さない方が無難です。
        //   今回は「エンティティのリセット」が目的なので resources は触りません。
    }



    // コンポーネント追加リクエストの実装部分
    // World.h の AddComponent<T> から呼ばれます
    void World::RequestAddComponent(EntityID entity,
        TypeID                               tid,
        size_t                               size,
        const std::function<void(void *)>   &ctor,
        const Destructor                    &dtor,
        TypeData::ConstructFunc              copyCtor,
        TypeData::AssignFunc                 assigner,
        TypeData::MoveFunc                   mover,
        const char                          *name)
    {
        if (!IsEntityValid(entity)) return;

        PendingOp op;
        op.kind     = PendingOpKind::AddComponent;
        op.entity   = entity;
        op.type     = tid;
        op.typeSize = size;
        op.destructor = dtor;
        // 関数ポインタの保存
        // ChunkManagerの ScrutinyAndApply で TypeData を生成する際、
        // これらのポインタがないと nullptr になり、memcpy へのフォールバックが発生してしまう
        op.constructor = copyCtor;
        op.assigner    = assigner;
        op.mover       = mover;
        op.name = name; // カプセルに名前を入れる

        // ここでコンストラクタを即時実行せず、factoryに保存する
        if (ctor) {
            op.factory = ctor;
            op.data    = nullptr; // dataは使わない
        }
        else {
            // ctorがない場合（従来通り）はメモリ確保してコピー用データを作る
            void *mem = _frameAllocator.Alloc(size);
            std::memset(mem, 0, size); // またはデフォルト構築
            op.data    = mem;
            op.factory = nullptr;
        }

        _pendingOps.Add(op);
    }

    void World::RequestRemoveComponent(EntityID entity, TypeID tid)
    {
        // 世代チェック
        if (!IsEntityValid(entity)) return;

        PendingOp op;
        op.kind     = PendingOpKind::RemoveComponent;
        op.entity   = entity;
        op.type     = tid;
        op.typeSize = 0; // 削除時はサイズ情報は不要
        op.data     = nullptr;

        _pendingOps.Add(op);
    }

    EntityID World::RequestSpawnEntity(const Archetype &archetype)
    {
        uint32_t index;
        uint32_t generation;

        // --- Phase 1: 世代付きIDの発行 ---
        {
            // 再利用リストに空きがあればそれを使う
            if (!_freeIndices.empty()) {
                index = _freeIndices.back();
                _freeIndices.pop_back();
                generation = _entityGenerations[index];
            }
            // 空きがなければ新規発行
            else {
                index = _nextUnusedIndex++;
                if (index >= _entityGenerations.size()) {
                    _entityGenerations.resize(index + 1024, 1);
                }
                generation                = 1;
                _entityGenerations[index] = 1;
            }
        }

        EntityID id = EntityHandle::Combine(index, generation);

        // --- Phase 2: PendingOp への登録 ---
        // アーキタイプ情報はコピーしてヒープに置く（ポインタで渡すため）
        // ★修正: new Archetype(...) をフレームアロケータに
        Archetype *heapArch = _frameAllocator.New<Archetype>(archetype);

        PendingOp op;
        op.kind   = PendingOpKind::Spawn;
        op.entity = id; // 新しい世代付きIDをセット
        op.data   = reinterpret_cast<void *>(heapArch);

        _pendingOps.Add(op);

        // 生成したIDを即座に返す（ユーザーはすぐこのIDを使える）
        return id;
    }

    void World::RecycleEntityID(EntityID id)
    {
        uint32_t index = EntityHandle::GetIndex(id);

        // ここが世代オーバーフロー対策
        // UINT32_MAXまで行ったら1に戻す（0は無効値として避ける）
        if (_entityGenerations[index] == UINT32_MAX) {
            _entityGenerations[index] = 1;
        }
        else {
            _entityGenerations[index]++;
        }

        // インデックスを再利用リストへ
        _freeIndices.push_back(index);
    }

    void World::RequestDestroyEntity(EntityID entity)
    {
        PendingOp op;
        op.kind     = PendingOpKind::Destroy;
        op.entity   = entity;
        op.type     = InvalidTypeID; // destroy marker
        op.typeSize = 0;
        op.data     = nullptr;

        _pendingOps.Add(op);
    }

    // --- ScrutinyAndApply: 遅延実行関数 ---
    //  やってはいけないこと
    //  ────────────────────────
    //  System実行中に...
    //  entity.AddComponent<Velocity>()
    //  ↓
    //  即座にChunk移動が発生すると...
    //  ↓
    //  今まさにループ中のChunk構造が変わり、イテレータが無効化される
    //  ↓
    //  クラッシュ / メモリ破壊
    //
    //  安全な遅延実行モデル
    //  ────────────────────────
    //  [System実行中]
    //    → 変更要求だけをPendingOpsに記録（Request...）
    //
    //  [Systemが全部終わった後]
    //    → ScrutinyAndApply()
    //      → まとめて整合性を保ちながら実行
    //      → 次フレームのSystemは最新状態で動く
    //
    void World::ScrutinyAndApply()
    {
        // 構造変更の適用時間を計測
        ZoneScopedN("World::ScrutinyAndApply");


        // 1. 溜まっていた予約（AddComponent, RemoveComponent, Spawn, Destroy）を
        //    ChunkManager に渡して、物理的なメモリ移動や生成を実行させる
        //    戻り値として、実際に削除されたEntityIDのリストを受け取る

        // 1. 保留されていた構造変更を適用
        //    この内部で SetEntityMoveCallback で登録したラムダが呼ばれ、
        //    _entityCache が自動的に更新されます！
        std::vector<EntityID> deadList = _chunkManager.ApplyPendingOperations(_pendingOps);



        // 3. IDをリサイクル（世代を上げてフリーリストへ）
        for (EntityID id : deadList) {
            RecycleEntityID(id);
        }


        // 3. アロケータのリセット
        // これで一時メモリは一瞬で解放されます。delete op.data は不要です。
        // (デストラクタが必要な場合は PendingOps::Clear 内で呼ぶ必要がありますが、
        //  生データコピーなら不要、管理が必要なら工夫がいりますが、一旦メモリ解放優先で)
        _pendingOps.Clear();
        _frameAllocator.Reset(); // ★ 一瞬でリセット

        // 構造変更があったことを記録
        _isStructureDirty = true;
    }

    // GetEntityArchetype の実装
    const Archetype &World::GetEntityArchetype(EntityID entity) const
    {
        size_t entityIdx = 0;
        // ChunkManagerのO(1) vectorアクセスを利用
        size_t chunkIdx = _chunkManager.SearchEntityIn(entity, &entityIdx);

        if (chunkIdx == InvalidIndex) {
            // 存在しない、または削除済みの場合は空のアーキタイプを返す
            static const Archetype emptyArchetype;
            return emptyArchetype;
        }

        return _chunkManager.GetChunks()[chunkIdx]->GetArchetype();
    }

    bool World::HasComponent(EntityID entity, TypeID tid)
    {
        return GetComponentOfEntity(entity, tid) != nullptr;
    }

    // Low-level access
    void *World::GetComponentOfEntity(EntityID entity, TypeID tid)
    {
        size_t entityIdx = 0;
        size_t chunkIdx  = _chunkManager.SearchEntityIn(entity, &entityIdx);
        if (chunkIdx == InvalidIndex) return nullptr;
        return _chunkManager.GetChunks()[chunkIdx]->GetComponentPtrByType(tid, entityIdx);
    }

} // namespace CCL::ECS::Core