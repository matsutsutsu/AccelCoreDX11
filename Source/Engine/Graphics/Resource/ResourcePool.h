#pragma once
#include <cassert>
#include <cstdint>
#include <utility>
#include <vector>
#include <deque>

namespace CCL {

    // 8バイトの超軽量ハンドル（ECSコンポーネントに持たせる用）
    // 
    template <typename T> struct Handle {
        //0xFFF...はuint32の一番大きな数字、無効な値を表すために
        uint32_t index      = 0xFFFFFFFF;   // 配列の何番目にあるか 
        uint32_t generation = 0;            // その場所が「何回使い回されたか」

        // このチケットが空っぽじゃないかどうかを確認するための関数
        bool IsValid() const { return index != 0xFFFFFFFF; }

        // !=　２つを比較して同じものを指しているかどうかの比較演算子
        bool operator==(const Handle &other) const
        {
            return index == other.index && generation == other.generation;
        }
        bool operator!=(const Handle &other) const { return !(*this == other); }

        // ソート（std::sort）やマップ（std::map）で使うために必須
        bool operator<(const Handle &other) const
        {
            // まず部屋番号(index)で比較
            if (index != other.index) {
                return index < other.index;
            }
            // 部屋番号が同じなら、世代(generation)で比較
            return generation < other.generation;
        }
    };

    // 型安全かつキャッシュ効率が極大化されたメモリプール
    template <typename T> class ResourcePool {
      private:
        
        struct Slot {
            // alignas(T)　メモリの部屋を用意するための指示　メモリアライメント
            // unsigned char data[...]　ただの空間（バイト配列）
            // sizeof(T)　Tクラスのデータの広さ
            alignas(T) unsigned char data[sizeof(T)];   // 型Tのサイズ分の「単なるバイト配列」
            uint32_t generation = 0;
            bool     active     = false; // 部屋に有効なデータが入っているかどうかのフラグ

            // dataはただのバイトの集まりなので
            // そのままだとモデルだとは認識してもらえないので
            // reinterpret_castは「このバイトの塊を、強制的に型Tのポインタ
            // として解釈して返す」 翻訳機
            T *GetPtr() { return reinterpret_cast<T *>(data); }
        };

        // アパートの部屋全体の配列
        // ===================================================================
        // ★修正: vector を deque に変更！
        // これにより、アパートの増築時に住人の「お引越し」が絶対に発生しなくなり、
        // 外部に配ったポインタ(T*)が永遠に安全に保たれるようになります。
        // ===================================================================
        std::deque<Slot>      m_slots;
        // 空き部屋リスト
        // 大挙して空っぽになった部屋の番号(index)を保管する場所
        std::vector<uint32_t> m_freeIndices;

      public:
        ~ResourcePool() { Clear(); }

        template <typename... Args> Handle<T> Create(Args &&...args)
        {
            uint32_t index;
            if (!m_freeIndices.empty()) {
                // もし空き部屋リストにメモがあれば
                // そこから一つの番号を取り出して使う
                index = m_freeIndices.back();
                m_freeIndices.pop_back();
            }
            else {
                // 空き部屋が一つもなければ、アパートを増築（配列を一つ拡張）
                // して新しい部屋番号にする
                index = static_cast<uint32_t>(m_slots.size());
                m_slots.emplace_back();
            }

            // 決まった部屋番号のスロット（部屋）を取り出す
            Slot &slot = m_slots[index];            
            
            // slot.data（用意しておいた空部屋）の中にT（データ）を直接組み立てる
            new (slot.data) T(std::forward<Args>(args)...); // Placement New
            slot.active = true;  // 入居中のフラグを立てる

            // 部屋番号と、その部屋の今の世代を書き込んだチケット(Handle)を発行して返す
            return Handle<T>{index, slot.generation};
        }

        // クラッシュさせないための最大防壁
        // 中身のハンドルが古くてもnullptrを渡して安全に処理をスキップできる
        T *Get(Handle<T> handle)
        {
            // チケットが空っぽ（0xFFFFFFFF）か、
            // アパートの部屋数より大きいデタラメな番号なら追い返す
            if (!handle.IsValid() || handle.index >= m_slots.size()) return nullptr;

            // チケットの部屋番号へ行く
            Slot &slot = m_slots[handle.index];

            // 「現在入居中(active)」かつ「チケットの世代と、
            // 部屋の今の世代が一致している」か確認する
            if (slot.active && slot.generation == handle.generation) {
                // 面会許可。翻訳機を通してポインタを渡す
                return slot.GetPtr();
            }
            return nullptr;// 世代が違う（＝前の住人のチケットで来ている）場合は絶対に会わせない
        }

        // 部屋の中をデストラクタを呼び出して破壊する
        void Destroy(Handle<T> handle)
        {
            // デタラメな番号なら何もしない
            if (!handle.IsValid() || handle.index >= m_slots.size()) return;

            Slot &slot = m_slots[handle.index];
            if (slot.active && slot.generation == handle.generation) {
                // この書き方は部屋のメモリは壊さないで
                // 中身のデータだけをデストラクタするための呼び方
                slot.GetPtr()->~T(); // デストラクタの明示的呼び出し
                slot.active = false; // 「空き部屋」の札を下げる
                slot.generation++;   // 「次の住人は○代目ですよ」と世代を1つ進める

                // この部屋番号を「空き部屋リスト」に追加する
                m_freeIndices.push_back(handle.index);
            }
        }

        // 次のシーンに行くときなどメモリリークを防ぐための終了関数
        void Clear()
        {
            for (auto &slot : m_slots) {
                if (slot.active) {
                    slot.GetPtr()->~T(); // 住んでいる部屋だけ、全員の荷物を片付ける
                    slot.active = false;
                }
            }
            m_slots.clear();       // アパートの配列自体を空にする
            m_freeIndices.clear(); // 空き部屋リストもリセット
        }
    };

} // namespace CCL