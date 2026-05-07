#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>


#include "tracy/Tracy.hpp"


namespace CCL::ECS::Core {

    // フレーム毎にリセットする超高速アロケータ
    class FrameAllocator {
      public:
        FrameAllocator(size_t pageSize = 1024 * 1024) : _pageSize(pageSize)
        {
            // 最初から1ページ確保しておく
            _pages.reserve(8);
            // 初期値を -1 にしておくことで、最初の AllocateNewPage で 0 になるように調整
            _pageIndex = -1;
            AllocateNewPage(pageSize);
        }

        ~FrameAllocator()
        {
            // ★重要: 終了時にデストラクタを呼ぶ
            RunDestructors();

            for (auto *p : _pages) {
                // ★追加: OSにメモリを返す瞬間をTracyに通知
                TracyFree(p);


                ::operator delete(p);
            }
        }

        // メモリ確保（ポインタをずらすだけ）
        void *Alloc(size_t size)
        {
            // 8バイトアライメント
            // これは **「きりの良い数字（8の倍数）に切り上げる」 **という計算です。
            // CPUはデータを読むとき、8バイト区切り（64bit）で並んでいると一番速く読めます。
            // 例えば「1バイト欲しい」と言われても、「8バイト」確保して渡します。隙間を開けて整列させるためです。
            size = (size + 7) & ~7;

            //「今のページ（落書き帳）の余白」が足りるかチェックします。g           
            //足りなければ AllocateNewPageで新しいページ（次の紙）を用意します。
            if (_offset + size > _pageSize) {
                AllocateNewPage(size);
            }

            //ここが爆速の理由です
            //「ここを使っていいよ」という場所（ポインタ）は、「ページの先頭 +
            //現在位置」 だけで決まります。OSへの問い合わせは一切ありません。
            void *ptr = _currentPage + _offset;

            //「ここまで使ったから、次はここからね」と、
            // 現在位置をずらすだけ。これで確保完了です。
            _offset += size;

            return ptr;
        }

        //Alloc は単なる「メモリの場所」を返すだけですが、New
        //はそこで「クラスの初期化（コンストラクタ）」までやってくれます。
        template <class T, typename... Args> T *New(Args &&...args)
        {
            // 1. 場所を確保
            //new (mem) T(...)
            //これを「Placement New（配置ニュー）」と呼びます。
            //普通の new T は「土地を買って家を建てる」ことですが、
            //Placement Newは *
            //*「既に持っている土地（mem）を指定して、そこに家を建てる」 *
            //*ことです。Allocでもらった場所に直接データを書き込みます。
            void *mem = Alloc(sizeof(T));

            // 2. placement new (配置new)
            T *ptr = new (mem) T(std::forward<Args>(args)...);

            // 3. デストラクタ登録
            //if constexpr (!std::is_trivially_destructible_v<T>)
            //これは「T型は、ちゃんと後片付けが必要な難しいクラスか？」を判定しています。
            //int や float なら単純な数字なので後片付け不要（何もしない）。
            //std::vector や
            //std::string を持っているクラスなら、中身を delete しないとメモリリークします。
            //そういう「難しいクラス」の場合だけ、「リセットするときにこのデストラクタを呼んでね」と
            //_destructors リストにメモしておきます。
            if constexpr (!std::is_trivially_destructible_v<T>) {
                _destructors.push_back([ptr]() { ptr->~T(); });
            }

              return ptr;
        }

        // フレームの終わりに呼ぶ（デストラクタは呼ばず、ポインタを戻すだけ）
        void Reset()
        {
            // ★重要: リセット時にデストラクタを呼ぶ
            //さっき登録した「後片付けリスト」を実行します。std::vector
            //などを持っているオブジェクトの中身を綺麗にします。
            RunDestructors();

            
            // このフレームで「実際に消費したバイト数」をグラフにプロット
            size_t totalUsed = (_pageIndex * _pageSize) + _offset;
            // "FrameAllocator Usage" という名前の折れ線グラフがTracy上に作成されます
            TracyPlot("FrameAllocator Usage", static_cast<int64_t>(totalUsed));


            //ここが最強のポイントです。
            //確保したメモリ（ページ）を delete（OSへ返却）しません。
            //ただ **「書き込み位置を0ページ目の、0行目に戻す」 **だけです。
            //これで、次のフレームではまた同じメモリ領域を最初から上書きして使えます。これが再利用です。
            _pageIndex = 0;
            _offset    = 0;
            if (!_pages.empty()) _currentPage = _pages[0];
        }

      private:
        // デストラクタを一括実行する関数
        void RunDestructors()
        {
            // 生成と逆順に破棄するのが安全
            for (auto it = _destructors.rbegin(); it != _destructors.rend(); ++it) {
                (*it)();
            }
            _destructors.clear();
        }

        //ページが足りなくなったときに呼ばれます。
        void AllocateNewPage(size_t requiredSize)
        {
            size_t sizeToAlloc = (std::max)(_pageSize, requiredSize);

            _pageIndex++;
            // ページが足りなければ新規確保、あれば再利用
            if (_pageIndex < _pages.size()) {
                 //すでに前のフレームで作った2ページ目、3ページ目があるなら、それを再利用！
                _currentPage = _pages[_pageIndex];
            }
            else {
                // 本当に足りない時だけ、OSからメモリをもらう (::operator new)
                uint8_t *ptr = static_cast<uint8_t *>(::operator new(sizeToAlloc));
                _pages.push_back(ptr);
                _currentPage = ptr;

                // OSから新たに「ページ」という物理メモリを確保した瞬間を通知
                TracyAlloc(ptr, sizeToAlloc);


            }
            _offset = 0;  // 新しいページなので、書き込み位置は0から
        }

        std::vector<uint8_t *> _pages;       // 落書き帳のページ（メモリの塊）のリスト
        uint8_t               *_currentPage; // 今書いているページ
        size_t                 _offset;      // 今書いているページの「何行目」にいるか
        size_t                 _pageIndex;   // 今何ページ目を使っているか
        size_t                 _pageSize;    // 1ページの大きさ（バイト数）

        std::vector<std::function<void()>> _destructors; // 後片付けリスト
    };
} // namespace CCL::ECS::Core