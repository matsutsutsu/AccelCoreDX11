#pragma once
#include "CCL_SystemManager.h"
#include <functional>
#include <vector>

namespace CCL::ECS {

    // 全システムの登録手続きを一時的に溜め込む受付窓口
    class SystemRegistry {
      public:
        using RegisterFunc = std::function<void(SystemManager &)>;

        // シングルトン（mainより前にアクセスされるため、静的ローカル変数で安全に初期化）
        static SystemRegistry &Instance()
        {
            static SystemRegistry instance;
            return instance;
        }

        void AddRegistration(RegisterFunc func) { _registrations.push_back(func); }

        // ゲーム起動時に1回だけ呼ばれる
        void RegisterAll(SystemManager &manager)
        {
            for (auto &reg : _registrations) {
                reg(manager); // 溜め込んだ登録処理を一斉に実行
            }
        }

      private:
        std::vector<RegisterFunc> _registrations;
    };

    // グローバル変数の初期化タイミングを利用して受付を済ませるためのヘルパー
    struct SystemRegistrar {
        SystemRegistrar(SystemRegistry::RegisterFunc func)
        {
            SystemRegistry::Instance().AddRegistration(func);
        }
    };
} // namespace CCL::ECS

// =========================================================================
// 自動登録用マクロ
// 各システムの .cpp ファイルの末尾にこれを書くだけで登録が完了する。
// ※タスクグラフが完成するまでは、過渡期としてStage（優先度）を引数に残しておく。
// =========================================================================
#define REGISTER_LOGIC_SYSTEM(SystemClass, Stage)                                                  \
    namespace {                                                                                    \
        static CCL::ECS::SystemRegistrar const auto_reg_##SystemClass(                             \
            [](CCL::ECS::SystemManager &m) { m.RegisterSystem<SystemClass>(Stage); });             \
    }

#define REGISTER_RENDER_SYSTEM(SystemClass, Stage, ...)                                            \
    namespace {                                                                                    \
        static CCL::ECS::SystemRegistrar const auto_reg_##SystemClass(                             \
            [](CCL::ECS::SystemManager &m) {                                                       \
                m.RegisterSystem<SystemClass>(Stage, __VA_ARGS__);                                 \
            });                                                                                    \
    }