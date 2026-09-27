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

// Size probe for the walking code: iteration and the list algorithms, for ZLL_PROBE_NODES
// distinct node types whose header sits behind a payload. Object file only, never linked.

#include <cstddef>
#include <cstdint>
#include <utility>
#include <zll.hpp>

void* probe_source() noexcept;
void  probe_sink( void* ) noexcept;
void  probe_sink( std::size_t ) noexcept;

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

        std::uint8_t                   payload[I % 5 + 1];
        zll::ll_header< item, access > hdr;

        friend bool operator<( item const& a, item const& b ) noexcept
        {
                return a.payload[0] < b.payload[0];
        }

        friend bool operator==( item const& a, item const& b ) noexcept
        {
                return a.payload[0] == b.payload[0];
        }
};

template < int I >
[[gnu::noinline]] void algos()
{
        using N = item< I >;

        zll::ll_list< N > l, m;
        l.link_back( *static_cast< N* >( probe_source() ) );
        m.link_back( *static_cast< N* >( probe_source() ) );

        for ( N& n : l )
                probe_sink( &n );
        l.sort();
        l.merge( std::move( m ) );
        probe_sink( l.remove_if( []( N& n ) noexcept {
                return n.payload[0] == 0;
        } ) );
        probe_sink( l.unique() );
        l.reverse();
        l.splice( l.end(), std::move( m ) );

        N& x = l.front();
        zll::for_each_node( x, []( N& n ) noexcept {
                probe_sink( &n );
        } );
        probe_sink( zll::find_if_node( x, []( N& n ) noexcept {
                return n.payload[0] == 1;
        } ) );
        probe_sink( &zll::first_node_of( x ) );
        probe_sink( &zll::last_node_of( x ) );
}

template < int... Is >
void all( std::integer_sequence< int, Is... > )
{
        ( algos< Is >(), ... );
}

}  // namespace

void zll_size_probe()
{
        all( std::make_integer_sequence< int, ZLL_PROBE_NODES >{} );
}
