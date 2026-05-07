#pragma once
// CCL_PendingOps.h
// Component Add/Remove の遅延処理リスト

#include "../Common/CCL_Common.h"
#include <vector>
#include <memory>
#include <functional>

namespace CCL::ECS::Core
{
    enum class PendingOpKind
    {
        Spawn,
        Destroy,
        AddComponent,
        RemoveComponent,
        PatchComponent, // コンポーネントの一部修正予約

        None
    };

    // 構造的変更要求をカプセル化
    struct PendingOp
    {
        PendingOpKind kind = PendingOpKind::None;     // 実行する操作の種類
        EntityID entity = 0;        // 操作対象のエンティティID

        TypeID type = 0;            // 操作対象のコンポーネントのTypeID  
        size_t typeSize = 0;        // 操作対象のコンポーネントのサイズ

        // data=nullptr の場合もある
        void* data = nullptr;   // AddComponentの場合追加されるコンポーネント初期データが格納されているヒープメモリへのポインタ
        Destructor destructor;  // dataに格納されたコンポーネントデータを破棄するためのオブジェクト

        // ★追加: 生成用ファクトリ関数 (Emplace用)
        // AddComponentで渡されたラムダ式をここに保存し、ChunkManagerまで運びます
        std::function<void(void *)> factory = nullptr;

        // ★追加: 修正用の関数（Patch処理）
        // 既に存在するコンポーネントのポインタを受け取り、内部を書き換えるラムダ式
        std::function<void(void *)> patcher = nullptr;

        // ★【追加】コピー・移動・代入用の関数ポインタ
        // これらを ChunkManager まで伝達しないと、memcpy が発動してクラッシュします
        TypeData::ConstructFunc constructor = nullptr; // コピー構築 (Placement New)
        TypeData::AssignFunc    assigner    = nullptr; // 代入 (Operator=)
        TypeData::MoveFunc      mover       = nullptr; // 移動 (Move)

        // デバッグ用の型名（XMTLで読み取り用）
        const char *name = nullptr;

        // ★ 修正: Spawn時のアーキタイプを値で保持する
        //   旧実装: data = FrameAllocator上のArchetype* → Reset()後にダングリング→クラッシュ
        //   新実装: spawnArchetype に直接コピー → FrameAllocatorに依存しないので安全
        Archetype spawnArchetype;
    };

    // ECSの実行フェーズ（Systemの更新中など）で発生したエンティティへの
    // 構造的変更要求（コンポーネント追加・削除）を一時的に記録し、
    // フレームの終了時など安全なタイミングで一括処理することを可能にする
    class PendingOps
    {
    public:
        PendingOps() = default;
        ~PendingOps() { Clear(); }

        // 新しいPendingOpをリスト_opsに追加します
        void Add(const PendingOp& op)
        {
            _ops.push_back(op);
        }

        std::vector<PendingOp>& Ops() { return _ops; }

        // リストをクリアする際に、AddComponentの操作で一時的に確保された
        // ヒープメモリ（op.data）を安全に解放します
        void Clear()
        {
            for (auto& op : _ops)
            {
                if (op.data == nullptr) continue;

                if (op.kind == PendingOpKind::AddComponent)
                {
                    op.destructor.Destruct(op.data);
                }

                op.data = nullptr;
            }
            _ops.clear();
        }

        bool Empty() const { return _ops.empty(); }

    private:
        std::vector<PendingOp> _ops;
    };
}
