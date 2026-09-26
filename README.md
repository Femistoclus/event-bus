# event_bus

Header-only C++20 event bus, with synchronous and asynchronous (SPSC) variants.

### Callback Storage

`std::function<void(const E&)>` was rejected as the callback type for the hot path because it uses type erasure via an internal vtable pointer, incurring one additional level of indirection on every invocation. Instead, each handler is stored as a `Delegate<E>`, a pair of a raw `void*` context pointer and a typed function pointer.

The one heap allocation per `subscribe()` call is irrelevant as subscriptions are registered once at startup, never in the hot loop. On the hot path `publish()` only dereferences the context pointer and calls the function pointer.

### SPSC Ring Buffer

`AsyncEventBus` stores a separate `SPSCQueue<EventType, Capacity>` for each event type, inspired by [Erik Rigtorp's SPSC queue](https://github.com/rigtorp/SPSCQueue) design.

Each atomic index and its local cached copy occupy a separate cache line (`alignas(kCacheLineSize)`). Without this, reading `write_idx_` from the consumer invalidates the producer's cache line, causing false sharing.

The producer caches its last-seen `read_idx_` and only re-reads the atomic when the cache indicates the queue is full. This converts the common case from a cross-core atomic read to a register access in the same cache line.

Queue size must be a power of 2, to provide access via bitmask without expensive division.

### `seal()`

Both buses enforce a strict two-phase cycle to prevent misuse:

```
Phase 1 (setup):   subscribe() - register callbacks
                   seal() - freeze subscription list
Phase 2 (running): publish() - enqueue events
                   dispatch() - drain queues
```

Calling `subscribe()` after `seal()`, or `publish()` before `seal()`, triggers an `assert()` in debug builds. In release builds both `assert()` and the `bool sealed_` member are optimised away, giving zero runtime overhead.

`seal()` also calls `shrink_to_fit()` on all handler vectors, releasing excess capacity. After sealing, handler vectors are immutable and their data pointers are stable for the lifetime of the bus.

### Thread Safety Model

`AsyncEventBus` is designed for the SPSC pattern, dominant in many trading systems: one thread publishes market data for each strategy, another executes subscriptions.

SPSC queues are wait-free: `try_push` and `try_pop` always complete in a bounded number of steps due to zero CAS operations. CAS-based queues (MPSC, MPMC) introduce a retry loop on contention and incur `lock cmpxchg` on x86, which invalidates the cache line on all cores.

For systems with multiple producer threads, the recommended pattern is one `AsyncEventBus` per producer thread, with the consumer calling `dispatch()` on each. Alternative way is to use a lightweight spinlock around `publish()`, or switch to an MPSC queue.

### Additional Notes

There is no way to unregister a handler, since it complicates the hot path and contradicts the usage model where all handlers are registered once before the session begins.

`AsyncEventBus` owns no threads. The caller decides which thread calls `dispatch()`, giving full control over CPU affinity, scheduling policy, and dispatch timing.

## Build

```sh
cmake -S . -B build
cmake --build build
```
