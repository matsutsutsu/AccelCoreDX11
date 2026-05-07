#include "CCL_ChunkManager.h"
#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <cassert>

namespace CCL::ECS::Core
{
    using namespace CCL::ECS;

    inline size_t GetPureIndex(EntityID id)
    {
        return static_cast<size_t>(id & 0xFFFFFFFF); // 世代情報をカット
    }

    void ChunkManager::CreateChunk(const Archetype& archetype)
    {
        for (auto& c : _chunks)
        {
            if (c->GetArchetype() == archetype)
                return;
        }
        _chunks.emplace_back(std::make_unique<Chunk>());

        // 固定の _defaultCapacity ではなく、計算した定員を渡す
        size_t capacity = CalculateChunkCapacity(archetype);
        _chunks.back()->Create(archetype, capacity);
    }

    void ChunkManager::FinalizeAll()
    {
        // ★排他ロックを取得
        std::unique_lock<std::shared_mutex> lock(_structureMutex);
    

        for (auto& c : _chunks)
            c->Finalize();
        _chunks.clear();

        // ★改善: マップもクリア
        _entityLocationMap.clear();
    }

    void ChunkManager::AddEntity(EntityID entity, const Archetype& archetype)
    {
        // ★排他ロックを取得
        std::unique_lock<std::shared_mutex> lock(_structureMutex);
    

        // 1. 「型が同じ」かつ「まだ空きがある」チャンクを探す
        auto it = std::find_if(_chunks.begin(), _chunks.end(),
            [&](const std::unique_ptr<Chunk>& c) {
                return c->GetArchetype() == archetype &&
                    c->GetEntityCount() < c->GetCapacity(); // 空き容量チェックを追加
            });

        // 2. 適切なチャンクがなければ新しく作る
        if (it == _chunks.end())
        {
            _chunks.emplace_back(std::make_unique<Chunk>());

            // 固定の _defaultCapacity ではなく、計算した定員を渡す
            size_t capacity = CalculateChunkCapacity(archetype);
            _chunks.back()->Create(archetype, capacity);

            it = std::prev(_chunks.end());
        }

        // 3. 挿入
        (*it)->InsertEntity(entity);

          // 位置を登録
        size_t chunkIdx  = std::distance(_chunks.begin(), it);
        size_t entityIdx = (*it)->GetEntityCount() - 1; // 最後に追加された位置
        UpdateEntityLocation(entity, chunkIdx, entityIdx);
    }

    void ChunkManager::DestroyEntity(EntityID entity)
    {
        // ★排他ロックを取得
        std::unique_lock<std::shared_mutex> lock(_structureMutex);
    

        size_t entityIndex;
        size_t chunkIndex = SearchEntityIn(entity, &entityIndex);
        if (chunkIndex != InvalidIndex)
        {
            _chunks[chunkIndex]->MarkDestroyEntity(entityIndex);

            RemoveEntityLocation(entity);
        }
    }

    size_t ChunkManager::SearchEntityIn(EntityID entity, size_t *outEntityIndex) const
    {
        size_t index = GetPureIndex(entity);

        // 配列の範囲外なら存在しない
        if (index >= _entityLocationMap.size()) return InvalidIndex;

        // O(1) キャッシュヒット確定のアクセス
        const auto &loc = _entityLocationMap[index];

        // 無効化されている（墓石である）場合は存在しない
        if (loc.chunkIndex == InvalidIndex) return InvalidIndex;

        if (outEntityIndex) *outEntityIndex = loc.entityIndex;
        return loc.chunkIndex;
    }

    // 指定したアーキタイプで、かつ空きがあるチャンクを返す（なければ作る）
    Chunk& ChunkManager::GetOrCreateAvailableChunk(const Archetype& archetype)
    {
        auto it = std::find_if(_chunks.begin(), _chunks.end(),
            [&](const std::unique_ptr<Chunk>& c) {
                return c->GetArchetype() == archetype &&
                    c->GetEntityCount() < c->GetCapacity(); // 空きチェック
            });

        if (it == _chunks.end())
        {
            _chunks.emplace_back(std::make_unique<Chunk>());
            size_t capacity = CalculateChunkCapacity(archetype);
            _chunks.back()->Create(archetype, capacity);
            return *_chunks.back();
        }
        return **it;
    }

    // 新しいエンティティを指定されたチャンクに挿入し、コンポーネントデータを初期化する
    void ChunkManager::InsertNewEntity(
        Chunk& dstChunk,
        EntityID entityID,
        const std::vector<PendingOp*>& addOps)
    {
        // 1. 席を確保し、正しいインデックスを直接受け取る
        size_t newIndex = dstChunk.AddEntity(entityID);

         // 位置を登録
        size_t chunkIdx = GetChunkIndex(&dstChunk);
        UpdateEntityLocation(entityID, chunkIdx, newIndex);

        // 2. チャンクの全構成要素（列）に対して初期化を行う
        for (const TypeData& td : dstChunk.GetTypeDatas())
        {
            void* dstPtr = dstChunk.GetComponentPtrByType(td.id, newIndex);
            if (!dstPtr) continue;

            bool filled = false;

            // 3. PendingOp に該当する初期データがあるか探す
            // 最後にしたAddComponentの値で上書きする仕様なので
            // 逆順ループで最後のものを見つけて適用した後に抜ける
            for (int i = (int)addOps.size() - 1; i >= 0; --i) // 逆順ループ
            {
                auto* op = addOps[i];
                if (op->type == td.id)
                {
                    // ファクトリ関数がある場合は、それを使って直接構築する (Emplace)
                    // これによりコピー禁止のGPUParticleComponentも、Chunk上で直接Move構築される
                    if (op->factory) {
                        op->factory(dstPtr);
                    }
                    // constructor を使用（未初期化メモリに構築）
                    if (op->data && td.constructor)
                    {
                        td.constructor(dstPtr, op->data);
                    }
                    else if (op->data)
                    {
                        std::memcpy(dstPtr, op->data, td.typeSize);
                    }
                    filled = true;
                    break;
                }
            }

            // 4. データが提供されなかった場合、デフォルトコンストラクトまたはゼロクリア
            if (!filled)
            {
                // デフォルトコンストラクタがあればそれを使う！
                if (td.defaultConstructor)
                {
                    // これで MaterialComponent() が呼ばれ、data = make_shared される
                    td.defaultConstructor(dstPtr);
                }
                else
                {
                    // なければ今まで通りゼロクリア（intやfloat用）
                    std::memset(dstPtr, 0, td.typeSize);
                }
            
            }
        }

        // ★追加: 生成されたことを通知
        if (_onEntityMove) {
            _onEntityMove(entityID, &dstChunk, newIndex);
        }
    }

    size_t ChunkManager::SearchEqualChunk(const Archetype& a) const
    {
        for (size_t i = 0; i < _chunks.size(); ++i)
        {
            if (_chunks[i]->GetArchetype() == a)
                return i;
        }
        return InvalidIndex;
    }

    
    void ChunkManager::UpdateEntityLocation(EntityID entity, size_t chunkIdx, size_t entityIdx)
    {
        size_t index = GetPureIndex(entity);

        if (index >= _entityLocationMap.size()) {
            // 純粋なインデックスの数だけ配列を広げる（数万件でも数MBなので一瞬です）
            _entityLocationMap.resize(index + 1);
        }

        _entityLocationMap[index] = EntityLocation(chunkIdx, entityIdx);
    }

    void ChunkManager::RemoveEntityLocation(EntityID entity)
    {
        size_t index = GetPureIndex(entity); // 世代を無視した純粋な番号

       if (index < _entityLocationMap.size()) {
            // erase は絶対に使わず、無効な位置情報で上書きする
            _entityLocationMap[index] = EntityLocation(InvalidIndex, InvalidIndex);
        }
    }

    size_t ChunkManager::GetChunkIndex(const Chunk *chunk) const
    {
        for (size_t i = 0; i < _chunks.size(); ++i) {
            if (_chunks[i].get() == chunk) {
                return i;
            }
        }
        return InvalidIndex;
    }

    // ---------------------------------------------------------
    // 容量の動的計算ロジック
    // ---------------------------------------------------------
    size_t ChunkManager::CalculateChunkCapacity(const Archetype &archetype) const
    {
        size_t stride = 0;

        // このアーキタイプに属するエンティティ1体あたりの合計バイト数を計算
        for (const auto &t : archetype) {
            stride += t.typeSize;
        }

        // 例外処理1: サイズが0の場合（タグコンポーネントしか持たない特殊なエンティティ）
        // ゼロ除算を防ぐため、適当な大きな数を返す
        if (stride == 0) return 1024;

        // 例外処理2: 1体のサイズが巨大すぎて16KBに収まらない場合
        // 最低でも1体は入るように保証する
        size_t capacity = CHUNK_SIZE_BYTES / stride;
        return capacity > 0 ? capacity : 1;
    }

    // Archetype遷移の中核ロジック
    // ソースチャンクからEntityのデータをコピー→デストラクタを呼び、
    // 新しいチャンクにコピー構築して移動を完了する
    bool ChunkManager::MoveEntityBetweenChunks(
        Chunk& srcChunk, // 引っ越し元の倉庫（現在のデータが入っているChunk）
        size_t srcIndex, // srcChunk内でそのエンティティが何番目にあるか
        Chunk& dstChunk, // 引っ越し先の倉庫（新しいデータを入れるChunk）
        EntityID entity, // 引っ越しするエンティティ自身のID
        const std::vector<PendingOp*>& addOps,     // 今回の移動で新しく追加されるコンポーネントの初期データ
        const std::vector<TypeID>& removeTypeIDs   // 今回の移動で削除されるコンポーネントのTypeIDリスト
    ) {
        // MoveEntityBetweenChunks の内部（あるいは呼び出し直前）で
        if (srcChunk.IsEntityDestroyed(srcIndex)) {
            // 既に死んでいるエンティティを移動させることは論理的に不可能
            return false;
        }

        // 1 宛先チャンクにエンティティを挿入し、
        // 　そのインデックス（何番目の席か）を取得
        // これからこのエンティティが入るよと伝えてメモリ領域を確保する
        dstChunk.InsertEntity(entity);
        // 新しく追加されたエンティティは「必ず配列の一番最後」に配置されるため、計算で一瞬で求まる
        size_t dstIndex = dstChunk.GetEntityCount() - 1;

        // 致命的なバグの修正: 新しい住所をマスターマップに登録する 
        size_t dstChunkIdx = GetChunkIndex(&dstChunk);
        UpdateEntityLocation(entity, dstChunkIdx, dstIndex);

        // 「何番目の列に何のデータがあるか」の対応表を作る
#pragma region 解説
        // ここが ECS における非常に重要なポイントです。 
        // アーキタイプ（コンポーネントの組み合わせ）が変わると、
        // データの並び順（列の番号）が変わってしまう可能性があります。

        //srcMap(元の倉庫の地図) : 
        // 「TypeID：Position は 0 番目の列、Velocity は 1 番目の列にある」
        // という情報を保持します。

        //dstMap(新しい倉庫の地図) : 
        // 「TypeID：Position は 0 番目の列、Velocity は 1 番目、HP は 2 番目にある」
        // という情報を保持します。

        //なぜ std::unordered_map を作るのか？
        //引越し作業では、「元の倉庫の Position を、新しい倉庫の Position に移す」
        // という作業を繰り返します。 しかし、Position が元の倉庫では 
        // 0 番目の列にあり、新しい倉庫では 2 番目の列にあるかもしれません。
        // この「地図（Map）」を事前に作っておくことで、
        // 「元の A は何番？ 新しい A は何番？」という検索を高速に行えるようにしています
#pragma endregion
        const Archetype& srcArch = srcChunk.GetArchetype();
        const Archetype& dstArch = dstChunk.GetArchetype();

        // アーキタイプから「どのTypeIDが何番目にあるか」を高速に引くためのマップを作成
        auto BuildIndexMap = [](const Archetype& a) {
            std::unordered_map<TypeID, size_t> m;
            for (size_t i = 0; i < a.size(); ++i) m[a[i].id] = i;
            return m;
            };
        auto srcMap = BuildIndexMap(srcArch);
        auto dstMap = BuildIndexMap(dstArch);


        // 2) 宛先チャンク（dst）の全コンポーネントを1つずつ埋めていく
        // 移動する先のチャンクのエンティティデータを1つずつセットしていく
        for (size_t di = 0; di < dstArch.size(); ++di)
        {
            // アーキタイプにあるコンポーネント一つずつから情報を取得
            const TypeData& tdDst = dstArch[di]; // 新しい倉庫でのコンポーネント情報
            // 宛先の書き込み先ポインタ（住所）を取得
            void* dstPtr = dstChunk.GetComponentPtrByType(tdDst.id, dstIndex);

            // --- パターンA: 元の家から持っていく荷物 ---
            // 新しい家の地図（dstMap）にある型が
            // 古い家の地図（srcMap）にも存在する場合
            auto itSrc = srcMap.find(tdDst.id);
            if (itSrc != srcMap.end())
            {
                // 古い家にも同じ型があるなら、その住所を取得してコピー
                void* srcPtr = srcChunk.GetComponentPtrByType(tdDst.id, srcIndex);

                // ★修正: ムーブ可能な場合はムーブコンストラクタ(mover)を使う
                if (tdDst.mover) {
                    tdDst.mover(dstPtr, srcPtr);
                }
                // ムーブがない場合はコピー(constructor)を試みる
                else if (tdDst.constructor) {
                    tdDst.constructor(dstPtr, srcPtr);
                }
                else {
                    std::memcpy(dstPtr, srcPtr, tdDst.typeSize);
                }
            }
            // --- パターンB: 新しく買った（追加された）荷物 ---
            else
            {
                bool filled = false;
                // addOps（追加予約リスト）の中に、この型の初期データがないか探す
                for (PendingOp *op : addOps) {
                    if (op->type == tdDst.id) {
                        // ★追加: ファクトリ優先
                        if (op->factory) {
                            op->factory(dstPtr);
                            filled = true;
                            break;
                        }
                        // 従来通り
                        else if (op->data) {
                            if (tdDst.constructor)
                                tdDst.constructor(dstPtr, op->data);
                            else
                                std::memcpy(dstPtr, op->data, tdDst.typeSize);
                            filled = true;
                            break;
                        }
                    }
                }
                // --- パターンC: 何も指定がなければ「0」で初期化 ---
                if (!filled)
                {
                    std::memset(dstPtr, 0, tdDst.typeSize);
                }
            }
        }


        // 元のチャンクに残っているデータは、移動・削除に関わらず全て破棄する
        for (size_t si = 0; si < srcArch.size(); ++si)
        {
            // 前のチャンクのアーキタイプ情報を取得
            const TypeData& tdSrc = srcArch[si];
            
            // 古い家からそのデータの場所を特定
            void* srcPtr = srcChunk.GetComponentPtrByType(tdSrc.id, srcIndex);

            // 全員破棄！ (移動済みのものはコピー先で生きているので、ここで消してOK)
            // その型専用の「掃除機（デストラクタ）」で適切に破棄
            tdSrc.destructor.Destruct(srcPtr);
            
            //ChunkManager: 「型は知らないけど、とにかくこの住所のデータを消さなきゃ」
            //Destructor : 「任せろ、私はこの住所がかつて何型だったかを知っている。正しく掃除しておこう」
            
        }

        // ★追加: 引っ越し完了を通知
        if (_onEntityMove) {
            _onEntityMove(entity, &dstChunk, dstIndex);
        }

        // 引っ越し元(srcChunk)のエンティティを
        // 「後で削除する」という印をつける
        // 上で既に全てのデータをdstChunkにコピーし終わっているので、
        // あとはsrcChunkから消すだけ
        srcChunk.MarkDestroyEntity(srcIndex);

        return true;
    }


    std::vector<EntityID> ChunkManager::ApplyPendingOperations(PendingOps& pendingOps)
    {
        // 排他ロックを取得（この間、Systemは待機）
        std::unique_lock<std::shared_mutex> lock(_structureMutex);

        // ECSの構造に変化がない場合は何もしない _pendingOps（変更予定リスト）
        std::vector<EntityID> destroyedEntities; // 今回削除されたIDを記録
        if (pendingOps.Ops().empty()) return destroyedEntities;

        // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━
        //  Entity単位に再編成
        // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━
#pragma region 構図
        // PendingOps を Entity ごとに束ねる
        // Entityごとに対する操作一覧
        // _pendingOps(フラット)
        // --------------------------------
        // [Entity10 AddTransform]
        // [Entity20 RemovePhysics]
        // [Entity10 AddModel]
        // [Entity30 Spawn]
        // --------------------------------
        //
        // opsByEntity（再構成後）
        // --------------------------------
        // Entity10 →[AddTransform, AddModel]
        // Entity20 →[RemovePhysics]
        // Entity30 →[Spawn]
        // --------------------------------

        // １Entityの最終状態を一気に計算できるようになる
#pragma endregion
        std::unordered_map<EntityID, std::vector<PendingOp*>> opsByEntity;
        for (auto& op : pendingOps.Ops()) {
            // 変更した要素をEntityごとにまとめる
            opsByEntity[op.entity].push_back(&op);
        }


        // 削除済みエンティティを追跡するセット（二重処理防止）
        std::unordered_set<EntityID> alreadyDestroyed;


        for (auto& [entityID, ops] : opsByEntity)
        {
            // ---------------------------------------------------------
            // ステップ1: 破棄(Destroy)の優先チェック
            // ---------------------------------------------------------
            bool hasDestroyMarker = false;
            for (auto* op : ops) {
                if (op->kind == PendingOpKind::Destroy) {
                    hasDestroyMarker = true;
                    break;
                }
            }

            if (hasDestroyMarker) {
                // 既に削除済みなら二重処理をスキップ
                if (alreadyDestroyed.find(entityID) != alreadyDestroyed.end()) {
                    continue;
                }

                // 既存のエンティティがいれば、デストラクタを呼んでマークする
                size_t entityIndex = 0;
                // SerchEntityIn でチャンクとインデックスを取得
                size_t chunkIdx = SearchEntityIn(entityID, &entityIndex);

                if (chunkIdx != InvalidIndex) {
                    // 実際のチャンクポインタを取得
                    Chunk* cptr = _chunks[chunkIdx].get();

                    // 既に削除マークが付いている場合もスキップ
                    if (cptr->IsEntityDestroyed(entityIndex)) {
                        alreadyDestroyed.insert(entityID);
                        continue;
                    }

                    // コンポーネントの破棄（デストラクタ呼び出し）
                    // ArcheTypeはTypeDataのリストなのでforループで回せる（特殊な処理により）
                    // 各コンポーネント型ごとにfor文を回す
                    for (const TypeData& td : cptr->GetArchetype()) {
                        // GetComponentPtrByType : 「td.id型のentityIndex番目のデータの住所」を取得
                        // ① td.id を使って、Chunk内の「対応する配列の先頭アドレス」を見つける
                        // ② entityIndex を使って、その配列の「何番目（特定のEntity）」かを計算する
                        void* ptr = cptr->GetComponentPtrByType(td.id, entityIndex);

                        // ③ 特定された「一個のコンポーネント」に対してデストラクタを呼ぶ
                        // ptrが指すメモリの位置をその型に合わせたtd.destructorで破棄
                        td.destructor.Destruct(ptr);


                    }
                    // 削除マーク
                    cptr->MarkDestroyEntity(entityIndex);

                    // 唯一追加すべき修正行：住所録（Map）からの完全抹消 
                    RemoveEntityLocation(entityID);

                    // 削除されたIDをリストに追加
                    alreadyDestroyed.insert(entityID);
                    destroyedEntities.push_back(entityID);
                }
                // 破棄対象なら、そのエンティティに対する他の操作(Add/Remove)はすべて無視して次へ
                continue;
            }

            // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━
            // 既存状態の確認と「生存チェック」
            // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━
            size_t srcIndex = 0;
            size_t chunkIdx = SearchEntityIn(entityID, &srcIndex);
            Chunk* srcChunk = (chunkIdx != InvalidIndex) ? _chunks[chunkIdx].get() : nullptr;

            // 削除済みエンティティへの操作をスキップ
            if (alreadyDestroyed.find(entityID) != alreadyDestroyed.end()) {
                continue;
            }

            // 既に削除マークがついている場合もスキップ
            if (srcChunk && srcChunk->IsEntityDestroyed(srcIndex)) {
                continue;
            }

            // --- ベースとなるアーキタイプの決定 ---
            Archetype nextArch; // 次のアーキタイプ候補
            bool isNewSpawn = false;

            // srcChunkを基準にするかSpawnで新規作成するか
            // opsの中に Spawn があるか探す
            for (auto* op : ops)
            {
                if (op->kind == PendingOpKind::Spawn)
                {
                    // spawnArchetype は PendingOp のメンバ（値）なので、
                    // FrameAllocator のライフタイムに無関係。常に安全にアクセスできる。
                    nextArch = op->spawnArchetype;
                    isNewSpawn = true;
                    break;
                }
            }

            // Spawnでなければ、現在のチャンクの構成を引き継ぐ
            if (!isNewSpawn) {
                if (!srcChunk) continue; // 存在しないしSpawnでもないなら無視
                // 既存エンティティ：現在のチャンクのアーキタイプを基準にする
                nextArch = srcChunk->GetArchetype();
            }

            // 最後に必要なコンポーネントのリストを確定させる
            // --- 差分（Add/Remove）の計算 ---
            std::vector<PendingOp*> addOps; // 実際に新しく追加するデータ
            std::vector<TypeID> removeIDs;

            for (auto* op : ops) {
                if (op->kind == PendingOpKind::AddComponent) {
                    // アーキタイプに追加する必要があるかどうかの判定とは別に、
                    // 「初期値データ」として addOps には必ず入れる
                    addOps.push_back(op);

                    bool has = false;
                    for (auto& td : nextArch) if (td.id == op->type) has = true;
                    if (!has) {
                        // アーキタイプに追加する際、関数ポインタを含めた完全なTypeDataを作成する
                        TypeData newType;
                        newType.id = op->type;
                        newType.typeSize = op->typeSize;

                        newType.destructor = op->destructor;
                        newType.constructor = op->constructor;
                        newType.assigner = op->assigner;
                        newType.mover = op->mover;

                        newType.name = op->name; // 名前を新しいアーキタイプに引き継ぐ

                        nextArch.Add(newType);
                    }
                }
                else if (op->kind == PendingOpKind::RemoveComponent) {
                    removeIDs.push_back(op->type);
                    nextArch.Sub(op->type);
                }
            }

            // --- 最終的な確定処理 ---
            // 最後に計算したnextArchに基づいてチャンク移動を行う
            if (isNewSpawn) {   // Spawnの場合
                // 新規生成：最終的な nextArch を持つチャンクに直接入れる
                Chunk& dstChunk = GetOrCreateAvailableChunk(nextArch);
                // ※ここでデータを流し込むための「InsertAndFill」のような関数が必要
                InsertNewEntity(dstChunk, entityID, addOps);
            }
            // 既存エンティティでアーキタイプが変わる場合
            else if (!(nextArch == srcChunk->GetArchetype())) {
                // アーキタイプ変更：別チャンクへ引っ越し
                Chunk& dstChunk = GetOrCreateAvailableChunk(nextArch);

                // MoveEntityBetweenChunks前に再度削除チェック
                if (!srcChunk->IsEntityDestroyed(srcIndex)) {
                    MoveEntityBetweenChunks(*srcChunk, srcIndex, dstChunk, entityID, addOps, removeIDs);
                }
            }
            else {
                // アーキタイプが変わらない（既にコンポーネントを持っている）場合、
                // PendingOps に積まれた新しいデータで中身を上書きする。
                for (auto* op : ops)
                {
                    if (op->kind == PendingOpKind::AddComponent)
                    {
                        void* dstPtr = srcChunk->GetComponentPtrByType(op->type, srcIndex);
                        // ポインタが有効で、かつ「ファクトリ」または「データ」がある場合
                        if (dstPtr && (op->factory || op->data)) {
                            const Archetype &arch = srcChunk->GetArchetype();
                            for (const auto &td : arch) {
                                if (td.id == op->type) {
                                    //  パターン1: ファクトリ関数がある場合 (Move-Only型など)
                                    // 上書き代入ができないため、「破棄 -> 新規構築」の手順を踏む
                                    if (op->factory) {
                                        // 既存のオブジェクトを安全に破棄
                                        td.destructor.Destruct(dstPtr);

                                        // 新しいオブジェクトを直接構築 (Placement New)
                                        op->factory(dstPtr);
                                    }
                                    //  パターン2: データポインタがある場合 (従来のコピー可能な型)
                                    else if (op->data) {
                                        // assigner を使用（既に構築済みなので代入演算子が使える）
                                        if (td.assigner) {
                                            td.assigner(dstPtr, op->data);
                                        }
                                        else {
                                            std::memcpy(dstPtr, op->data, td.typeSize);
                                        }
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            // ---------------------------------------------------------
            // ステップX: Patch(部分修正)の適用
            // ---------------------------------------------------------
            // 移動・生成が完了した「最終的な場所」を取得
            size_t finalIndex    = 0;
            size_t finalChunkIdx = SearchEntityIn(entityID, &finalIndex);

            if (finalChunkIdx != InvalidIndex) {
                Chunk *finalChunk = _chunks[finalChunkIdx].get();

                for (auto *op : ops) {
                    if (op->kind == PendingOpKind::PatchComponent) {
                        // 対象コンポーネントのポインタを取得
                        void *ptr = finalChunk->GetComponentPtrByType(op->type, finalIndex);

                        // ポインタが存在すれば（コンポーネントを持っていれば）、修正関数を実行
                        if (ptr && op->patcher) {
                            op->patcher(ptr);
                        }
                    }
                }
            }
        }

        // 4. 掃除
        for (auto &c : _chunks) {
            c->CompactHoles([this](EntityID entity, Chunk *chunk, size_t newIndex) {
                // 位置を更新
                size_t chunkIdx = GetChunkIndex(chunk);
                UpdateEntityLocation(entity, chunkIdx, newIndex);

                // 既存のコールバックも呼ぶ（Worldの_entityCacheも更新される）
                if (_onEntityMove) {
                    _onEntityMove(entity, chunk, newIndex);
                }
            });
        }

        return destroyedEntities;
    }
}
