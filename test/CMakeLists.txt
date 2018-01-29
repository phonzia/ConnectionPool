set(CMAKE_CXX_FLAGS "-std=c++14 -O0 -g")
add_executable(pool_test test.cpp
        ../src/conn_guard.hpp ../src/connection_pool.hpp ../src/static_detected.hpp ../src/conn_factory_concept.hpp)