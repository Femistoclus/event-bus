#pragma once

#include <concepts>
#include <cstddef>
#include <type_traits>

namespace event_bus::detail {

template<typename T, typename... Ts>
inline constexpr bool contains_v = (std::is_same_v<T, Ts> || ...);

// Compile-time index of type T in the parameter pack

template<typename T, typename First, typename... Rest>
struct type_index : std::integral_constant<size_t, 1 + type_index<T, Rest...>::value> {};

template<typename T, typename... Rest>
struct type_index<T, T, Rest...> : std::integral_constant<size_t, 0> {};

template<typename T, typename... Ts>
inline constexpr size_t type_index_v = type_index<T, Ts...>::value;

template<typename...>
inline constexpr bool has_duplicates_v = false;

template<typename First, typename... Rest>
inline constexpr bool has_duplicates_v<First, Rest...> =
    contains_v<First, Rest...> || has_duplicates_v<Rest...>;

} // namespace event_bus::detail

namespace event_bus {

// E must be one of the registered Events
template<typename E, typename... Events>
concept RegisteredEvent = detail::contains_v<E, Events...>;

// F must be callable with const E&
template<typename F, typename E>
concept EventCallback = std::invocable<F, const E&>;

} // namespace event_bus
