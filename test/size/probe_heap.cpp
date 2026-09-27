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

// Size probe for the skew heap, for ZLL_PROBE_NODES distinct node types whose header sits behind a
// payload. Object file only, never linked.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
#include <zll.hpp>

void* probe_source() noexcept;
void  probe_sink( void* ) noexcept;
void  probe_sink( bool ) noexcept;

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

                template < typename N, typename C >
                static N& node( zll::sh_header< N, access, C >& h ) noexcept
                {
                        void* p = reinterpret_cast< char* >( &h ) - offsetof( N, hdr );
                        return *static_cast< N* >( p );
                }
        };

        std::uint8_t                   payload[I % 5 + 1];
        zll::sh_header< item, access > hdr;

        friend bool operator<( item const& a, item const& b ) noexcept
        {
                return a.payload[0] < b.payload[0];
        }
};

template < int I >
[[gnu::noinline]] void heap_ops()
{
        using N = item< I >;

        zll::sh_heap< N > h, g;
        N&                a = *static_cast< N* >( probe_source() );
        N&                b = *static_cast< N* >( probe_source() );
        N&                c = *static_cast< N* >( probe_source() );
        h.link( a );
        h.link( b );
        g.link( c );
        h.merge( std::move( g ) );
        probe_sink( &h.take() );
        h.pop();
        probe_sink( h.empty() );

        N& d = *static_cast< N* >( probe_source() );
        zll::link_detached( a, d );
        N e;
        zll::move_from_to( d, e );
        zll::detach( e, std::less<>{} );
        zll::inorder_traverse( a, []( N& n ) noexcept {
                probe_sink( &n );
        } );
        zll::preorder_traverse( a, []( N& n ) noexcept {
                probe_sink( &n );
        } );
        zll::postorder_traverse( a, []( N& n ) noexcept {
                probe_sink( &n );
        } );
        probe_sink( &zll::top_node_of( a ) );
}

template < int... Is >
void all( std::integer_sequence< int, Is... > )
{
        ( heap_ops< Is >(), ... );
}

}  // namespace

void zll_size_probe()
{
        all( std::make_integer_sequence< int, ZLL_PROBE_NODES >{} );
}
