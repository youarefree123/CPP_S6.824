
#include "wc.h"
// #include <iostream> // FIXME
/**

filename: 文件名：统计单词中并不会用到
contents: 文件内容，非常大

*/
std::vector<KeyValue> mapTask( std::string_view filename, std::string_view contents ) {
    std::vector<KeyValue> ret; // 存放
    size_t len = contents.size();
    size_t p = 0, q = 0;

    auto check = []( const char& ch ) -> bool {
        return ( ch >= 'A' && ch <= 'Z' ) || ( ch >= 'a' && ch <= 'z' );  
    };

    // std::cout<<" 进入map "<<std::endl; 
    // 分割word
    while( q < len ) {
        while( q < len && check( contents[q] ) ) { ++q; }
        if( p < q ) {
            ret.emplace_back( std::string{ contents.substr( p, q-p ) }, "1" );
        }
        p = q+1; q = p;
    }

    return ret;
}