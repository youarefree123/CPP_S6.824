#include "wc.h"
#include <string>
#include <ylt/easylog.hpp>

/**

filename: 文件名：统计单词中并不会用到
contents: 文件内容，非常大

*/
std::vector<KeyValue> mapTask( std::string_view filename, std::string_view contents ) {
    std::vector<KeyValue> ret; // 存放
    
    // ELOG_DEBUG << " Map the file " << filename <<" content size = " << contents.size() ;

    auto check = []( const char& ch ) -> bool {
        return ( ch >= 'A' && ch <= 'Z' ) || ( ch >= 'a' && ch <= 'z' );  
    };

    // 分割word
    size_t len = contents.size();
    size_t p = 0, q = 0;
    while( q <= len ) {
        while( q < len && check( contents[q] ) ) { ++q; }
        if( p < q ) {
            // ret.push_back( KeyValue{ ( std::string )contents.substr( p, q-p ), "1" } );
            ret.emplace_back( std::string{ contents.substr( p, q-p ) }, "1" );
        }
        p = q+1; q = p;

        // ELOG_DEBUG << " ret insert " << ret.back().key << " : "<< ret.back().value << " q = " << q << " len = " << contents.size();

    }

    ELOG_DEBUG << " map 完成 " ; 

    return ret;
}

/**

key : 单词
value：由1组成的字符串，长度就代表单词出现次数

*/
std::string reduceTask( std::string_view key, std::string_view value ) {
    return std::to_string( value.size() );
}