#pragma once
#include "ComponentMeta.h"
#include <DirectXMath.h>
#include <cstring>
#include <imgui.h>
#include <type_traits> // メタプログラミング用
#include "Editor/Core/EditorCommandHistory.h"
#include "Engine/Platform/Logger.h"

namespace ComponentMetaImGui {

    // ========================================================================
    //  SFINAE 1: カスタムGUI変数が定義されているかを検知
    // ========================================================================
    template <typename T, typename = void> struct CheckCustomGui : std::false_type {};

    template <typename T>
    struct CheckCustomGui<T, std::void_t<decltype(ComponentMeta<T>::hasCustomGui)>>
        : std::bool_constant<ComponentMeta<T>::hasCustomGui> {};

    // ========================================================================
    //  SFINAE 2: CustomGui が「3引数」で定義されているかを検知
    // ========================================================================
    template <typename T, typename = void> struct Has3ArgsCustomGui : std::false_type {};

    template <typename T>
    struct Has3ArgsCustomGui<T,
        std::void_t<decltype(ComponentMeta<T>::CustomGui(std::declval<T &>(), 0ULL, nullptr))>>
        : std::true_type {};

    // ========================================================================
    //  SFINAE 3: コンポーネントが「isDirty」という変数を持っているかを検知
    // ========================================================================
    template <typename T, typename = void> struct HasIsDirty : std::false_type {};

    template <typename T>
    struct HasIsDirty<T, std::void_t<decltype(std::declval<T>().isDirty)>> : std::true_type {};


    // ------------------------------------------------------------------------
    // DrawField 関数は既存のまま完全に維持 (変更なし)
    // ------------------------------------------------------------------------
    inline bool DrawField(const FieldDescriptor &fd, void *obj)
    {
        using namespace DirectX;
        bool changed = false;
        switch (fd.kind) {
        case FieldKind::Float: {
            float &v = RawField<float>(obj, fd.offset);

            // 第4引数(min)と第5引数(max)を渡すことで、ドラッグ操作のまま上限・下限をロックできる。
            changed = ImGui::DragFloat(fd.name, &v, fd.dragSpeed, fd.rangeMin, fd.rangeMax);
            break;
        }
        case FieldKind::Int: {
            int &v = RawField<int>(obj, fd.offset);

            changed = ImGui::DragInt(fd.name, &v, fd.dragSpeed, (int)fd.rangeMin, (int)fd.rangeMax);
            break;
        }
        case FieldKind::Bool: {
            changed = ImGui::Checkbox(fd.name, &RawField<bool>(obj, fd.offset));
            break;
        }
        case FieldKind::UInt8: {
            uint8_t &v   = RawField<uint8_t>(obj, fd.offset);
            int      tmp = (int)v;
            if (ImGui::DragInt(fd.name, &tmp, 1.0f, 0, 255)) {
                v       = (uint8_t)tmp;
                changed = true;
            }
            break;
        }
        case FieldKind::UInt16: {
            uint16_t &v   = RawField<uint16_t>(obj, fd.offset);
            int       tmp = (int)v;
            if (ImGui::DragInt(fd.name, &tmp, 1.0f, 0, 65535)) {
                v       = (uint16_t)tmp;
                changed = true;
            }
            break;
        }
        case FieldKind::UInt32: {
            uint32_t &v   = RawField<uint32_t>(obj, fd.offset);
            int       tmp = (int)v;
            if (ImGui::DragInt(fd.name, &tmp, 1.0f, 0, 4294967295)) {
                v       = (uint32_t)tmp;
                changed = true;
            }
            break;
        }
        case FieldKind::EntityID: {
            // 1. RawField関数を使って、コンポーネント内の正しい変数の参照を安全に取得する
            // ※ fd.offset分だけアドレスを進めて uint32_t として扱う
            auto& val = RawField<uint32_t>(&obj, fd.offset);

            // 2. UI描画 (InputScalar で 32bit整数として扱う)
            if (ImGui::InputScalar(fd.name, ImGuiDataType_U32, &val)) {
                changed = true;
            }

            // =========================================================
            // 3. ★ ドラッグ＆ドロップ（ターゲット/受け取り側）の実装
            // =========================================================
            // 直前に描画した InputScalar の矩形領域がドロップ判定エリアになる
            if (ImGui::BeginDragDropTarget()) {

                // ※注意: "ENTITY" の部分は、ヒエラルキービュー（送信側）が
                // SetDragDropPayload で設定している文字列に合わせる必要がある。
                // よくある命名例: "ENTITY", "ENTITY_ID", "HIERARCHY_ENTITY" など
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_ENTITY_ID")) {

                    // 安全性確認：送られてきたデータサイズが uint32_t(4バイト) であること
                    if (payload->DataSize == sizeof(uint32_t)) {
                        val = *(const uint32_t*)payload->Data;
                        changed = true;

                        // デバッグ用（不要なら消してよい）
                        CCL_LOG_INFO(LogCategory::Editor, "[ImGui] EntityID %u dropped into %s", val, fd.name);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            // ツールチップ（マウスオーバーでD&D可能であることを教える）
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("ドラッグ＆ドロップでエンティティをアタッチ可能");
            }

            break;
        }
        case FieldKind::EnumU8: {
            uint8_t &v   = RawField<uint8_t>(obj, fd.offset);
            int      cur = (int)v;
            if (fd.enumNames && fd.enumCount > 0) {
                if (ImGui::Combo(fd.name, &cur, fd.enumNames, fd.enumCount)) {
                    v       = (uint8_t)cur;
                    changed = true;
                }
            }
            else {
                int tmp = cur;
                if (ImGui::DragInt(fd.name, &tmp, 1, 0, 255)) {
                    v       = (uint8_t)tmp;
                    changed = true;
                }
            }
            break;
        }
        case FieldKind::EnumInt: {
            int &v = RawField<int>(obj, fd.offset);
            if (fd.enumNames && fd.enumCount > 0)
                changed = ImGui::Combo(fd.name, &v, fd.enumNames, fd.enumCount);
            else
                changed = ImGui::DragInt(fd.name, &v);
            break;
        }
        case FieldKind::Float2: {
            changed =
                ImGui::DragFloat2(fd.name, &RawField<XMFLOAT2>(obj, fd.offset).x, fd.dragSpeed);
            break;
        }
        case FieldKind::Float3: {
            changed =
                ImGui::DragFloat3(fd.name, &RawField<XMFLOAT3>(obj, fd.offset).x, fd.dragSpeed);
            break;
        }
        case FieldKind::Float4: {
            changed =
                ImGui::DragFloat4(fd.name, &RawField<XMFLOAT4>(obj, fd.offset).x, fd.dragSpeed);
            break;
        }
        case FieldKind::String: {
            std::string &v = RawField<std::string>(obj, fd.offset);
            char         buf[256];
            strncpy_s(buf, sizeof(buf), v.c_str(), _TRUNCATE);
            if (ImGui::InputText(fd.name, buf, sizeof(buf))) {
                v       = buf;
                changed = true;
            }
            break;
        }
        default: break;
        }
        return changed;
    }

    // ------------------------------------------------------------------------
    // DrawInspector: 自動描画と手動描画(CustomGui)を静的に分岐させる
    // ------------------------------------------------------------------------
    template <typename T>
    bool DrawInspector(T &component, unsigned long long entity = 0, void *world = nullptr)
    {
        bool anyChanged = false;

        // ラッグ開始時の状態を保持するスタティック変数
        // UI操作は同時に1つしかできないため、staticで安全に保持できます
        static T s_backupState;

        ImGui::PushID(ComponentMeta<T>::displayName);
        ImGui::Indent(4.0f);

        //  自動描画か手動描画かをコンパイル時に分岐
        if constexpr (CheckCustomGui<T>::value) {
            
            static bool s_isEditing = false;

            T preState = component; // 描画される前の「過去の状態」をメモ
            bool guiChanged = false;

            // 実際のカスタムGUIを描画
            if constexpr (Has3ArgsCustomGui<T>::value) {
                guiChanged = ComponentMeta<T>::CustomGui(component, entity, world);
            }
            else {
                guiChanged = ComponentMeta<T>::CustomGui(component);
            }

            // もしカスタムGUIの中で値が変更されたら
            if (guiChanged) {
                anyChanged = true;

                // 初めて変更された瞬間に、過去の状態をバックアップとしてロックする
                if (!s_isEditing) {
                    s_backupState = preState;
                    s_isEditing = true;
                    CCL_LOG_INFO(LogCategory::Editor, "[UndoSystem] CustomGUI Edit Started: %s", ComponentMeta<T>::displayName);
                }
            }

            // ユーザーがマウスの左クリックを離した、またはテキスト入力を終えた瞬間の判定
            if (s_isEditing && !ImGui::IsAnyItemActive()) {
                if (world && entity != 0) {
                    auto* ecsWorld = static_cast<CCL::ECS::Core::World*>(world);
                    auto ecsEntity = static_cast<CCL::ECS::EntityID>(entity);

                    CCL_LOG_INFO(LogCategory::Editor, "[UndoSystem] CustomGUI Edit Finished: %s", ComponentMeta<T>::displayName);

                    // 履歴マネージャーに提出！
                    EditorCommandHistory::Instance().ExecuteCommand(
                        std::make_unique<ChangeComponentCommand<T>>(
                            ecsWorld, ecsEntity, s_backupState, component
                        )
                    );
                }
                // 編集状態をリセット
                s_isEditing = false;
            }
            
        }
        else {
            // カスタムGUIがない場合は、従来通りの自動生成描画を実行する
            const auto &fields = ComponentMeta<T>::Fields();
            if (!fields.empty()) {
                void       *obj         = &component;
                const char *curCategory = nullptr;
                bool        catOpen     = true;

                for (const auto &fd : fields) {
                    if (fd.category != nullptr) {
                        if (curCategory == nullptr || strcmp(fd.category, curCategory) != 0) {
                            if (curCategory != nullptr && catOpen) {
                                ImGui::Unindent(12.0f);
                                ImGui::TreePop();
                            }
                            curCategory = fd.category;
                            catOpen     = ImGui::TreeNodeEx(curCategory,
                                ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
                            if (catOpen) ImGui::Indent(12.0f);
                        }
                    }
                    if (!catOpen) continue;

                    ImGui::PushID((int)fd.offset);

                    // ==========================================================
                    // 描画とUndo/Redoフックの統合
                    // ==========================================================
                    bool fieldChanged = DrawField(fd, obj);
                    if (fieldChanged) anyChanged = true;

                    // 1. スライダーをクリックした瞬間（ドラッグ開始前）の値をバックアップ
                    if (ImGui::IsItemActivated()) {
                        s_backupState = component;

                        // 🔍 【足跡 1】掴んだ瞬間をログ出力
                        CCL_LOG_INFO(LogCategory::Editor, "[UndoSystem] Mouse Activated: Backup taken for %s", ComponentMeta<T>::displayName);
                    }

                    // 2. マウスを離した瞬間（編集完了）にコマンドを発行！
                    if (ImGui::IsItemDeactivatedAfterEdit()) {

                        // 🔍 【足跡 2】離した（編集完了）瞬間をログ出力
                        CCL_LOG_INFO(LogCategory::Editor, "[UndoSystem] Mouse Deactivated: Editing finished for %s", ComponentMeta<T>::displayName);

                        if (world && entity != 0) {
                            auto* ecsWorld = static_cast<CCL::ECS::Core::World*>(world);
                            auto ecsEntity = static_cast<CCL::ECS::EntityID>(entity);

                            // 🔍 【足跡 3】コマンドの発行を確認
                            CCL_LOG_INFO(LogCategory::Editor, "[UndoSystem] Issued ChangeCommand for Entity ID: %llu", ecsEntity);

                            EditorCommandHistory::Instance().ExecuteCommand(
                                std::make_unique<ChangeComponentCommand<T>>(
                                    ecsWorld, ecsEntity, s_backupState, component
                                )
                            );
                        }
                        else {
                            // ⚠️ 異常事態：ポインタが渡ってきていない場合の警告
                            CCL_LOG_WARN(LogCategory::Editor, "[UndoSystem] Failed to issue command! World or Entity is null.");
                        }
                    }

                    ImGui::PopID();
                }

                if (curCategory != nullptr && catOpen) {
                    ImGui::Unindent(12.0f);
                    ImGui::TreePop();
                }
            }
        }

        // ========================================================================
        // ★最強の自動化: 
        // どの値でもいいから変更され(anyChanged == true)、かつ
        // このコンポーネントが isDirty 変数を持っているなら、強制的に true を叩き込む！
        // ========================================================================
        if (anyChanged) {
            if constexpr (HasIsDirty<T>::value) {
                component.isDirty = true;
            }
        }

        ImGui::Unindent(4.0f);
        ImGui::PopID();
        return anyChanged;
    }

    // 登録時にポインタとIDをキャストしてリレーする
    template <typename T, typename RegistryType> void RegisterGuiMeta(RegistryType &registry)
    {
        registry.template Register<T>(
            std::string(
                ComponentMeta<T>::displayName), // const char* を std::string に明示的キャスト
            typename RegistryType::GuiFunc([](void *ptr, auto entity, auto *world) {
                // DrawInspector<T> と明示し、引数も全てキャストして渡す
                DrawInspector<T>(*static_cast<T *>(ptr),
                    static_cast<unsigned long long>(entity),
                    static_cast<void *>(world));
            }));
    }

} // namespace ComponentMetaImGui