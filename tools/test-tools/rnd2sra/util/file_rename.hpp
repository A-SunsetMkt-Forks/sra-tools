#pragma once

#include <string>
#include <vector>

#if GCC_VERSION <= 6
#include <experimental/filesystem>
#else
#include <filesystem>
#endif

#include <iostream>

using namespace std;

#if GCC_VERSION <= 6
namespace fs = std::experimental::filesystem;
#else
namespace fs = std::filesystem;
#endif

namespace sra_convert {

class FileRename {

    private :
        static bool move_file_to_file( const string& src, const string& dst, bool ignore_err ) {
            bool res = true;
            try {
                fs::rename( src, dst );
            } catch ( fs::filesystem_error& e ) {
                if ( !ignore_err ) {
                    cerr << e.what() << '\n';
                    res = false;
                }
            }
            return res;
        }

        static bool move_file_patterns( vector< string >& args, bool ignore_err )  {
            bool res = true;
            size_t idx = 0;
            size_t count = args . size();
            string& src_head = args[ idx++ ];
            string& dst_head = args[ idx++ ];
            string& ext = args[ idx++ ];
            while ( ( res ) && ( idx < count ) ) {
                string& tail = args[ idx++ ];
                string src = src_head + tail + ext;
                string dst = dst_head + tail + ext;
                res = move_file_to_file( src, dst, ignore_err );
            }
            return res;
        }

    public :
        static bool move_files( vector< string >& args, bool ignore_err ) {
            bool res = false;
            switch( args . size() ) {
                case 0  : if ( ignore_err ) { res = true; } break;
                case 1  : if ( ignore_err ) { res = true; } break;
                case 2  : res = move_file_to_file( args[ 0 ], args[ 1 ], ignore_err ); break;
                default : res = move_file_patterns( args, ignore_err ); break;
            }
            return res;
        }

};

}
