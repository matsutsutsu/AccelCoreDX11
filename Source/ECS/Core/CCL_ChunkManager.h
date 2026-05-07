#pragma once
#include <vector>
#include <memory>
#include "../Common/CCL_Common.h"
#include "CCL_Chunk.h"
#include "CCL_PendingOps.h"
#include <functional>
#include <unordered_map> 
#include <shared_mutex>

namespace CCL::ECS::Core
{
    using namespace CCL::ECS;

    //1. ChunkManager の役割（概要）
    //   ECSにおいて、エンティティはコンポーネントが追加・削除されるたびに、
    // 　その構成（アーキタイプ）が変わります。 ChunkManager の最大の仕事は、
    // 「エンティティをその時々の構成に最適な Chunk（倉庫）へ振り分け、移動させること」
	//  です。
    class ChunkManager
    {
    public:
        ChunkManager() = default;
        ~ChunkManager() = default;

        // 指定されたArchetypeに対応する新しいChunkを作成して_chunkに追加する
        void CreateChunk(const Archetype& archetype);
        // 管理下の全チャンクに対してFinalizeを呼び出し全てのEntityとデータを破棄する
        void FinalizeAll();

        // 新しいエンティティを生成し、指定されたArchetypeに対応するチャンクに挿入
        void AddEntity(EntityID entity, const Archetype& archetype);
        // 指定されたエンティティを破棄リストに入れる
        void DestroyEntity(EntityID entity);

        // ECS処理中に発生した変更リスト(PendingOps）を受け取り
        //（Entityの追加、削除、コンポーネント変更）を一括処理する
        std::vector<EntityID> ApplyPendingOperations(PendingOps& pendingOps);

        // Archetype遷移の中核ロジック
        // ソースチャンクからEntityのデータをコピー→デストラクタを呼び、
        // 新しいチャンクにコピー構築して移動を完了する
#pragma region MoveEntityBetweenChunks概要
        // これが ECSの中で最も重要かつ複雑な処理 です。 例えば、
        // あるエンティティに Velocity（速度）コンポーネントが追加された場合、
        // 以下の手順で「引越し」をさせます。

        //１，今の倉庫（位置のみ）からデータを取り出す。

        //２，新しい倉庫（位置＋速度）に席を確保する。

        //３，共通するデータ（位置）だけを新しい倉庫にコピーする。

        //４，新しく追加されたデータ（速度）を書き込む。

        //５，古い倉庫の席を空ける（削除する）。
#pragma endregion
        bool MoveEntityBetweenChunks(
            Chunk& srcChunk, size_t srcIndex,
            Chunk& dstChunk, EntityID entity,
            const std::vector<PendingOp*>& addOps,
            const std::vector<TypeID>& removeTypeIDs
        );

        // 指定されたArchetypeと完全に一致するチャンクが既に存在しているか検索
        size_t SearchEqualChunk(const Archetype& a) const;

        std::vector<std::unique_ptr<Chunk>>& GetChunks() { return _chunks; }

        // const版（読み取り専用の時用）
        const std::vector<std::unique_ptr<Chunk>>& GetChunks() const { return _chunks; }

        // 指定されたEntityIDがどのチャンクに存在し、チャンク内のどのインデックスにあるか検索
        size_t SearchEntityIn(EntityID entity, size_t* entityIndexOut = nullptr) const;

        // Worldから「キャッシュ更新関数」を受け取る
        using EntityMoveCallback = std::function<void(EntityID, Chunk *, size_t)>;
        void SetEntityMoveCallback(EntityMoveCallback cb) { _onEntityMove = cb; }

        // 構造変更用のmutexを取得（SystemManagerから使用される）
        std::shared_mutex       &GetStructureMutex() { return _structureMutex; }
        const std::shared_mutex &GetStructureMutex() const { return _structureMutex; }
    


    private:
        // 指定したアーキタイプで、かつ空きがあるチャンクを返す（なければ作る）
        Chunk& GetOrCreateAvailableChunk(const Archetype& archetype);

		// 新しいエンティティを指定されたチャンクに挿入し、コンポーネントデータを初期化する
        void InsertNewEntity(
			Chunk& dstChunk,    // 挿入先チャンク
			EntityID entity,    // 挿入するエンティティID
			const std::vector<PendingOp*>& addOps   // 今回の移動で新しく追加されるコンポーネントの初期データ
        );

         // Entityの位置情報をキャッシュするための構造体
        struct EntityLocation {
            size_t chunkIndex;  // どのChunkにいるか
            size_t entityIndex; // Chunk内のどのインデックスか

            EntityLocation() : chunkIndex(InvalidIndex), entityIndex(InvalidIndex) {}
            EntityLocation(size_t ci, size_t ei) : chunkIndex(ci), entityIndex(ei) {}
        };

        // Entityの位置を更新する内部ヘルパー
        void UpdateEntityLocation(EntityID entity, size_t chunkIdx, size_t entityIdx);

        // Entityの位置情報を削除する内部ヘルパー
        void RemoveEntityLocation(EntityID entity);

        // ChunkポインタからChunk配列内のインデックスを取得
        size_t GetChunkIndex(const Chunk *chunk) const;

      
    private:
		// このChunkManagerが管理する全チャンクのリスト
		// ・外側のvector: チャンクごと
		// ・内側のunique_ptr: チャンク本体
        // 「位置情報だけを持つ倉庫」「位置と速度を持つ倉庫」など
        // アーキタイプごとに作成された倉庫を全て保持している
        std::vector<std::unique_ptr<Chunk>> _chunks;


        // チャンク1つあたりの固定メモリサイズ（例: 16KB = 16384 バイト）
        // ※ 16KBはL1/L2キャッシュに収まりやすく、バランスの良い標準サイズです
        // なぜ16000ではなく16384かというと、16384は2の14乗であり、コンピュータのメモリ管理において効率的なサイズだから
        static constexpr size_t CHUNK_SIZE_BYTES = 16384;

        // アーキタイプ（構成）から、このチャンクに何人入れるか（定員）を計算する
        size_t CalculateChunkCapacity(const Archetype &archetype) const;


        EntityMoveCallback _onEntityMove = nullptr;

        // EntityID → (ChunkIndex, EntityIndex) の高速検索マップ     
        std::vector<EntityLocation> _entityLocationMap;

        // 構造変更用の共有ミューテックス
        // - 読み取り（System実行）: shared_lock（複数スレッド並列OK）
        // - 書き込み（ApplyPendingOperations）: unique_lock（排他的）
        mutable std::shared_mutex _structureMutex;

    };
}
