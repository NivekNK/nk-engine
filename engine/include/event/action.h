#pragma once

#include "event/function.h"
#include "core/map.h"

namespace nk {
    template<typename... Args>
    class Action {
    public:
        Action() = default;
        ~Action() = default;

        void init(Allocator* allocator) {
            m_actions.init(allocator);
        }

        u32 add_listener(const FuncHandler<Args...>::Func& func) {
            FuncHandler<Args...> aux{func};
            u32 id = aux.id();
            m_actions.insert_move(id, aux);
            return id;
        }

        bool remove_listener(const u32 id) {
            return m_actions.remove(id);
        }

        void invoke(Args... args) {
            for (auto& key_value : m_actions) {
                key_value.value(args...);
            }
        }

        void free() {
            m_actions.free();
        }

    private:
        Map<u32, FuncHandler<Args...>> m_actions;
        std::mutex m_actions_mutex;
    };
}
