#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace CCL::ECS::Core {

    // =======================================================
    // 1. 型IDジェネレータ (重い typeid を排除する魔法)
    // =======================================================
    class EventTypeIDGenerator {
        static inline uint32_t counter = 0;

      public:
        template <typename T> static uint32_t GetID()
        {
            // 型 T ごとに、この static 変数が「たった一つだけ」作られる
            static uint32_t id = counter++;
            return id;
        }
    };

    // 購読解除用のチケット
    using EventSubscriptionID = uint64_t;

    // =======================================================
    // 2. イベントの箱 (型の消失 / 型消去)
    // =======================================================
    class IEventWrapper {
      public:
        virtual ~IEventWrapper() = default;
    };

    template <typename EventType> class EventWrapper : public IEventWrapper {
      public:
        using CallbackType = std::function<void(const EventType &)>;

        // 誰の関数かを特定するために、チケットIDと一緒に保存する
        struct Subscriber {
            EventSubscriptionID id;
            CallbackType        callback;
        };

        std::vector<Subscriber> subscribers;
        EventSubscriptionID     nextId = 1;

        // 関数を登録し、キャンセル用のチケットを返す
        EventSubscriptionID Add(CallbackType cb)
        {
            EventSubscriptionID id = nextId++;
            subscribers.push_back({id, cb});
            return id;
        }

        // チケットを使って特定の関数だけを削除する
        void Remove(EventSubscriptionID id)
        {
            auto it = std::remove_if(subscribers.begin(),
                subscribers.end(),
                [id](const Subscriber &s) { return s.id == id; });
            if (it != subscribers.end()) {
                subscribers.erase(it, subscribers.end());
            }
        }

        // 全員に手紙を配る (イベント発火)
        void Trigger(const EventType &eventData)
        {
            for (auto &s : subscribers) {
                s.callback(eventData);
            }
        }
    };

    // =======================================================
    // 3. 郵便局本体 (EventBus)
    // =======================================================
    class EventBus {
      private:
        // 「型のID」と「そのイベント専用の箱」のペアを管理する名簿
        std::unordered_map<uint32_t, std::unique_ptr<IEventWrapper>> _subscribers;

      public:
        // 購読する (手紙が欲しいと登録する)
        template <typename EventType>
        EventSubscriptionID Subscribe(std::function<void(const EventType &)> callback)
        {
            uint32_t typeId = EventTypeIDGenerator::GetID<EventType>();

            if (_subscribers.find(typeId) == _subscribers.end()) {
                _subscribers[typeId] = std::make_unique<EventWrapper<EventType>>();
            }

            auto wrapper = static_cast<EventWrapper<EventType> *>(_subscribers[typeId].get());
            return wrapper->Add(callback);
        }

        // 購読を解除する (もう手紙は要らないと伝える)
        template <typename EventType> void Unsubscribe(EventSubscriptionID subId)
        {
            uint32_t typeId = EventTypeIDGenerator::GetID<EventType>();

            if (_subscribers.find(typeId) != _subscribers.end()) {
                auto wrapper = static_cast<EventWrapper<EventType> *>(_subscribers[typeId].get());
                wrapper->Remove(subId);
            }
        }

        // 手紙を投函する (全員に配る)
        template <typename EventType> void Publish(const EventType &eventData)
        {
            uint32_t typeId = EventTypeIDGenerator::GetID<EventType>();

            if (_subscribers.find(typeId) != _subscribers.end()) {
                auto wrapper = static_cast<EventWrapper<EventType> *>(_subscribers[typeId].get());
                wrapper->Trigger(eventData);
            }
        }

        // 世界がリセットされる時に名簿を空にする
        void Clear() { _subscribers.clear(); }
    };

} // namespace CCL::ECS::Core