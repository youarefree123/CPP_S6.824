#include <async_simple/coro/SyncAwait.h>
#include <chrono>
#include <memory>
#include <pthread.h>
#include <string>
#include <ylt/coro_rpc/impl/coro_rpc_client.hpp>
#include <ylt/coro_rpc/impl/default_config/coro_rpc_config.hpp>
#include <ylt/easylog.hpp>
#include "raft/raft.h"
#include "raft/common.h"

constexpr auto RaftElectionTimeout = std::chrono::milliseconds(1000);
constexpr auto THREAD_NUM = 4;

static std::vector<int> ports = { 8000, 8001, 8002, 8003, 8004, 8005, 8006, 8007, 8008 };

void initRaft( int nums, std::vector< std::unique_ptr<Raft> >& rafts, std::vector<Server_Ptr>& servers ) {
    
    // 异步开启nums 个 服务器
    for( int i = 0; i < nums; i++ ) {
        servers[i] = std::move( std::make_unique<coro_rpc_server>( THREAD_NUM, ports[i] ) );
        Raft* rf = new Raft( i );
        rf->port = std::to_string( ports[i] );

        // ELOG_DEBUG<<"raft 初始状态:me = "<<rf->me<<"; state = "<<rf->state<<" currentTerm = "<<rf->currentTerm<<" votedFor = "<<rf->votedFor;

        rafts[i] = std::move( std::unique_ptr<Raft>( rf ) );
        servers[i]->register_handler<&Raft::RequestVote> ( rf );
        servers[i]->register_handler<&Raft::test> ( rf );
        
        auto res = servers[i]->async_start();
        assert(res.has_value());
    }

    syncAwait( co_sleep(500) );
    ELOG_INFO << "Raft server 设置完成";

    for( int i = 0; i < nums; i++ ) {
        Make( rafts, i, nullptr );
    }
    ELOG_INFO << "Make设置完成";
}

bool test_ipc() {
    int nums = 3;
    std::vector<std::unique_ptr<Raft>> rafts( nums );
    std::vector<Server_Ptr> servers( nums );
    
    ELOG_INFO << "server 创建完成";
    initRaft( nums, rafts, servers );
    ELOG_INFO << "raft 设置完成";

    // for( int i = 0; i < nums; i++ ) {
    //     for( int j = 0; j < nums; j++ ) {
    //         if( i == j ) continue;
    //         std::string str = std::to_string( i ) + "_访问_"+ std::to_string( j);
    //         coro_rpc_client client;
    //         ELOG_INFO << " try " << i << " conn port "<<  rafts[i]->peers[j];
    //         syncAwait( client.connect( "localhost", rafts[i]->peers[j] ) ); 
    //         async_simple::coro::syncAwait( co_sleep( 1000 ) );
    //         auto ret = syncAwait(client.call<&Raft::test>( str ) );
    //         if( ret.has_value() ) {
    //             ELOG_INFO << ret.value();
    //         }
    //         else {
    //             ELOG_ERROR << "无响应";
    //         }
    //     }
    // }

    syncAwait( co_sleep( 10000 ) ); 
    return true;
}




int main(int argc, char* argv[]){
     easylog::init_log( easylog::Severity::DEBUG, "log", false, false,0,0,true);
    // easylog::set_console(false);
    // easylog::set_min_severity(easylog::Severity::DEBUG);

    test_ipc();
    // if ( !single_test() ) {
    //     ELOG_CRITICAL << "single_test failed.";
    //     exit(1);
    // }
    // ELOG_INFO << "single_test success.";

    return  0;
}