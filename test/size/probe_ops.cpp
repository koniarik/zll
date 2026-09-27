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

// Size probe for the linking operations. Every node type is used by ZLL_PROBE_SITES separate
// functions, each running the same operations, as different parts of a program would. Object file
// only, never linked.

#include <cstddef>
#include <cstdint>
#include <utility>
#include <zll.hpp>

// Never defined: the compiler cannot see through them, so nothing is optimized away.
void* probe_source() noexcept;
void  probe_sink( void* ) noexcept;
void  probe_sink( bool ) noexcept;
void  probe_mark( int ) noexcept;

namespace
{

template < int I >
struct item
{
        struct access
        {
                static auto& get( item& n ) noexcept
                {
                        return n.hdr;
                }

                static auto& get( item const& n ) noexcept
                {
                        return n.hdr;
                }

                template < typename N >
                static N& node( zll::ll_header< N, access >& h ) noexcept
                {
                        void* p = reinterpret_cast< char* >( &h ) - offsetof( N, hdr );
                        return *static_cast< N* >( p );
                }
        };

        // payload sizes differ, so no two node types share a layout and the header offset varies
        std::uint8_t                   payload[I % 5 + 1];
        zll::ll_header< item, access > hdr;
};

// One place in the program that uses lists of item<I>. `probe_mark( K )` keeps the sites distinct
// so the compiler cannot fold them into one function.
template < int I, int K >
[[gnu::noinline]] void site()
{
        probe_mark( K );
        using N = item< I >;

        N& a = *static_cast< N* >( probe_source() );
        N& b = *static_cast< N* >( probe_source() );
        N  local;

        zll::ll_list< N > l;
        l.link_back( a );
        l.link_back( b );
        l.link_back( local );
        probe_sink( l.empty() );
        probe_sink( &l.front() );

        zll::detach( a );
        probe_sink( zll::detached( a ) );
        probe_sink( &l.take_front() );

        l.link_front( a );
        zll::ll_list< N > m = std::move( l );
        probe_sink( m.empty() );

        N& c = *static_cast< N* >( probe_source() );
        zll::detach( c );
        zll::link_detached_as_next( b, c );
        N moved;
        zll::move_from_to( c, moved );
        zll::link_detached_as_prev( moved, c );
        probe_sink( zll::detached( c ) );
}

template < int I, int... Ks >
void sites( std::integer_sequence< int, Ks... > )
{
        ( site< I, Ks >(), ... );
}

template < int... Is >
void all( std::integer_sequence< int, Is... > )
{
        ( sites< Is >( std::make_integer_sequence< int, ZLL_PROBE_SITES >{} ), ... );
}

}  // namespace

void zll_size_probe()
{
        all( std::make_integer_sequence< int, ZLL_PROBE_NODES >{} );
}
