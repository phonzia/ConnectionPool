//
// Created by dx2880 on 2018/1/16.
//

#ifndef CONNECTIONPOOL_CONN_FACTORY_CONCEPT_HPP
#define CONNECTIONPOOL_CONN_FACTORY_CONCEPT_HPP

#include "static_detected.hpp"

namespace modern_utils{
namespace traits {
template<typename Conn, typename ConnFactory>
using static_create_connection = enable_if_same<decltype(std::declval<ConnFactory>().createConnection()), Conn *>;

template<typename Conn, typename ConnFactory>
using static_check_valid = enable_if_same<decltype(std::declval<ConnFactory>().checkValid(
        static_cast<Conn *>(nullptr))), bool>;

template<typename Conn, typename ConnFactory>
using static_destroy = enable_if_same<decltype(std::declval<ConnFactory>().destroy(
        static_cast<Conn *>(nullptr))), void>;

template<typename Conn, typename ConnFactory>
using all = void_t<static_create_connection<Conn, ConnFactory>,
        static_check_valid<Conn, ConnFactory>,
        static_destroy<Conn, ConnFactory>>;
};

template<typename Conn, typename ConnFactory>
struct is_acceptable : is_detected<traits::all, Conn, ConnFactory> {
    constexpr static bool diagnose() {
        static_assert(is_detected<traits::static_create_connection, Conn, ConnFactory>::value,
                      "createConnection concept: Conn* ConnFactory::createConnnection() not satisfied");
        static_assert(is_detected<traits::static_check_valid, Conn, ConnFactory>::value,
                      "checkValid concept: bool ConnFactory::checkValid(Conn*) not satisfied");
        static_assert(is_detected<traits::static_destroy, Conn, ConnFactory>::value,
                      "destroy concept: void ConnFactory::destroy(Conn*) not satisfied");
        return true;
    }
};
}

#endif //CONNECTIONPOOL_CONN_FACTORY_CONCEPT_HPP
