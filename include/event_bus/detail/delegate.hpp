#pragma once

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>

namespace event_bus::detail {

// Move-only 
template<typename E>
class Delegate {
    using InvokeFn = void(*)(void*, const E&);
    using DestroyFn = void(*)(void*);

    void* context_ = nullptr;
    InvokeFn invoke_ = nullptr;
    DestroyFn destroy_ = nullptr;

public:
    Delegate() = default;

    template<typename F>
        requires std::invocable<std::decay_t<F>, const E&>
              && (!std::same_as<std::decay_t<F>, Delegate>)
    explicit Delegate(F&& f) {
        using Callable = std::decay_t<F>;

        context_ = static_cast<void*>(new Callable(std::forward<F>(f)));
        invoke_ = [](void* ctx, const E& e) {
            (*static_cast<Callable*>(ctx))(e);
        };
        destroy_ = [](void* ctx) {
            delete static_cast<Callable*>(ctx);
        };
    }

    ~Delegate() {
        if (destroy_) {
            destroy_(context_);
        }
    }

    Delegate(Delegate&& other) noexcept
        : context_(other.context_)
        , invoke_(other.invoke_)
        , destroy_(other.destroy_)
    {
        other.context_ = nullptr;
        other.invoke_ = nullptr;
        other.destroy_ = nullptr;
    }

    Delegate& operator=(Delegate&& other) noexcept {
        if (this != &other) {
            if (destroy_) {
                destroy_(context_);
            }
            context_ = other.context_;
            invoke_ = other.invoke_;
            destroy_ = other.destroy_;

            other.context_ = nullptr;
            other.invoke_ = nullptr;
            other.destroy_ = nullptr;
        }
        return *this;
    }

    Delegate(const Delegate&) = delete;

    Delegate& operator=(const Delegate&) = delete;

    void operator()(const E& event) const {
        invoke_(context_, event);
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return invoke_ != nullptr;
    }
};

} // namespace event_bus::detail
