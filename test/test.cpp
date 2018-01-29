#include <iostream>
#include "../src/conn_guard.hpp"
#include "../src/connection_pool.hpp"

using namespace std;
using namespace modern_utils;

class FakeConnection {
public:
    FakeConnection() : count(s_count++) {}

    void print() {
        std::cout << "test fake connection" << count << std::endl;
    }

private:
    int count{0};
private:
    static int s_count;
};

int FakeConnection::s_count = 0;

class FakeConnFactory {
public:
    FakeConnFactory(int arg) : arg_(arg) {}

    auto createConnection() { return new FakeConnection; }

    bool checkValid(FakeConnection *conn) { return conn == nullptr; }

    void destroy(FakeConnection *conn) {
        delete conn;
        std::cout << "connection destroyed" << std::endl;
    }

private:
    int arg_{0};
};


int main() {
    using ConnectionPool = ConnectionPool<FakeConnection, FakeConnFactory>;

    auto pool = std::make_shared<ConnectionPool>(make_shared<FakeConnFactory>(10));
    pool->setMaxIdleTime(20);
    std::thread thread([=] {
        ConnGuard<ConnectionPool> conn(pool);
        conn->print();
        conn.recover();
        conn->print();
    });
    {
        ConnGuard<ConnectionPool> conn(pool);
        conn->print();
        conn.recover();
        conn->print();
    }
    thread.join();
    char c;
    std::cin >> c;
}