/// MIT License
///
/// Copyright (c) 2026 koniarik
///
/// Permission is hereby granted, free of charge, to any person obtaining a copy
/// of this software and associated documentation files (the "Software"), to deal
/// in the Software without restriction, including without limitation the rights
/// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
/// copies of the Software, and to permit persons to whom the Software is
/// furnished to do so, subject to the following conditions:
///
/// The above copyright notice and this permission notice shall be included in all
/// copies or substantial portions of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
/// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
/// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
/// SOFTWARE.

#include "zll.hpp"

#include <cassert>
#include <fstream>
#include <iostream>
#include <source_location>
#include <sstream>
#include <string_view>

enum class mode
{
        gen,
        run,
        eval
};

void check_impl(
    mode                              m,
    std::string_view                  var,
    [[maybe_unused]] std::string_view expected,
    std::ostream&                     out,
    std::source_location              sl = std::source_location::current() )
{
        if ( m != mode::gen )
                return;
        out << "break " << sl.file_name() << ":" << sl.line() << "\n";
        out << "commands\n";
        out << "p " << var << "\n";
        out << "c\n";
        out << "end\n";
}

void check_impl(
    [[maybe_unused]] mode             m,
    [[maybe_unused]] std::string_view var,
    std::string_view                  expected,
    std::istream&                     in,
    std::source_location              sl = std::source_location::current() )
{
        assert( m == mode::eval );
        while ( in && in.get() != '$' ) {
        }

        std::string line;
        std::getline( in, line );

        // strip "N = " prefix
        auto             sep   = line.find( " = " );
        std::string_view value = sep != std::string::npos ?
                                     std::string_view( line ).substr( sep + 3 ) :
                                     std::string_view( line );

        if ( value == expected )
                return;
        std::cerr << "Failed match:\n";
        std::cerr << "Expected: " << expected << "\n";
        std::cerr << "     Got: " << value << "\n";
        std::cerr << "  Source: " << sl.file_name() << ":" << sl.line() << "\n";
        std::exit( 2 );
}

#define CHECK( m, var, expected, out ) check_impl( m, #var, expected, out )

// ---------------------------------------------------------------------------
// Node types
// ---------------------------------------------------------------------------

namespace
{

// ll_node — intrusive linked-list node, no data payload
struct ll_access
{
        static auto& get( auto& n ) noexcept
        {
                return n.hdr;
        }
};

struct ll_node
{
        zll::ll_header< ll_node, ll_access > hdr;
        using access = ll_access;
};

// sh_node — intrusive skew-heap node with integer key
struct sh_access
{
        static auto& get( auto& n ) noexcept
        {
                return n.hdr;
        }
};

struct sh_node
{
        zll::sh_header< sh_node, sh_access > hdr;
        using access = sh_access;

        int x;

        sh_node( int v = 0 )
          : x( v )
        {
        }

        bool operator<( sh_node const& o ) const noexcept
        {
                return x < o.x;
        }
};

// dual_node — belongs to both an ll_list and an sh_heap simultaneously
struct dual_ll_access;
struct dual_sh_access;

struct dual_node
{
        zll::ll_header< dual_node, dual_ll_access > ll_hdr;
        zll::sh_header< dual_node, dual_sh_access > sh_hdr;

        int x;

        dual_node( int v = 0 )
          : x( v )
        {
        }

        bool operator<( dual_node const& o ) const noexcept
        {
                return x < o.x;
        }
};

struct dual_ll_access
{
        static auto& get( auto& n ) noexcept
        {
                return n.ll_hdr;
        }
};

struct dual_sh_access
{
        static auto& get( auto& n ) noexcept
        {
                return n.sh_hdr;
        }
};

}  // namespace

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void run_tests( mode m, auto& st )
{
        // ---------------------------------------------------------------
        // ll_list / ll_header
        // ---------------------------------------------------------------

        // ll_list: empty
        {
                zll::ll_list< ll_node, ll_access > l;
                CHECK( m, l, "{}", st );
        }

        // ll_header: detached
        {
                ll_node n;
                CHECK( m, n.hdr, "{prev = null, next = null}", st );
        }

        // ll_header: only node in a 1-element list — both sides point to list
        {
                ll_node                            n;
                zll::ll_list< ll_node, ll_access > l{ &n };
                CHECK( m, n.hdr, "{prev = {{...}}, next = {{...}}}", st );
        }

        // ll_header: first in a 2-node list — prev = list, next = node
        {
                ll_node                            n1, n2;
                zll::ll_list< ll_node, ll_access > l{ &n1, &n2 };
                CHECK( m, n1.hdr, "{prev = {{...}, {...}}, next = {hdr = {...}}}", st );
                CHECK( m, n2.hdr, "{prev = {hdr = {...}}, next = {{...}, {...}}}", st );
        }

        // ll_header: first/middle/last in a 3-node list
        {
                ll_node                            n1, n2, n3;
                zll::ll_list< ll_node, ll_access > l{ &n1, &n2, &n3 };
                CHECK( m, n1.hdr, "{prev = {{...}, {...}, {...}}, next = {hdr = {...}}}", st );
                CHECK( m, n2.hdr, "{prev = {hdr = {...}}, next = {hdr = {...}}}", st );
                CHECK( m, n3.hdr, "{prev = {hdr = {...}}, next = {{...}, {...}, {...}}}", st );
        }

        // ll_header: nodes connected without ll_list — prev/next = null on ends
        {
                ll_node n1, n2;
                zll::link_detached_as_next< ll_node, ll_access >( n1, n2 );
                CHECK( m, n1.hdr, "{prev = null, next = {hdr = {...}}}", st );
                CHECK( m, n2.hdr, "{prev = {hdr = {...}}, next = null}", st );
                // detach manually so destructors don't assert
                zll::detach< ll_node, ll_access >( n2 );
        }

        // ---------------------------------------------------------------
        // sh_heap / sh_header
        // ---------------------------------------------------------------

        // sh_heap: empty
        {
                zll::sh_heap< sh_node, sh_access > h;
                CHECK( m, h, "{}", st );
        }

        // sh_header: detached
        {
                sh_node n;
                CHECK( m, n.hdr, "{parent = null, left = null, right = null}", st );
        }

        // sh_header: top of a 1-node heap — parent = heap, left/right = null
        {
                zll::sh_heap< sh_node, sh_access > h;
                sh_node                            n( 1 );
                h.link( n );
                CHECK( m, n.hdr, "{parent = {{...}}, left = null, right = null}", st );
        }

        // sh_header: top and child in a 2-node heap (n1=1 < n2=2)
        {
                zll::sh_heap< sh_node, sh_access > h;
                sh_node                            n1( 1 ), n2( 2 );
                h.link( n1 );
                h.link( n2 );
                CHECK(
                    m,
                    n1.hdr,
                    "{parent = {{...}, {...}}, left = {hdr = {...}, x = 2}, right = null}",
                    st );
                CHECK(
                    m, n2.hdr, "{parent = {hdr = {...}, x = 1}, left = null, right = null}", st );
        }

        // sh_heap: 1-node printer output
        {
                zll::sh_heap< sh_node, sh_access > h;
                sh_node                            n( 42 );
                h.link( n );
                CHECK( m, h, "{{hdr = {...}, x = 42}}", st );
        }

        // sh_heap: 10 nodes — in-order DFS ends with x=6
        {
                zll::sh_heap< sh_node, sh_access > h;
                sh_node                            nodes[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
                for ( auto& n : nodes )
                        h.link( n );
                CHECK( m, h, "{{hdr = {...}, x = 10}, {hdr = {...}, x = 6}}", st );
        }

        // dual: node simultaneously in ll_list and sh_heap
        {
                dual_node                                 n1( 1 ), n2( 2 ), n3( 3 );
                zll::ll_list< dual_node, dual_ll_access > l{ &n1, &n2, &n3 };
                zll::sh_heap< dual_node, dual_sh_access > h{ &n1, &n2, &n3 };
                CHECK(
                    m,
                    n1.ll_hdr,
                    "{prev = {{...}, {...}, {...}}, next = {ll_hdr = {...}, sh_hdr = {...}, x = 2}}",
                    st );
                CHECK(
                    m,
                    n1.sh_hdr,
                    "{parent = {{...}, {...}}, left = {ll_hdr = {...}, sh_hdr = {...}, x = 3}, right = {ll_hdr = {...}, sh_hdr = {...}, x = 2}}",
                    st );
        }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main( [[maybe_unused]] int argc, char* argv[] )
{
        assert( argc >= 2 );
        std::string_view mode_str = argv[1];

        if ( mode_str == "gen" ) {
                assert( argc >= 5 );
                std::string_view pprinter   = argv[2];
                std::string_view gdb_script = argv[3];
                std::string_view log_path   = argv[4];
                std::ofstream    out{ std::string( gdb_script ) };
                out << "source " << pprinter << "\n";
                out << "set logging file " << log_path << "\n";
                out << "set logging overwrite on\n";
                out << "set logging redirect on\n";
                out << "set logging enabled on\n";
                out << "set width 0\n";
                out << "set print max-depth 2\n";
                run_tests( mode::gen, out );
                out << "run run\n";
        } else if ( mode_str == "run" ) {
                std::ostringstream ss;
                run_tests( mode::run, ss );
        } else if ( mode_str == "eval" ) {
                assert( argc >= 3 );
                std::ifstream inpt{ argv[2] };
                run_tests( mode::eval, inpt );
        } else {
                std::cerr << "invalid mode\n";
                return 1;
        }
}
