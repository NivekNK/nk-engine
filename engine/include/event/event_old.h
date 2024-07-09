// #pragma once

// #include "event/event_handler.h"
// #include "core/map.h"

// namespace nk {
//     template <typename... Args>
//     class Event {
//     public:
//         using Handler = EventHandler<Args...>;

//         Event() {};

//         void init(Allocator* allocator) {
//             m_handlers.init(allocator);
//         }

//         Event(const Event& event) {
//             std::scoped_lock lock(event.m_handlers_mutex);
//             m_handlers = event.m_handlers;
//         }

//         Event(Event&& event) {
//             std::scoped_lock lock(event.m_handlers_mutex);
//             m_handlers = std::move(event.m_handlers);
//         }

//         Event& operator=(const Event& event) {
//             std::scoped_lock lock(m_handlers_mutex);
//             std::scoped_lock lock2(event.m_handlers_mutex);

//             m_handlers = event.m_handlers;

//             return *this;
//         }

//         Event& operator=(Event&& event) {
//             std::scoped_lock lock(m_handlers_mutex);
//             std::scoped_lock lock2(event.m_handlers_mutex);

//             std::swap(m_handlers, event.m_handlers);

//             return *this;
//         }

//         u32 add_listener(const Handler& handler) {
//             std::scoped_lock lock(m_handlers_mutex);
//             m_handlers.insert(handler.id(), handler);
//             return handler.id();
//         }

//         u32 add_listener(const Handler::HandlerFunc& func) {
//             return add_listener(Handler(func));
//         }

//         bool remove_listener(const Handler& handler) {
//             std::scoped_lock lock(m_handlers_mutex);
//             return m_handlers.remove(handler.id());
//         }

//         bool remove_listener(const u32 id) {
//             std::scoped_lock lock(m_handlers_mutex);
//             return m_handlers.remove(id);
//         }

//         void invoke(Args... args) const {
//             auto handlers = get_handlers_copy();
//             DebugLog("Handlers copy?");
//             invoke_impl(handlers, args...);
//         }

//         std::future<void> invoke_async(Args... args) const {
//             return std::async(
//                 std::launch::async, [this](Args... asyncArgs) {
//                     invoke(asyncArgs...);
//                 },
//                 args...);
//         }

//         inline void operator()(Args... args) const {
//             invoke(args...);
//         }

//         inline u32 operator+=(const Handler& handler) {
//             return add_listener(handler);
//         }

//         inline u32 operator+=(const Handler::HandlerFunc& func) {
//             return add_listener(func);
//         }

//         inline bool operator-=(const Handler& handler) {
//             return remove_listener(handler);
//         }

//     private:
//         using HandlersMap = Map<u32, Handler>;

//         void invoke_impl(const HandlersMap& handlers, Args... args) const {
//             for (const auto& handler : handlers) {
//                 u32 id = handler.value.id();
//                 DebugLog("TESTING: {}", id);
//                 handler.value(args...);
//             }
//         }

//         HandlersMap get_handlers_copy() const {
//             std::scoped_lock lock(m_handlers_mutex);
//             return m_handlers;
//         }

//         HandlersMap m_handlers;
//         mutable std::mutex m_handlers_mutex;
//     };
// }
