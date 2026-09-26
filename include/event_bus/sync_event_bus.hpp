#pragma once

#include <cassert>
#include <cstddef>
#include <tuple>
#include <vector>

#include "event_bus/detail/delegate.hpp"
#include "event_bus/detail/type_list.hpp"

namespace event_bus {

/// Synchronous event bus: publish() blocks until all subscriber callbacks finish.
///
/// Usage phases (must be followed in order):
///   1. subscribe() -- register all callbacks
///   2. seal()      -- freeze the subscription list; transitions to running phase
///   3. publish()   -- deliver events (hot path)
///
/// Thread safety:
/// - subscribe()/seal() must complete on one thread before publish() is called.
/// - Multiple publish() calls from the same thread are safe after seal().

template<typename... Events>
class SyncEventBus {
    static_assert(sizeof...(Events) > 0,
                  "SyncEventBus requires at least one event type");
    static_assert(!detail::has_duplicates_v<Events...>,
                  "SyncEventBus event types must be unique (no duplicates)");

    template<typename E>
    using HandlerList = std::vector<detail::Delegate<E>>;

    std::tuple<HandlerList<Events>...> handlers_;
    bool sealed_ = false;

public:
    SyncEventBus() = default;

    SyncEventBus(const SyncEventBus&) = delete;
    SyncEventBus& operator=(const SyncEventBus&) = delete;
    SyncEventBus(SyncEventBus&&) = delete;
    SyncEventBus& operator=(SyncEventBus&&) = delete;

    /// Subscribe a callback for event type E.
    /// Must be called before seal().
    template<typename E, EventCallback<E> F>
        requires RegisteredEvent<E, Events...>
    void subscribe(F&& callback) {
        assert(!sealed_ && "subscribe() called after seal()");
        auto& list = std::get<detail::type_index_v<E, Events...>>(handlers_);
        list.emplace_back(std::forward<F>(callback));
    }

    /// Freeze the subscription list. Must be called before the first publish().
    /// Shrinks handler vectors to fit (releases unused capacity).
    void seal() noexcept {
        assert(!sealed_ && "seal() called more than once");
        (std::get<detail::type_index_v<Events, Events...>>(handlers_).shrink_to_fit(), ...);
        sealed_ = true;
    }

    /// Publish an event to all subscribers. Blocks until all callbacks complete.
    /// Must be called after seal().
    template<typename E>
        requires RegisteredEvent<E, Events...>
    void publish(const E& event) {
        assert(sealed_ && "publish() called before seal()");
        const auto& list = std::get<detail::type_index_v<E, Events...>>(handlers_);
        for (const auto& handler : list) {
            handler(event);
        }
    }

    [[nodiscard]] bool is_sealed() const noexcept { return sealed_; }

    template<typename E>
        requires RegisteredEvent<E, Events...>
    [[nodiscard]] size_t subscriber_count() const noexcept {
        return std::get<detail::type_index_v<E, Events...>>(handlers_).size();
    }
};

} // namespace event_bus
