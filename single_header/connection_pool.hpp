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

/*** Start of inlined file: conn_factory_concept.hpp ***/
//
// Created by dx2880 on 2018/1/16.
//

#ifndef CONNECTIONPOOL_CONN_FACTORY_CONCEPT_HPP
#define CONNECTIONPOOL_CONN_FACTORY_CONCEPT_HPP


/*** Start of inlined file: static_detected.hpp ***/
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

/*** End of inlined file: static_detected.hpp ***/

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

/*** End of inlined file: conn_factory_concept.hpp ***/



/*** Start of inlined file: conn_guard.hpp ***/
//
// Created by dx2880 on 2018/1/13.
//

#ifndef CONNECTIONPOOL_CONNECTION_HPP
#define CONNECTIONPOOL_CONNECTION_HPP

#include <functional>
#include <iostream>
#include <memory>

/*** Start of inlined file: connection_pool.hpp ***/
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

/*** End of inlined file: connection_pool.hpp ***/


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

/*** End of inlined file: conn_guard.hpp ***/

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

