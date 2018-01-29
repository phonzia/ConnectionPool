//
// Created by dx2880 on 2018/1/13.
//

#ifndef CONNECTIONPOOL_CONNECTION_POOL_HPP
#define CONNECTIONPOOL_CONNECTION_POOL_HPP

#include <deque>
#include <mutex>
#include <chrono>
#include <thread>
#include <condition_variable>
#include "conn_factory_concept.hpp"
#include "conn_guard.hpp"

namespace modern_utils {
template<typename Conn, typename ConnFactory>
class ConnectionPool {
private:
    static_assert(is_acceptable<Conn, ConnFactory>::diagnose());
public:
    using ConnectionType = Conn;
    using ConnFactoryType = ConnFactory;
public:
    explicit ConnectionPool(const std::shared_ptr<ConnFactoryType> &conn_factory, int max_count = 20) : conn_factory_(
            conn_factory), max_count_(max_count) {
        initPool();
    }


    explicit ConnectionPool(std::shared_ptr<ConnFactoryType> &&conn_factory, int max_count = 20) : conn_factory_(
            std::move(conn_factory)), max_count_(20) {
        initPool();
    }

    ~ConnectionPool() {
        std::lock_guard<std::mutex> guard(mutex_);
        idle_connection_.clear();
    }

    ConnectionPool(const ConnectionPool &rhs) = delete;

    ConnectionPool &operator=(const ConnectionPool &rhs) = delete;

    void setConnectionCount(int count) {
        std::lock_guard<std::mutex> guard(mutex_);
        max_count_ = count;
        auto old_idle_count = idle_count_;
        auto new_idle_count = 0;
        if (idle_count_ + busy_count_ > max_count_) {
            new_idle_count = max_count_ - busy_count_ > 0 ? max_count_ - busy_count_ : 0;
        }

        auto differ_count = new_idle_count - old_idle_count;
        if (differ_count > 0) {
            for (int i = 0; i < differ_count; ++i) {
                auto conn = std::shared_ptr<Conn>(conn_factory_->createConnection(),
                                                  [conn_factory = conn_factory_](Conn *p) {
                                                      conn_factory->destroy(p);
                                                  });
                idle_connection_.push_back({conn, time(nullptr)});
                ++idle_count_;
            }
        } else if (differ_count < 0) {
            for (int i = 0; i < abs(differ_count) && !idle_connection_.empty(); ++i) {
                idle_connection_.pop_back();
                --idle_count_;
            }
        }
    }

    auto getConnection() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, std::chrono::seconds(timeout_),
                     [this] { return idle_count_ > 0 || idle_count_ + busy_count_ < max_count_; });
        if (idle_count_ > 0) {
            --idle_count_;
            ++busy_count_;
            auto conn = idle_connection_.back().first;
            idle_connection_.pop_back();
            return conn;
        } else if (idle_count_ + busy_count_ < max_count_) {
            auto conn = std::shared_ptr<Conn>(conn_factory_->createConnection(),
                                              [conn_factory = conn_factory_](Conn *p) { conn_factory->destroy(p); });
            ++busy_count_;
            return conn;
        }

        throw std::runtime_error("getConnection timeout");
    }

    auto recoverConnection() {
        auto conn = std::shared_ptr<Conn>(conn_factory_->createConnection(),
                                          [conn_factory = conn_factory_](Conn *p) { conn_factory->destroy(p); });

        return conn;
    }

    void releaseConnecion(const std::shared_ptr<Conn> &conn, bool destroy = false) {
        std::lock_guard<std::mutex> guard(mutex_);
        if (idle_count_ + busy_count_ <= max_count_ && !destroy) {
            idle_connection_.push_back({conn, time(nullptr)});
            ++idle_count_;
            --busy_count_;
        } else {
            --busy_count_;
        }

        cv_.notify_one();
    }

private:
    void initPool() {
        for (int i = 0; i < max_count_; ++i) {
            auto conn = std::shared_ptr<Conn>(conn_factory_->createConnection(),
                                              [conn_factory = conn_factory_](Conn *p) { conn_factory->destroy(p); });
            idle_connection_.push_back({conn, time(nullptr)});
            ++idle_count_;
        }

        connection_checker_ = std::make_shared<std::thread>([this] {
            while (checking_) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                std::lock_guard<std::mutex> guard(mutex_);
                auto now = time(nullptr);
                if (idle_count_ > 0 && idle_count_ + busy_count_ > 1) {
                    for (auto iter = idle_connection_.begin(); iter != idle_connection_.end() - 1; ++iter) {
                        if (now - iter->second >= max_idle_time_) {
                            --idle_count_;
                            idle_connection_.pop_front();
                        }
                    }
                }
            }
        });
    }

private:
    std::shared_ptr<ConnFactoryType> conn_factory_;
public:
    const std::shared_ptr<ConnFactory> &getConnFactory() const {
        return conn_factory_;
    }

private:
    std::deque<std::pair<std::shared_ptr<Conn>, time_t> > idle_connection_;
    int max_count_{20};
    int idle_count_{0};
    int busy_count_{0};
    int timeout_{3};
    int max_idle_time_{300};
public:
    void setMaxIdleTime(int max_idle_time) {
        max_idle_time_ = max_idle_time;
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    std::shared_ptr<std::thread> connection_checker_;
    bool checking_{true};
};
};

#endif //CONNECTIONPOOL_CONNECTION_POOL_HPP
