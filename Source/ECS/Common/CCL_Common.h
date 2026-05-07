#pragma once
// CCL_Common.h
// 共通型 / ユーティリティ / Archetype 定義
// - このヘッダは Core 以下の実装で参照される最低限の定義を含みます.

#include <vector>
#include <algorithm>
#include <cstdint>
#include <cstddef>           
#include <mutex>
#include <type_traits>
#include <cassert>
#include <utility>     // std::move に必要
#include <new>         // placement new に必要


namespace CCL::ECS
{
    // ------------------------------
    // 汎用定義（ID型）
    // ------------------------------
	// using 別名　= 既存の型;　でunsigned long longという型に別名をつけている
	// つまり、EntityIDはunsigned long long型として扱われる
    // unsigned long long id;  // これ何の数字？
    // EntityID player;        // エンティティ番号だな！
    // コード読むときの理解力が段違いになる
	using EntityID = unsigned long long;        // 個々のエンティティを識別するID
	using TypeID = unsigned short;              // 個々のコンポーネント型を識別するID
	using ComponentID = unsigned long long;     // 個々のコンポーネントインスタンスを識別するID
	using SystemID = unsigned short;            // 個々のシステムを識別するID
    

    // アウトプット
    // EntityIDは純粋にEntityを識別するときに渡されるID
	// TypeIDはコンポーネントの型を識別するときに使われるID  Position(1), Velocity(2) みたいな
	// ComponentIDはコンポーネントの実体を識別するときに使われるID 
	// Typeでコンポーネントの種類、ComponentIDでその実体を識別するイメージ

	// SystemIDはシステムを識別するときに使われるID
    // 複数のSystemを識別して順番に更新したりするために使われる
    



    // ------------------------------
    // 定数定義 (constexpr)
    // ------------------------------
    // マクロの代わりに型付き定数を使用します。
    // unsigned 型を -1 にキャストすると、その型の最大値になる
    // unsignedの場合はビットが全部１の最大値になる
    // ECSではオブジェクトの実体を消さずにIDを無効化することで存在しないと判定する
    // ECSではID番号でEntityを管理するからこそのやり方

    // static を付けることで内部リンケージ（このファイル内でのみ有効）にし、
    // 多重定義エラーを防ぎつつ定数を定義します。
    constexpr EntityID    InvalidEntityID = static_cast<EntityID>(-1);
    constexpr TypeID      InvalidTypeID = static_cast<TypeID>(-1);
    constexpr ComponentID InvalidComponentID = static_cast<ComponentID>(-1);
    constexpr SystemID    InvalidSystemID = static_cast<SystemID>(-1);

    // インデックス無効値 (size_t)
    constexpr size_t      InvalidIndex = static_cast<size_t>(-1);


    // ------------------------------
    // ID_Emitter: 型ID / EntityID 等の発行
    // ------------------------------
	// 新しいIDを要求されるたびにインクリメントして返す
	// static 変数なので、プロセス全体で一意になる
    // 
	// イメージ：社員番号発行機みたいなもの新しい社員（Entity）を
    // 雇うたびに新しい社員番号（EntityID）を発行する
    class ID_Emitter
    {
    public:
        // inline static メンバ変数を廃止し、関数内 static に変更して分裂を強制阻止する
        static EntityID EmitEntityID()
        {
            static EntityID s_nextEntityID = 0;
            return s_nextEntityID++;
        }
        static TypeID EmitTypeID()
        {
            static TypeID s_nextTypeID = 0;
            return s_nextTypeID++;
        }
        static ComponentID EmitComponentID()
        {
            static ComponentID s_nextComponentID = 0;
            return s_nextComponentID++;
        }
        static SystemID EmitSystemID()
        {
            static SystemID s_nextSystemID = 0;
            return s_nextSystemID++;
        }
    };


    // 便利に分解・合成するためのヘルパー
    struct EntityHandle {
        static constexpr EntityID INDEX_MASK = 0xFFFFFFFFULL;
        static constexpr EntityID GEN_SHIFT = 32;

        static uint32_t GetIndex(EntityID id) { return static_cast<uint32_t>(id & INDEX_MASK); }
        static uint32_t GetGeneration(EntityID id) { return static_cast<uint32_t>(id >> GEN_SHIFT); }
        static EntityID Combine(uint32_t index, uint32_t gen) {
            return (static_cast<EntityID>(gen) << GEN_SHIFT) | static_cast<EntityID>(index);
        }
    };



    // ------------------------------
    // Destructor ユーティリティ（placement newで作ったオブジェクトの破棄を扱う）
    // ------------------------------
    class Destructor
    {
    public:
        Destructor() = default;
        ~Destructor() = default;

        // このDestructorはTの破棄を担当すると登録する
        // Destructorはtemplate<classT>のCreateでそのクラスの
        // デストラクタ（CallDestructorFromPtr）のやり方をdestructFuncに
        // 保存して破棄するときにそれを呼び出してデストラクタを呼び出すということです
		// 型情報が分からなくなる前に、掃除の仕方を登録しておくイメージ
        template <class T>
        void Create()
        {
            destructFunc = &CallDestructorFromPtr<T>;
        }

        // 登録済みの破棄関数を呼び出す
        void Destruct(void* dataPtr) const
        {
            if (destructFunc) destructFunc(dataPtr);
        }

        // 本物のデストラクタを直接たたく
        template <class T>
        static void CallDestructorFromPtr(void* dataPtr)
        {
			// reinterpret_cast :  void* →　T* に戻す（型復元）
			// ここはT型のデータと思い出してから本物のデストラクタを呼び出す
            if (dataPtr) reinterpret_cast<T*>(dataPtr)->~T();
        }

    private:
        // 関数ポインタ
        //・ void（戻り値の型）
        //・(*destructFunc)（これはポインタ変数の名前）
        //・(void*)（関数の引数リスト）
        // destructFuncはvoid(void*)型の関数を指すポインタ
        void (*destructFunc)(void*) = nullptr;
    };

    // 型情報が消えた生メモリ（void）に対して、正しいコピーコンストラクタを
    // 呼び出すための装置
    // 用途：ChunkからChunkへEntityを移す時に必須
    // Entityが別のArcheTypeに移動するとき・Chunkが満杯→新Chunkへ詰め替え
    // そのどちらも型の並びは変わるけど、データは保持したまま移動したい
    // そのためにデータを正しくコピーするために
    class Copier
    {
    public:
        template <class T>
        void Create()
        {
            copyFunc = &CallCopyFromPtr<T>;
        }
        // src（既存）、コピー　→　dst（新しい場所）
        void Copy(void* dst, const void* src) const
        {
            if (copyFunc) copyFunc(dst, src);
        }
    private:
        void (*copyFunc)(void*, const void*) = nullptr;

        // srcにあるTの完全な複製をdstの生メモリに正しく構築
        template <class T>
        static void CallCopyFromPtr(void* dst, const void* src)
        {
            // new(dst)         :  placement new
            // T(*src)          :  コピーコンストラクタの呼び出し
            // reinterpret_cast :  void* →　T* に戻す（型復元）
            new (dst) T(*reinterpret_cast<const T*>(src));
        }
    };


    // ------------------------------
    // TypeInfo / TypeData
    // ------------------------------
	// Entity / Component / System などの型Tに関する情報を提供するユーティリティ
	// TypeInfo: 型Tに関する情報を提供
	// 型そのものを「IDという数字」として扱うための仕組み
	// 型を入れたらそれにあった数字が返ってくる
	// 例： TypeInfo<Position>::ID() → 1　という感じ
    template <class T>
    class TypeInfo
    {
    public:
        // ここが究極の解決策（C++17の inline static 変数）
        // リンカは異なる .cpp ファイルに散らばった TypeInfo<Transform>::s_id を見つけると、
        // C++17の言語仕様に基づき、絶対に1つのメモリ領域に統合（マージ）します。
        // その結果、EmitTypeID() はプログラム全体で "型ごとにたった1回" しか呼ばれず、
        // 100%安全なユニークID（数字）が生成されます。
        inline static const TypeID s_id = ID_Emitter::EmitTypeID();


        static TypeID ID()
        {
            return s_id; // 単なる変数の読み取りなので、超高速
        }
		// Position->常に1, Velocity->常に2 みたいな感じで型ごとにユニークな数字を返す
		// 同じ型には必ず同じ数字が返る

		// ArchetypeでChunkに構造体を並べて配置するために型サイズが必要
        // 例：先頭アドレス + (entityIndex * コンポーネントサイズ)
		// この計算をするために各コンポーネントのサイズを知る必要がある
        static size_t Size()
        {
            return sizeof(T);
        }

        // 追加: 型名を登録・取得する
        static const char* Name() {
            static std::string name = typeid(T).name(); // 実際は "struct Transform" 等が返る
            return name.c_str();
        }
    };


	// TypeData: 型Tに関する実体情報を格納
    // 
	// ArchetypeでChunkに構造体を並べて配置するため
    // ・別Archetypeへ移す → コピー処理
    // ・Entity削除 → デストラクタ呼び出し
    // ・メモリ効率化 → 型サイズが必要
    // ・型一致判定 → TypeID比較が最速
    // 
	// これらを全て一手に引き受ける司令塔的なクラス
    // ECSが各コンポーネント型を「メモリ操作できるデータ」として扱うための情報
    class TypeData
    {
    public:
        TypeData() = default;
        ~TypeData() = default;
        TypeData(const TypeData&) = default;              // 削除されたコピーコンストラクタを強制的に生成
        TypeData& operator=(const TypeData&) = default;   // 削除されたコピー代入演算子を強制的に生成
        TypeData(TypeData&&) = default;
        TypeData& operator=(TypeData&&) = default;

		// コンポーネント型からCreateで各種情報を初期化        
        template <class T>
        void Create()
        {
            id = TypeInfo<T>::ID();       // Tの種類番号（絶対に衝突しない）
            typeSize = TypeInfo<T>::Size(); // Tのサイズ
            destructor.template Create<T>(); // templateキーワードが必要な場合があります
            name = TypeInfo<T>::Name();     // XMTLで読み取り用に必要

            // デフォルトコンストラクタ（引数なしの new T()）
            defaultConstructor = [](void* dst) {
                new (dst) T();
                };

            // placement new 用のコンストラクタ（未初期化メモリ用）
            // ★修正: コピー可能な場合のみ生成する
            // このif文はコンパイル時に「この型Tはコピー可能か？」を判定する
            if constexpr (std::is_copy_constructible_v<T>) {
                constructor = [](void *dst, const void *src) {
                    new (dst) T(*reinterpret_cast<const T *>(src));
                };
            }
            else {
                // コピー禁止の型でこれが呼ばれたらエラーにする（または何もしない）
                constructor = [](void *, const void *) {
                    assert(false && "Copy constructor called for non-copyable type!");
                };
            }

            // 代入演算子用 (コピー代入)
            // ★修正: コピー代入可能な場合のみ生成する
            if constexpr (std::is_copy_assignable_v<T>) {
                assigner = [](void *dst, const void *src) {
                    T       *dstObj = reinterpret_cast<T *>(dst);
                    const T *srcObj = reinterpret_cast<const T *>(src);
                    *dstObj         = *srcObj;
                };
            }
            else {
                assigner = [](void *, const void *) {
                    assert(false && "Copy assignment called for non-copyable type!");
                };
            }

            // move用（CompactHoles や MoveEntityBetweenChunks で使用）
            mover = [](void *dst, void *src) {
                // 代入(=)ではなく、placement new + std::move で「移動構築」する
                // dstは未初期化メモリなので、operator= を呼ぶのは危険です。
                new (dst) T(std::move(*reinterpret_cast<T *>(src)));
            };
        }
        
		// id: Archetype内でコンポーネント型を識別するためのID   ：　id == id　で最速の整数比較ができる
		// typeSize:    コンポーネントのメモリサイズ      ：　メモリ配置やコピーに使用
		// destructor: 型コンポーネントのデストラクタの手段保存　　   ：　destructor.destory(ptr)で呼び出し可能
		// copier: コンポーネントのコンストラクタの手段保存         ：　copier.copy(dst, src)で呼び出し可能
        TypeID id = InvalidTypeID;
        size_t typeSize = 0;
        Destructor destructor;
        const char *name = nullptr;
        //Copier copier; 

           // 3つの異なる関数ポインタ
        using ConstructFunc = void(*)(void*, const void*);  // 構築用
        using AssignFunc = void(*)(void*, const void*);     // 代入用
        using MoveFunc = void(*)(void*, void*);             // move用（非const）
        using DefaultConstructFunc = void(*)(void*);        // デフォルト構築用の関数型定義

        ConstructFunc constructor = nullptr;  // placement new用
        AssignFunc assigner = nullptr;        // 代入用
        MoveFunc mover = nullptr;             // move用
        DefaultConstructFunc defaultConstructor = nullptr;  // デフォルトコンストラクタ


		// 比較演算子（Archetype内でソートや一致判定に使用）
        // 同じコンポーネント集合の際に順番が違ってもソートして統一するために必要
        // Entity A → [ Velocity(2), Position(1) ]
        // ソート後 → [ Position(1), Velocity(2) ]
        // Entity B → [ Position(1), Velocity(2) ]
        // ソート後 → [ Position(1), Velocity(2) ]
        // < はソートのために == はArchetype一致判定のために必要
        bool operator<(const TypeData& r) const { return id < r.id; }
        bool operator==(const TypeData& r) const { return id == r.id; }

    };


    // ------------------------------
    // Archetype（TypeData のソートされた集合）
    // ------------------------------
    // コンポーネント型の組み合わせの情報、決してデータがある場所ではない
    // 中の関数では Add/Sub　構成の変更
    class Archetype
    {
    public:
        Archetype() = default;
        ~Archetype() = default;

        // コピーコンストラクタと代入演算子をデフォルト化
        // 包含している std::vector<TypeData> _data のコピーを正しく行わせる
        Archetype(const Archetype& other) = default;             
        Archetype& operator=(const Archetype& other) = default;  

        // TypeDataをソートしながら挿入
        // | Before                   | Add<Velocity>() → TypeIDソート | After                                 |
        // | ------------------------ | -------------------------- -    | ------------------------------------  |
        // |[Position(1), Render(5)]  | Velocity(2)追加                 | [Position(1), Velocity(2), Render(5)] |
        // 常に昇順に並ぶから一致判定が高速
        // 
        // 例：
        // Before:       [ Position(1), Render(5) ]
        // Add<Velocity> TypeID = 2
        // 比較  1 < 2 → next
        //       5 >= 2 → ここに挿入！
        // After:        [ Position(1), Velocity(2), Render(5) ]
        template <class T>
        size_t Add()
        {
            TypeData type;
            type.Create<T>();
    
            return Add(type);
  
        }

        size_t Add(const TypeData& typeData)
        {
            // 挿入位置を二分探索で見つける
            auto it = std::lower_bound(
                _data.begin(),
                _data.end(),
                typeData.id,
                [](const TypeData& td, TypeID targetId) {
                    return td.id < targetId;
                }
            );

            // 重複チェック：既にあるなら挿入せず、その位置を返す
            if (it != _data.end() && it->id == typeData.id) {
                return std::distance(_data.begin(), it);
            }

            // 適切な位置に挿入
            size_t index = std::distance(_data.begin(), it);
            _data.insert(it, typeData);
            return index;
        }

        // 指定タイプのIDを削除
        // Componentを削除したときのArchetype遷移にお必要
        template <class T>
        void Sub()
        {
            Sub(TypeInfo<T>::ID());
        }

        // 指定タイプのIDを削除 (TypeID を直接指定)
        void Sub(TypeID id)
        {
            // std::lower_bound を使用して O(log N) で要素を検索
            auto it = std::lower_bound(
                _data.begin(),
                _data.end(),
                id,
                [](const TypeData& td, TypeID targetId) {
                    return td.id < targetId;
                }
            );

            // 見つかったかチェック
            if (it != _data.end() && it->id == id)
            {
                _data.erase(it);
            }
        }

    public:
        // TypeID から TypeData が格納されているインデックスを取得するヘルパー関数 (O(log N))
        size_t GetComponentIndex(TypeID id) const
        {
            // std::lower_bound を使用
            auto it = std::lower_bound(
                _data.begin(),
                _data.end(),
                id,
                [](const TypeData& td, TypeID targetId) {
                    return td.id < targetId;
                }
            );

            if (it != _data.end() && it->id == id)
            {
                return std::distance(_data.begin(), it);
            }
            return InvalidIndex; // 見つからなかった場合
        }

        // コンポーネントが同じ構成かを見ている (TypeData がソートされているため高速)
        bool operator==(const Archetype& r) const
        {
            if (_data.size() != r._data.size()) return false;
            return std::equal(_data.begin(), _data.end(), r._data.begin());
        }

        // 内部データへの読み取り専用アクセス（ChunkManagerなどで使用）
        const TypeData& at(size_t i) const { return _data.at(i); }
        const TypeData& operator[](size_t i) const { return _data[i]; }

        // サイズ情報
        size_t size() const { return _data.size(); }
        bool empty() const { return _data.empty(); }

        // イテレータの提供 (範囲for文対応)
        std::vector<TypeData>::const_iterator begin() const { return _data.cbegin(); }
        std::vector<TypeData>::const_iterator end() const { return _data.cend(); }

    private:
        // 継承ではなく、プライベートメンバとして包含
        std::vector<TypeData> _data;
    };



    // ArchetypeHelper: constexpr生成（簡易版）
    class ArchetypeHelper
    {
    public:
        // 複数コンポーネントからArchetypeを自動生成する
        // 例：auto a = ArchetypeHelper::Generate<Position, Velocity, Render>();
        // Add(Position)
        // Add(Velocity)
        // Add(Render)
        // → ソート → TypeID昇順に整列
        // [Position(1), Velocity(2), Render(5)]
        // これがないと一つ一つAddを連打してソートしないといけない
        template <class ...Types>
        static Archetype Generate()
        {
            Archetype a;
            (a.Add<Types>(), ...);
            //std::stable_sort(a.begin(), a.end());
            return a;
        }

        // LのArcheTypeが右をArcheTypeを包含しているかを判定する
        // l = [Position, Velocity, Render]
        // r = [Position, Velocity]
        // この場合はtrueを返す、lとrが逆の場合はfalseになる
        static bool LhasR(const Archetype& l, const Archetype& r)
        {
            // R（フィルタ）が空なら、無条件で全チャンクを対象にする
            if (r.size() == 0) return true;
            // L（チャンク）の方が要素が少なければ、絶対に包含できない
            if (l.size() < r.size()) return false;

            //  O(N+M) の高速な包含判定
            // 両者がソート済みであることを前提に、ポインタを同時に進めて判定します
            return std::includes(
                l.begin(), l.end(), r.begin(), r.end(), [](const TypeData &a, const TypeData &b) {
                    return a.id < b.id; // TypeIDの比較
                });
        }
    };

    // LhasRですが、STLを使わないバージョン　こっちが早いらしい？
    //// ★ STLを捨てて、インデックス（2本の指）で直接走査する O(N+M)
    //size_t i = 0; // l (Chunk) を指す指
    //size_t j = 0; // r (Filter) を指す指

    //while (i < l.size() && j < r.size()) {
    //    if (l[i].id == r[j].id) {
    //        // 見つかった！両方の指を次に進める
    //        i++;
    //        j++;
    //    }
    //    else if (l[i].id < r[j].id) {
    //        // Chunk側のIDが小さいので、Chunk側の指だけ進める
    //        i++;
    //    }
    //    else {
    //        // Filter側のIDの方が小さくなってしまった。
    //        // （＝Chunk側には探しているIDが存在しない）
    //        return false;
    //    }
    //}

    //// 最終的に Filter(r) の最後まで指が進みきっていれば、全て包含されていた証拠
    //return j == r.size();

  
   

} // namespace CCL::ECS
