#include "CCL_Chunk.h"
#include <algorithm>
#include <cstring>
#include <cassert>


namespace CCL::ECS::Core
{
    using namespace CCL::ECS;

    // ヘルパー：指定したアライメント境界へサイズを切り上げる
    inline size_t AlignUp(size_t size, size_t alignment)
    {
        return (size + alignment - 1) & ~(alignment - 1);
    }

    Chunk::~Chunk()
    {
        Finalize();
    }

    void Chunk::Create(const Archetype& archetype, size_t initialEntityCapacity)
    {

        _archetype = archetype;
		_capacity = initialEntityCapacity;
        _entityStride = 0;
        for (const TypeData& t : _archetype) _entityStride += t.typeSize;

        _ownerEntities.assign(initialEntityCapacity, InvalidEntityID);

        // 単一メモリブロックの一括確保ロジック
        size_t compCount = _archetype.size();
        _componentOffsets.resize(compCount); // 白線を引く準備

        size_t       currentOffset = 0; // 現在の白線の位置（0バイト目からスタート）
        const size_t ALIGNMENT     = 16; // SIMD命令等を見据えた16バイトアライメント

        for (size_t i = 0; i < compCount; ++i)
        {
            // 1. CPUが計算しやすいように、白線の位置を「16の倍数」にキリよく調整する
            currentOffset = AlignUp(currentOffset, ALIGNMENT);

            // 2. 「このコンポーネントのエリアはここから始まる」とメモ帳に記録
            _componentOffsets[i] = currentOffset;

            // 3. 次の白線を引くために、このコンポーネントが占領するサイズ分だけ位置を進める
            // （例：Position(12byte)が100人分なら、1200バイト先へ）
            currentOffset += _archetype[i].typeSize * _capacity;
        }

        // 4. 計算し尽くした「総面積(currentOffset)」で、1回だけ巨大な体育館を借りる！
        // 計算し尽くした「必要な総バイト数」で、1回だけメモリをドカンと確保する
        _memoryBlock.resize(currentOffset);

        _entityCount = 0;
        _destroyedEntities.clear();
    }

    void Chunk::CreateNull(size_t initialEntityCapacity)
    {
        _archetype = Archetype();
        _capacity = initialEntityCapacity;
        _entityStride = 0;
        _ownerEntities.assign(initialEntityCapacity, InvalidEntityID);


        _componentOffsets.clear();
        _memoryBlock.clear();

        _entityCount = 0;
        _destroyedEntities.clear();
    }

    void Chunk::Finalize()
    {
        // 既にFinalize済み（または初期化前）なら何もしない（二重解放・クラッシュ防止）
        if (_componentOffsets.empty()) return;

        // 1. 体育館の中にいる全員のデストラクタ（お掃除関数）を呼ぶ
        for (size_t ci = 0; ci < _archetype.size(); ++ci) {

            size_t sz         = _archetype[ci].typeSize;
            size_t baseOffset = _componentOffsets[ci]; // 白線の位置

            for (size_t i = 0; i < _entityCount; ++i) {
                if (!IsEntityDestroyed(i)) {
                    // 体育館の正確な場所を特定して、お掃除を実行
                    void *ptr = _memoryBlock.data() + baseOffset + (i * sz);
                    _archetype[ci].destructor.Destruct(ptr);
                }
            }
        }

        // 2. 最後に体育館をドカンと一括で返却する
        _memoryBlock.clear();
        _componentOffsets.clear(); // 白線の情報を消去（旧 _components.clear() の代わり）
        _ownerEntities.clear();    // 座席表をクリア
        _destroyedEntities.clear();
        _entityCount = 0;

    }


    void* Chunk::InsertEntity(EntityID entity)
    {
        // 1. AddEntity内で「満員でないか」だけをチェックし、座席番号をもらう
        size_t writeIndex = AddEntity(entity);

        // 2. 各コンポーネントのエリアに行き、そこを「ゼロ」で水拭き（初期化）する
        for (size_t ci = 0; ci < _archetype.size(); ++ci) {
            void *dst = GetComponentPtr(ci, writeIndex); // 体育館の正確な場所
            std::memset(dst, 0, _archetype[ci].typeSize);
        }
        return GetComponentPtr(0, writeIndex);
    }



    void Chunk::RemoveEntity(size_t entityIndex)
    {
        // 不正なインデックスなら何もしない
        if (entityIndex >= _entityCount) return;

        // このエンティティが持つ全てのコンポーネントの「お掃除関数」を実行
        for (size_t ci = 0; ci < _archetype.size(); ++ci) {
            // 新設計：専用関数を使って、体育館の床の正確なポインタを一発で取得
            void *ptr = GetComponentPtr(ci, entityIndex);

            // お掃除実行（中身のメモリ解放など）
            _archetype[ci].destructor.Destruct(ptr);
        }

        // 持ち主の座席を「無効」としてマーク。
        // ※実際にメモリの穴を詰める(Compact)のは、あとで一括でやります。
        _ownerEntities[entityIndex] = InvalidEntityID;
    }

    void Chunk::MarkDestroyEntity(size_t entityIndex)
    {

        if (entityIndex >= _entityCount) return;

        EntityID id = _ownerEntities[entityIndex];

        // mark owner slot invalid; actual data will be compacted later
        _ownerEntities[entityIndex] = InvalidEntityID;
        _destroyedEntities.push_back(entityIndex);
    }



    // CCL_Chunk.cpp の CompactHoles 修正版
    void Chunk::CompactHoles(const std::function<void(EntityID, Chunk *, size_t)> &onMoveCallback)
    {
        // 削除されたエンティティが一つもなければ、穴埋めする必要なし
        if (_destroyedEntities.empty()) return;

        size_t read  = 0; // 読み込み用の指（走査用）
        size_t write = 0; // 書き込み用の指（詰め先用）

        while (read < _entityCount) 
        {
            EntityID currentID = _ownerEntities[read];

            // この座席が「空席（すでに死んでいる）」かどうかの判定
            bool isDead = (currentID == InvalidEntityID);

            if (isDead) {
                // すでに RemoveEntity でデストラクタは呼ばれている。
                // したがって、ここは「書き込み位置(write)」を進めずに、
                // 「読み込み位置(read)」だけを1つ進める（穴をスキップする）だけでよい。

                ++read;
                continue;
            }

            // 生きているエンティティで、かつ前に「穴」があった（read と write がズレた）場合のみ前に詰める
            if (read != write) {
                // 名簿の書き換え
                _ownerEntities[write] = currentID;
               

                // 各コンポーネントのデータを、後ろの席(read)から前の席(write)へ移動
                for (size_t ci = 0; ci < _archetype.size(); ++ci) {
                    size_t baseOffset = _componentOffsets[ci];
                    void  *dst =
                        _memoryBlock.data() + baseOffset + (write * _archetype[ci].typeSize);
                    void *src = _memoryBlock.data() + baseOffset + (read * _archetype[ci].typeSize);

                    if (_archetype[ci].mover) {
                        // ムーブコンストラクタ等の専用移動関数があればそれを使う
                        _archetype[ci].mover(dst, src);
                    }
                    else {
                        // なければメモリごと物理コピー
                        std::memmove(dst, src, _archetype[ci].typeSize);
                    }

                    // 移動元の「抜け殻」の後始末
                    // srcのデータはdstに移動（ムーブ）したため、srcには空っぽのデータが残っています。
                    // （std::stringのムーブ後や、shared_ptrのムーブ後など）
                    // これを安全に消滅させるために、ここで一度だけデストラクタを呼びます。
                    _archetype[ci].destructor.Destruct(src);
                }

                // 移動が発生した事実を、ChunkManagerに報告して全体マップを更新してもらう
                if (onMoveCallback) {
                    onMoveCallback(currentID, this, write);
                }
            }

            // 次の座席へ
            ++read;
            ++write;
        }

        // 最後に、生き残った人数（write）に定員カウンターを更新
        _entityCount = write;
        // ゴミ箱を空にする
        _destroyedEntities.clear();
    }


    size_t Chunk::AddEntity(EntityID entity) {

        // 満員でないかチェック
        if (_entityCount >= _capacity)
        {
            assert(false && "Chunk is full!");
            return InvalidIndex; // クラッシュを防ぐ
        }

        // 新しい人の番号（現在の入場者数）
        size_t index = _entityCount;

        // 誰が入っているか名簿に記録
        _ownerEntities[index] = entity;

        // ★旧コードではここにあった _components へのアクセスや
        // メモリのresize処理は、事前一括確保済みのため完全に不要です。
        // エンティティの数を1つ増やすだけで完了します。

        // 入場者カウンターを+1するだけ！ メモリの確保は一切発生しない。
        _entityCount++;
        return index; // ① インデックスを返す
    }

}
