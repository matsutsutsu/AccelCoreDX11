#include "TransformUpdateSystem.h"
#include "ECS/Core/CCL_World.h"

#include "Engine/GamePlay/Transform/PendingParentComponent.h"
#include "BoneAttachmentComponent.h"      
// ★ Animatorではなく、Modelから計算済みのボーン行列を取得する
#include "Engine/GamePlay/Graphics/Core/ModelComponent.h" 
#include "Engine/Graphics/Resource/Model.h"

// ★ AnimatorComponent をインクルード（アニメーション再生中かの検知用）
#include "Engine/GamePlay/Animation/AnimatorComponent.h"

// 各システムの .cpp ファイルの上部に追加
#include "ECS/System/CCL_SystemRegistry.h"
#include "Game/Core/SystemPriority.h"

#include "Engine/Platform/Logger.h"

using namespace CCL::ECS;
using namespace DirectX;

TransformUpdateSystem::TransformUpdateSystem() : IfSystem("TransformUpdateSystem") {}


// ----------------------------------------------------------------------------
// ★ DODスタイル: 外部の静的関数として定義（クラスメンバにしない）
// これにより、ヘッダー(.h)を汚さず、コンパイルも高速になり、インライン化も効きやすくなる。
// ----------------------------------------------------------------------------
static void UpdateTransformRecursiveHelper(Core::World* world, EntityID entity, const XMMATRIX& parentWorldMatrix, bool isParentUpdated) {
    auto* trans = world->GetComponent<TransformComponent>(entity);
    if (!trans) return;

    bool isUpdated = isParentUpdated || trans->isDirty;
    XMMATRIX finalParentMatrix = parentWorldMatrix;

    // --- 1. ボーンアタッチメントの評価 ---
    if (auto* boneAttach = world->GetComponent<BoneAttachmentComponent>(entity)) {
        if (auto* modelComp = world->GetComponent<ModelComponent>(trans->parentID)) {
            Model* model = modelComp->GetModel();
            if (model) {
                if (boneAttach->cachedBoneIndex == -1 && !boneAttach->boneName.empty()) { // ★ .empty() でチェック
                    // std::string をそのまま渡す (もし GetNodeIndex が const char* を要求するなら .c_str() をつける)
                    boneAttach->cachedBoneIndex = model->GetNodeIndex(boneAttach->boneName.c_str());
                }
                if (boneAttach->cachedBoneIndex != -1) {
                    const auto& nodes = model->GetNodes();
                    XMMATRIX boneMatrix = XMLoadFloat4x4(
                        &nodes[boneAttach->cachedBoneIndex].globalTransform);
                    finalParentMatrix = boneMatrix * parentWorldMatrix;
                    isUpdated = true;

                    trans->isTeleported = true;
                }
            }
        }
    }

    // =======================================================================
    //  ここが丸ごと抜けていました！（2. 自分のワールド行列計算）
    // =======================================================================
    if (isUpdated && !trans->isStatic) {
        // 親の行列(finalParentMatrix)を渡し、自分のローカル座標と掛け合わせて worldMatrix を上書きする
        trans->UpdateMatrix(finalParentMatrix);
        trans->isDirty = false;
    }

    // --- 4. 子エンティティへ再帰伝播 ---
    EntityID childID = trans->firstChildID;
    while (childID != 0) {
        auto* childTrans = world->GetComponent<TransformComponent>(childID);
        if (childTrans) {
            EntityID nextChild = childTrans->nextSiblingID;
            XMMATRIX transWorldMat = XMLoadFloat4x4(&trans->worldMatrix);
            UpdateTransformRecursiveHelper(world, childID, transWorldMat, isUpdated);
            childID = nextChild;
        }
        else {
            break;
        }
    }
}


void TransformUpdateSystem::Update(float dt)
{
    // -----------------------------------------------------------------------
    // 1. 構造変更フェーズ (メインスレッド必須)
    // -----------------------------------------------------------------------

    // 生成時に予約された親子関係の適用
    ProcessPendingParents();

    // 壊れたリンク（親がいるのに親から認識されていない子）の修復
    RepairBrokenLinks();

    // -----------------------------------------------------------------------
    // 2. 行列更新フェーズ (並列処理可能)
    // -----------------------------------------------------------------------
    ForEachWithID([this](EntityID id, TransformComponent& trans) {
        if (trans.parentID != 0) return; // ルートのみ

        bool isUpdated = trans.isDirty;

        // ★【重要】ルートエンティティが立ち止まっていても、アニメーションが再生中なら
        // 骨が動いているため、強制的に isUpdated = true にして子供（剣）に伝播させなければならない
        if (auto* animator = _world->GetComponent<AnimatorComponent>(id)) {
            if (animator->currentSequence != nullptr && animator->playbackSpeed > 0.0f) {
                isUpdated = true;
            }
        }

        // ルートのワールド行列計算
        if (isUpdated && !trans.isStatic) {
            XMMATRIX localMat = XMMatrixAffineTransformation(
                XMLoadFloat3(&trans.scale),
                XMVectorZero(),
                XMLoadFloat4(&trans.rotation),
                XMLoadFloat3(&trans.position)
            );
            XMStoreFloat4x4(&trans.worldMatrix, localMat);
            trans.isDirty = false;
        }

        // ルートの骨格行列をベイク
        if (isUpdated) {
            if (auto* modelComp = _world->GetComponent<ModelComponent>(id)) {
                if (Model* model = modelComp->GetModel()) {
                    model->UpdateTransform(trans.worldMatrix);
                }
            }
        }

        // ルートから子へ伝播
        EntityID childID = trans.firstChildID;
        while (childID != 0) {
            auto* childTrans = _world->GetComponent<TransformComponent>(childID);
            if (childTrans) {
                EntityID nextChild = childTrans->nextSiblingID;
                XMMATRIX rootWorldMat = XMLoadFloat4x4(&trans.worldMatrix);
                UpdateTransformRecursiveHelper(_world, childID, rootWorldMat, isUpdated);
                childID = nextChild;
            }
            else {
                break;
            }
        }
        });
    
}

// ★追加: 実装
void TransformUpdateSystem::ProcessPendingParents()
{
    // PendingParentComponent を持っているエンティティを探す
    // (IfSystemのフィルタとは別に、Worldから直接検索するか、専用のViewを使う)
    // ここでは簡易的にWorld経由で全走査またはViewを使用
    // ※Systemのテンプレート引数にPendingParentComponentを追加していないので、
    //   _world->View<PendingParentComponent>() を使います。

    // 注意: ループ中にコンポーネント削除(RemoveComponent)を行うため、
    // イテレータ無効化を避けるために一度リストに集めるのが安全です。
    std::vector<EntityID> pendingEntities;
    for (auto id : _world->View<PendingParentComponent>()) {
        pendingEntities.push_back(id);
    }

    for (auto id : pendingEntities) {
        auto *pending = _world->GetComponent<PendingParentComponent>(id);
        // ★ 究極の修正: pending->parentID != InvalidEntityID という「無効化チェック」を削除！
         // 予約票が付いている時点で、必ず何らかの処理を行う。
        if (pending) {

            // ================================================================
            // parentID が 0 または InvalidEntityID の場合は「親の解除 (Unparent)」
            // ================================================================
            if (pending->parentID == 0 || pending->parentID == InvalidEntityID) {
                // ルートへの移動（親の解除）
                if (_world->IsEntityValid(id)) {
                    DetachChild(*_world, id);

                    // 階層が変わったのでワールド行列の再計算を要求する
                    if (auto* trans = _world->GetComponent<TransformComponent>(id)) {
                        trans->isDirty = true;
                    }
                }
            }
            else {
                // 通常の親子付け (親も子も実在するエンティティの場合)
                if (_world->IsEntityValid(pending->parentID) && _world->IsEntityValid(id)) {
                    SetParent(*_world, id, pending->parentID);

                    // 階層が変わったのでワールド行列の再計算を要求する
                    if (auto* trans = _world->GetComponent<TransformComponent>(id)) {
                        trans->isDirty = true;
                    }
                }
            }
        }

        // 用が済んだら予約票を捨てる
        _world->RequestRemoveComponent<PendingParentComponent>(id);
    }
}

// リンク切れ修復の実装
void TransformUpdateSystem::RepairBrokenLinks()
{
    // 構造を変更する可能性があるのでシングルスレッド(ForEach)で実行
    ForEachWithID([&](EntityID id, TransformComponent &trans) {
        // 親が設定されている場合のみチェック
        if (trans.parentID != 0) {
            auto *parentTrans = _world->GetComponent<TransformComponent>(trans.parentID);

            // 親が存在するのに、親から認識されていない（リンク切れ）場合を判定
            bool isOrphan = false;
            if (parentTrans) {
                // 親にとっての最初の子が自分ならOK
                bool isFirstChild = (parentTrans->firstChildID == id);
                // 兄弟がいるならリストにいるのでOK
                bool hasSibling = (trans.nextSiblingID != 0 || trans.prevSiblingID != 0);

                if (!isFirstChild && !hasSibling) {
                    isOrphan = true;
                }
            }

            if (isOrphan) {
                // 自動修復: 正しい親子関係を結び直す (書き込み発生)
                SetParent(*_world, id, trans.parentID);
            }
        }
    });
}


// ---------------------------------------------------------
// ヘルパー関数: 親子関係の構築
// ---------------------------------------------------------

void TransformUpdateSystem::SetParent(Core::World &world, EntityID childID, EntityID newParentID)
{
    // 自己参照チェック
    if (childID == newParentID) return;
    if (!world.IsEntityValid(childID)) return;

    // 1. まず現在の親から切り離す
    DetachChild(world, childID);

    // 親を「無し(0)」にするだけならここで終了
    if (newParentID == 0) return;
    if (!world.IsEntityValid(newParentID)) return;

    auto *childTrans  = world.GetComponent<TransformComponent>(childID);
    auto *parentTrans = world.GetComponent<TransformComponent>(newParentID);

    if (!childTrans || !parentTrans) return;

    // 2. 新しい親を設定
    childTrans->parentID = newParentID;

    // 3. 親の子リストの「先頭」に自分を挿入する (Insert Head)
    //    双方向リストへの挿入処理: O(1)

    EntityID oldFirstChildID = parentTrans->firstChildID;

    // 自分の次 = 親の元々の先頭
    childTrans->nextSiblingID = oldFirstChildID;
    // 自分の前 = 無し (先頭になるため)
    childTrans->prevSiblingID = 0;

    // もし親に元々子供がいたら、その子供の「前」を自分にする
    if (oldFirstChildID != 0) {
        auto *oldFirstTrans = world.GetComponent<TransformComponent>(oldFirstChildID);
        if (oldFirstTrans) {
            oldFirstTrans->prevSiblingID = childID;
        }
    }

    // 親の先頭ポインタを自分に更新
    parentTrans->firstChildID = childID;
}

void TransformUpdateSystem::DetachChild(Core::World &world, EntityID childID)
{
    auto *childTrans = world.GetComponent<TransformComponent>(childID);
    if (!childTrans || childTrans->parentID == 0) return;

    EntityID parentID    = childTrans->parentID;
    auto    *parentTrans = world.GetComponent<TransformComponent>(parentID);

    // 1. 親から見たリンクの修正
    if (parentTrans) {
        // 親の「最初の子」が自分なら、自分の「次」を新しい「最初の子」にする
        if (parentTrans->firstChildID == childID) {
            parentTrans->firstChildID = childTrans->nextSiblingID;
        }
    }

    // 2. 兄弟間のリンク修正
    // 「前の兄弟」がいる場合、その「次」を自分の「次」につなぐ
    if (childTrans->prevSiblingID != 0) {
        auto *prevTrans = world.GetComponent<TransformComponent>(childTrans->prevSiblingID);
        if (prevTrans) {
            prevTrans->nextSiblingID = childTrans->nextSiblingID;
        }
    }

    // 「次の兄弟」がいる場合、その「前」を自分の「前」につなぐ
    if (childTrans->nextSiblingID != 0) {
        auto *nextTrans = world.GetComponent<TransformComponent>(childTrans->nextSiblingID);
        if (nextTrans) {
            nextTrans->prevSiblingID = childTrans->prevSiblingID;
        }
    }

    // 3. 自分のリンク情報をクリア
    childTrans->parentID      = 0;
    childTrans->prevSiblingID = 0;
    childTrans->nextSiblingID = 0;
}

// 階層構造を考慮した削除の実装
void TransformUpdateSystem::DestroyEntityWithHierarchy(
    CCL::ECS::Core::World &world, CCL::ECS::EntityID entity, bool destroyChildren)
{
    if (!world.IsEntityValid(entity)) return;

    auto *trans = world.GetComponent<TransformComponent>(entity);
    if (!trans) {
        // Transformがないなら普通に消すだけ
        world.RequestDestroyEntity(entity);
        return;
    }

    // 1. 子供の処理（再帰的削除）
    if (destroyChildren) {
        CCL::ECS::EntityID childID = trans->firstChildID;
        while (childID != 0) {
            auto *childTrans = world.GetComponent<TransformComponent>(childID);
            if (!childTrans) break;

            CCL::ECS::EntityID nextChild = childTrans->nextSiblingID;

            // 子供自身もこの関数を呼んで、そのまた子供も消す (再帰)
            DestroyEntityWithHierarchy(world, childID, true);

            childID = nextChild;
        }
    }
    else {
        // 子供を道連れにしない場合、子供を「親なし」にして逃がす
        CCL::ECS::EntityID childID = trans->firstChildID;
        while (childID != 0) {
            auto *childTrans = world.GetComponent<TransformComponent>(childID);
            if (!childTrans) break;

            CCL::ECS::EntityID nextChild = childTrans->nextSiblingID;

            // 親から切り離す（これでリスト構造が安全に更新される）
            DetachChild(world, childID);

            childID = nextChild;
        }
    }

    // 2. 親・兄弟からの切り離し (自分をリストから安全に抜く)
    // これを忘れると、前の兄弟が「死んだ自分」を nextSibling として参照し続けてしまう
    DetachChild(world, entity);

    // 3. 最後に本体の削除予約
    world.RequestDestroyEntity(entity);
}

// ★修正: 削除予約ではなく、タグの付与を行う
void TransformUpdateSystem::MarkForDestruction(
    CCL::ECS::Core::World &world, CCL::ECS::EntityID entity, bool destroyChildren)
{
    if (!world.IsEntityValid(entity)) return;

    // 既にタグが付いているなら何もしない（無限ループ防止）
    if (world.HasComponent<Tag::DestroyTag>(entity)) return;

    // 1. 自分に死の宣告 (タグ付与)
    world.AddComponent<Tag::DestroyTag>(entity);

    auto *trans = world.GetComponent<TransformComponent>(entity);
    if (!trans) return;

    // 2. 子供への伝播
    if (destroyChildren) {
        CCL::ECS::EntityID childID = trans->firstChildID;
        while (childID != 0) {
            auto *childTrans = world.GetComponent<TransformComponent>(childID);
            if (!childTrans) break;

            CCL::ECS::EntityID nextChild = childTrans->nextSiblingID;

            // 再帰的にタグをつける
            MarkForDestruction(world, childID, true);

            childID = nextChild;
        }
    }
    else {
        // 子供を道連れにしない場合、子供を「親なし」にして逃がす
        // 注意: HierarchyCleanupSystemでDetachされるので、ここでは何もしなくて良いが、
        // 明示的に親子関係を切りたい場合は DetachChild を呼ぶ。
        // タグシステムの場合は「CleanupSystemが実行されるまで構造は維持される」のが正しい姿なので、
        // ここでは親子のリンクはいじらないのが正解。
        // しかし、親だけ死んで子が残る場合、CleanupSystemの実行時に「親が消える」ので
        // 自動的に子は孤立する。

        // ただし、親がタグ付きで、子がタグ無しの場合の挙動として
        // 「親が消える前に子をDetach」しておく必要がある。
        // それは HierarchyCleanupSystem::Update 内の DetachChild で行われるので、
        // ここでは何もしなくてOK！
    }
}


// 自動登録。SystemGroups.h に書く必要はなくなります
REGISTER_LOGIC_SYSTEM(TransformUpdateSystem, Priority::LogicStage::L02_PostUpdate);