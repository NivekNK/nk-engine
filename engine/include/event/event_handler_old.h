// #pragma once

// namespace nk {
//     template <typename... Args>
//     class EventHandler {
//     public:
//         using HandlerFunc = std::function<void(Args...)>;

//         explicit EventHandler(const HandlerFunc& func)
//             : m_func{func} {
//             m_id = s_id_counter++;
//         }

//         EventHandler(const EventHandler& other)
//             : m_id{other.m_id}, m_func{other.m_func} {
//         }

//         EventHandler(EventHandler&& other)
//             : m_id{other.m_id}, m_func{std::move(other.m_func)} {
//         }

//         EventHandler& operator=(const EventHandler& other) {
//             m_id = other.m_id;
//             m_func = other.m_func;
//             return *this;
//         }

//         EventHandler& operator=(EventHandler&& other) {
//             m_id = other.m_id;
//             std::swap(m_func, other.m_func);
//             return *this;
//         }

//         void operator()(Args... params) const {
//             DebugLog("AQUI?");
//             m_func(params...);
//             if (m_func) {
//                 DebugLog("ANTES DE LA FUNCION");
//                 m_func(params...);
//                 DebugLog("DESPUES DE LA FUNCION");
//             }
//         }

//         bool operator==(const EventHandler& other) const {
//             return m_id == other.m_id;
//         }

//         operator bool() {
//             return m_func;
//         }

//         u32 id() const { return m_id; }

//     private:
//         u32 m_id;
//         HandlerFunc m_func;

//         static std::atomic<u32> s_id_counter;
//     };

//     template <typename... Args>
//     std::atomic<u32> EventHandler<Args...>::s_id_counter = 0;
// }
