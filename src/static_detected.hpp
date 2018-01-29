//
// Created by dx2880 on 2018/1/15.
//

#ifndef CONNECTIONPOOL_STATIC_DETECTED_HPP
#define CONNECTIONPOOL_STATIC_DETECTED_HPP

#include <type_traits>

namespace modern_utils {
template <typename T, typename U>
using enable_if_same = std::enable_if_t<std::is_same< T, U >::value >;

template <typename ... T>
using void_t = void;

template <typename /**/,
        template <typename...> class C,
        typename... T>
struct is_detected_
        : std::false_type {};

template <template <typename...> class C,
        typename... T>
struct is_detected_<void_t<C<T...>>,
C,
T...>
: std::true_type {};

template <template <typename...> class C,
        typename... T>
using is_detected = is_detected_<void, C, T...>;
}

#endif //CONNECTIONPOOL_STATIC_DETECTED_HPP
