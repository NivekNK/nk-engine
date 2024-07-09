#pragma once

namespace nk {
    template <typename... Args>
    class FuncHandler {
    public:
        using Func = std::function<void(Args...)>;

        explicit FuncHandler(const Func& func)
            : m_func{func} {
            m_id = s_id_counter;
            s_id_counter++;
        }

        void operator()(Args... params) const {
            if (m_func) {
                m_func(params...);
            }
        }
        
        bool operator==(const FuncHandler& other) const {
            return m_id == other.m_id;
        }

        operator bool() {
            return m_func;
        }

        u32 id() { return m_id; }

    private:
        u32 m_id;
        Func m_func;

        static std::atomic<u32> s_id_counter;
    };

    template <typename... Args>
    std::atomic<u32> FuncHandler<Args...>::s_id_counter = 0;
}