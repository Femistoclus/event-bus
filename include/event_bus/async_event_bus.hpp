#pragma once

#include <cassert>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <vector>

#include "event_bus/detail/delegate.hpp"
#include "event_bus/detail/spsc_queue.hpp"
#include "event_bus/detail/type_list.hpp"

namespace event_bus {

/// Asynchronous event bus: publish() enqueues, dispatch() executes callbacks.
///
/// Usage phases (must be followed in order):
///   1. subscribe() -- register all callbacks
///   2. seal()      -- freeze subscriptions;
///   3. publish()   -- enqueue events (producer thread, hot path)
///      dispatch()  -- drain queues and invoke callbacks (consumer thread)
///
/// Thread safety:
/// - Exactly one thread may call publish() (producer thread)
/// - Exactly one thread may call dispatch() (consumer thread)
/// - Producer and consumer may be different threads
/// - subscribe()/seal() must complete before any publish()/dispatch() calls

template<size_t QueueCapacity, typename... Events>
class AsyncEventBus {
    static_assert(sizeof...(Events) > 0,
                  "AsyncEventBus requires at least one event type");
    static_assert(!detail::has_duplicates_v<Events...>,
                  "AsyncEventBus event types must have no duplicates");
    static_assert(QueueCapacity > 0 && (QueueCapacity & (QueueCapacity - 1)) == 0,
                  "QueueCapacity must be a power of 2");
    static_assert((std::is_nothrow_move_constructible_v<Events> && ...),
                  "All event types must be nothrow move constructible for lock-free queue safety");

    template<typename E>
    using HandlerList = std::vector<detail::Delegate<E>>;

    std::tuple<HandlerList<Events>...> handlers_;
    std::tuple<detail::SPSCQueue<Events, QueueCapacity>...> queues_;
    bool sealed_ = false;

    /// Drain the queue for a single event type and invoke all handlers.
    template<typename E>
    size_t dispatch_type() {
        auto& queue = std::get<detail::type_index_v<E, Events...>>(queues_);
        const auto& handlers = std::get<detail::type_index_v<E, Events...>>(handlers_);

        size_t count = 0;
        while (auto event = queue.try_pop()) {
            for (const auto& handler : handlers) {
                handler(*event);
            }
            ++count;
        }
        return count;
    }

public:
    AsyncEventBus() = default;

    AsyncEventBus(const AsyncEventBus&) = delete;
    AsyncEventBus& operator=(const AsyncEventBus&) = delete;
    AsyncEventBus(AsyncEventBus&&) = delete;
    AsyncEventBus& operator=(AsyncEventBus&&) = delete;

    /// Subscribe a callback for event type E. Must be called before seal().
    template<typename E, EventCallback<E> F>
        requires RegisteredEvent<E, Events...>
    void subscribe(F&& callback) {
        assert(!sealed_ && "subscribe() called after seal()");
        auto& list = std::get<detail::type_index_v<E, Events...>>(handlers_);
        list.emplace_back(std::forward<F>(callback));
    }

    /// Freeze the subscription list. Must be called before publish()/dispatch().
    void seal() noexcept {
        assert(!sealed_ && "seal() called more than once");
        (std::get<detail::type_index_v<Events, Events...>>(handlers_).shrink_to_fit(), ...);
        sealed_ = true;
    }

    /// Enqueue an event for later dispatch. Returns immediately.
    /// Returns true if enqueued, false if the queue is full.
    /// Must be called after seal(), from the single producer thread.
    template<typename E>
        requires RegisteredEvent<E, Events...>
    [[nodiscard]] bool publish(E event) {
        assert(sealed_ && "publish() called before seal()");
        auto& queue = std::get<detail::type_index_v<E, Events...>>(queues_);
        return queue.try_push(std::move(event));
    }

    /// Process all pending events across all event types.
    /// Returns the total number of events dispatched.
    /// Must be called after seal(), from the single consumer thread.
    size_t dispatch() {
        assert(sealed_ && "dispatch() called before seal()");
        size_t total = 0;
        ((total += dispatch_type<Events>()), ...);
        return total;
    }

    [[nodiscard]] bool is_sealed() const noexcept { return sealed_; }

    template<typename E>
        requires RegisteredEvent<E, Events...>
    [[nodiscard]] size_t subscriber_count() const noexcept {
        return std::get<detail::type_index_v<E, Events...>>(handlers_).size();
    }
};

template<typename... Events>
using DefaultAsyncEventBus = AsyncEventBus<4096, Events...>;

} // namespace event_bus
