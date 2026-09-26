#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>

namespace event_bus::detail {

/// Lock-free SPCS bounded ring buffer.

template<typename T, size_t Capacity>
class SPSCQueue {
    static_assert(Capacity > 0, "Queue capacity must be greater than zero");
    static_assert((Capacity & (Capacity - 1)) == 0, "Queue capacity must be a power of 2");
    static_assert(std::is_nothrow_destructible_v<T>, "T must be nothrow destructible");

#ifdef __cpp_lib_hardware_interference_size
    static constexpr size_t kCacheLineSize = std::hardware_destructive_interference_size;
#else
    static constexpr size_t kCacheLineSize = 64;
#endif
    static constexpr size_t kMask = Capacity - 1;

    struct alignas(alignof(T)) Slot {
        unsigned char data[sizeof(T)];

        // I guess we need std::launder here if T is not trivially copyable
        T* ptr() noexcept { return std::launder(reinterpret_cast<T*>(data)); }
        const T* ptr() const noexcept { return std::launder(reinterpret_cast<const T*>(data)); }
    };

    alignas(kCacheLineSize) std::atomic<size_t> write_idx_{0};
    alignas(kCacheLineSize) size_t cached_read_idx_{0};

    alignas(kCacheLineSize) std::atomic<size_t> read_idx_{0};
    alignas(kCacheLineSize) size_t cached_write_idx_{0};

    alignas(kCacheLineSize) Slot slots_[Capacity];

public:
    SPSCQueue() = default;

    ~SPSCQueue() {
        size_t read = read_idx_.load(std::memory_order_relaxed);
        size_t write = write_idx_.load(std::memory_order_relaxed);
        while (read != write) {
            slots_[read & kMask].ptr()->~T();
            ++read;
        }
    }

    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;
    SPSCQueue(SPSCQueue&&) = delete;
    SPSCQueue& operator=(SPSCQueue&&) = delete;

    bool try_push(const T& val) noexcept(std::is_nothrow_copy_constructible_v<T>) {
        const size_t write = write_idx_.load(std::memory_order_relaxed);
        if (write - cached_read_idx_ >= Capacity) {
            cached_read_idx_ = read_idx_.load(std::memory_order_acquire);
            if (write - cached_read_idx_ >= Capacity) {
                return false; // Full queue
            }
        }
        new (&slots_[write & kMask].data) T(val);
        write_idx_.store(write + 1, std::memory_order_release);
        return true;
    }

    bool try_push(T&& val) noexcept(std::is_nothrow_move_constructible_v<T>) {
        const size_t write = write_idx_.load(std::memory_order_relaxed);
        if (write - cached_read_idx_ >= Capacity) {
            cached_read_idx_ = read_idx_.load(std::memory_order_acquire);
            if (write - cached_read_idx_ >= Capacity) {
                return false;
            }
        }
        new (&slots_[write & kMask].data) T(std::move(val));
        write_idx_.store(write + 1, std::memory_order_release);
        return true;
    }

    template<typename... Args>
    bool try_emplace(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
        const size_t write = write_idx_.load(std::memory_order_relaxed);
        if (write - cached_read_idx_ >= Capacity) {
            cached_read_idx_ = read_idx_.load(std::memory_order_acquire);
            if (write - cached_read_idx_ >= Capacity) {
                return false;
            }
        }
        new (&slots_[write & kMask].data) T(std::forward<Args>(args)...);
        write_idx_.store(write + 1, std::memory_order_release);
        return true;
    }

    std::optional<T> try_pop() noexcept(std::is_nothrow_move_constructible_v<T>) {
        const size_t read = read_idx_.load(std::memory_order_relaxed);
        if (read == cached_write_idx_) {
            cached_write_idx_ = write_idx_.load(std::memory_order_acquire);
            if (read == cached_write_idx_) {
                return std::nullopt; // Empty queue
            }
        }
        T* ptr = slots_[read & kMask].ptr();
        std::optional<T> val{std::move(*ptr)};
        ptr->~T();
        read_idx_.store(read + 1, std::memory_order_release);
        return val;
    }

    [[nodiscard]] bool empty() const noexcept {
        return read_idx_.load(std::memory_order_acquire) == write_idx_.load(std::memory_order_acquire);
    }

    [[nodiscard]] size_t size() const noexcept {
        const size_t write = write_idx_.load(std::memory_order_acquire);
        const size_t read = read_idx_.load(std::memory_order_acquire);
        return write - read;
    }

    static constexpr size_t capacity() noexcept { return Capacity; }
};

} // namespace event_bus::detail
