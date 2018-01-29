//
// Created by dx2880 on 2018/1/13.
//

#ifndef CONNECTIONPOOL_CONNECTION_HPP
#define CONNECTIONPOOL_CONNECTION_HPP

#include <functional>
#include <iostream>
#include <memory>
#include "connection_pool.hpp"

namespace modern_utils {

template<typename ConnectionPool>
class ConnGuard {
public:
    using ConnectionType = typename ConnectionPool::ConnectionType;
    using ConnFactoryType = typename ConnectionPool::ConnFactoryType;
public:
    explicit ConnGuard(const std::shared_ptr<ConnectionPool> &pool) : pool_(pool) {
        auto _pool = pool_.lock();
        if(_pool) {
            conn_ = _pool->getConnection();
        }
    }

    explicit ConnGuard(std::shared_ptr<ConnectionPool> &&pool) : pool_(
            std::move(pool)) {
        auto _pool = pool_.lock();
        if(_pool) {
            conn_ = _pool->getConnection();
        }
    }

    ConnGuard(const ConnGuard &rhs) = delete;

    ConnGuard &operator=(const ConnGuard &rhs) = delete;

    virtual ~ConnGuard() {
        auto _pool = pool_.lock();
        if(_pool && conn_ != nullptr) {
            _pool->releaseConnecion(conn_);
        }
    }

    auto operator->() {
        if (isReady()) {
            return conn_;
        } else {
            throw std::runtime_error("connection not initialized");
        }

        return decltype(conn_)();
    }

    bool isReady() {
        return conn_ != nullptr;
    }

    virtual bool checkValid() {
        auto _pool = pool_.lock();
        if(_pool && conn_ != nullptr) {
            return _pool->getConnFactory()->checkValid(conn_.get());
        }

        return false;
    }

    auto get() {
        return conn_.get();
    }

    void recover() {
        auto _pool = pool_.lock();
        if(_pool && conn_ != nullptr) {
            conn_ = _pool->recoverConnection();
        } else {
            conn_ = nullptr;
        }
    }
private:
    virtual void log_error(const char *error) { std::cout << error << std::endl; };
private:
    std::shared_ptr<ConnectionType> conn_;
    std::weak_ptr<ConnectionPool> pool_;
};
};

#endif //CONNECTIONPOOL_CONNECTION_HPP
