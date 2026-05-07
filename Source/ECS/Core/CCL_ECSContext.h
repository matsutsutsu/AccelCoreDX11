#pragma once
#include "ECS/Core/CCL_World.h"
#include "ECS/System/CCL_SystemManager.h"
#include <memory>

namespace CCL::ECS {

    // 1つの独立したECS環境（データとロジックのセット）を管理するクラス
    class ECSContext {
      public:
        ECSContext()
        {
            _world         = std::make_unique<Core::World>();
            _systemManager = std::make_unique<SystemManager>();

            // SystemManagerに「お前が処理すべきデータはこれだ」と教える
            _systemManager->SetWorld(_world.get());
        }

        ~ECSContext() = default;

        // ---------------------------------------------------------
        // チームメンバー（利用者）が呼び出す安全な更新フロー
        // ---------------------------------------------------------

        // 物理・ロジックの更新（FixedUpdate等で呼ぶ）
        void UpdateLogic(float dt)
        {
            //  FixedUpdate全体を計測
            ZoneScopedN("World::FixedUpdate");


            // 1. 溜まっていた構造変更（Entity生成/削除）を適用し、データを確定させる
            _world->ScrutinyAndApply();

            // 2. 構造変更があったフレームのみターゲットを再解決する
            if (_world->IsStructureDirty())
            {
                _systemManager->ResolveTargetChunks(&_world->GetChunkManager());
                _world->ResetStructureDirty(); // フラグを下ろす
            }

            // 3. ロジックを回す
            _systemManager->UpdateLogic(dt);
        }

        // 描画系の更新（RenderUpdate等で呼ぶ）
        void UpdateRender(float dt) 
        {
            // RenderUpdate全体を計測         
            ZoneScopedN("World::RenderUpdate");

        

            _systemManager->UpdateRender(dt); 
        }

        // ポーズ画面用など、ロジックは止めて構造変更と描画だけを行う特殊更新
        void UpdatePaused(float dt)
        {
            _world->ScrutinyAndApply();
            _systemManager->ResolveTargetChunks(&_world->GetChunkManager());
            _systemManager->UpdateRender(dt);
        }


        // ---------------------------------------------------------
        // アクセッサ
        // ---------------------------------------------------------
        Core::World   *GetWorld() const { return _world.get(); }
        SystemManager *GetSystemManager() const { return _systemManager.get(); }

      private:
        std::unique_ptr<Core::World>   _world;
        std::unique_ptr<SystemManager> _systemManager;
    };

} // namespace CCL::ECS