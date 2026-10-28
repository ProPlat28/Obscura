#pragma once
#include <variant>
#include <vector>
#include <unordered_map>
#include <functional>
#include <typeindex>

namespace obscura::core {
    struct PlayerMovedEvent { int entityId; float x, y; };
    struct CheatActivatedEvent { int entityId; };
    using EventVariant = std::variant<PlayerMovedEvent, CheatActivatedEvent>;

    class EventBus {
    public:
        template<typename TEvent>
        void Subscribe(std::function<void(const TEvent&)> callback) {
            m_listeners[std::type_index(typeid(TEvent))].push_back(
                [callback](const EventVariant& e) { callback(std::get<TEvent>(e)); }
            );
        }
        template<typename TEvent>
        void Publish(const TEvent& event) {
            EventVariant variant = event;
            auto it = m_listeners.find(std::type_index(typeid(TEvent)));
            if (it != m_listeners.end()) for (auto& l : it->second) l(variant);
        }
    private:
        std::unordered_map<std::type_index, std::vector<std::function<void(const EventVariant&)>>> m_listeners;
    };
}
