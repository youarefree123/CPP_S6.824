#pragma once 
#include <async_simple/coro/Lazy.h>
#include <chrono>
#include <ylt/coro_io/coro_io.hpp>
// #include "raft/raft.h"
// #include "raft/persister.h"

class Raft;
class Persister;


constexpr size_t GAP_TIME = 5; // 每次检测状态的时间
constexpr size_t DURATION_TIME = 100; // 异步超时时间
constexpr size_t ELECTION_BASE_TIME = 150; // 选举超时时间
constexpr size_t ELECTION_RANGE_TIME = 150; // 选举超时的随机值
constexpr size_t HEARTBEAT_TIME = 50; // 发送心跳包的间隔时间



#define milli_duration(time) std::chrono::duration< int, std::milli >(time) 


inline size_t Now() {
    auto cur = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(cur.time_since_epoch());
    return duration.count();
}

// inline async_simple::coro::Lazy<void> co_sleep( size_t time ) {
//     co_await coro_io::sleep_for( std::chrono::duration< int, std::milli >( time ) );
// }

inline async_simple::coro::Lazy<void> co_sleep( size_t time ) {
    co_await coro_io::sleep_for( milli_duration( time ) );
}