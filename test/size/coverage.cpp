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

// Every list and heap function, instantiated for a node with its own accessor and for a node using
// the CRTP base. Compiled with the warning flags of the CI presets at each optimization level, so a
// warning that only the embedded compiler reports fails the build. Object file only, never linked.

#include <cstddef>
#include <functional>
#include <utility>
#include <zll.hpp>

struct list_node
{
        struct access
        {
                static auto& get( list_node& n ) noexcept
                {
                        return n.hdr;
                }

                static auto& get( list_node const& n ) noexcept
                {
                        return n.hdr;
                }

                static list_node& node( zll::ll_header< list_node, access >& h ) noexcept
                {
                        void* p = reinterpret_cast< char* >( &h ) - offsetof( list_node, hdr );
                        return *static_cast< list_node* >( p );
                }
        };

        int                                 v = 0;
        zll::ll_header< list_node, access > hdr;

        friend bool operator<( list_node const& a, list_node const& b ) noexcept
        {
                return a.v < b.v;
        }

        friend bool operator==( list_node const& a, list_node const& b ) noexcept
        {
                return a.v == b.v;
        }
};

struct list_base_node : zll::ll_base< list_base_node >
{
        int v = 0;

        friend bool operator<( list_base_node const& a, list_base_node const& b ) noexcept
        {
                return a.v < b.v;
        }

        friend bool operator==( list_base_node const& a, list_base_node const& b ) noexcept
        {
                return a.v == b.v;
        }
};

struct heap_node
{
        struct access
        {
                static auto& get( heap_node& n ) noexcept
                {
                        return n.hdr;
                }

                static auto& get( heap_node const& n ) noexcept
                {
                        return n.hdr;
                }

                static heap_node& node( zll::sh_header< heap_node, access >& h ) noexcept
                {
                        return *static_cast< heap_node* >( static_cast< void* >( &h ) );
                }
        };

        zll::sh_header< heap_node, access > hdr;
        int                                 v = 0;

        friend bool operator<( heap_node const& a, heap_node const& b ) noexcept
        {
                return a.v < b.v;
        }
};

struct heap_base_node : zll::sh_base< heap_base_node >
{
        int v = 0;

        friend bool operator<( heap_base_node const& a, heap_base_node const& b ) noexcept
        {
                return a.v < b.v;
        }
};

template < typename N >
void list_functions( N& a, N& b, N& c )
{
        zll::ll_list< N > l{ &a }, m;
        l.link_back( b );
        l.link_front( c );
        m.link_back( c );
        l.sort();
        m.sort( std::less<>{} );
        l.merge( std::move( m ) );
        l.merge( std::move( m ), std::less<>{} );
        l.splice( l.end(), std::move( m ) );
        l.reverse();
        (void) l.remove( a );
        (void) l.remove_if( []( N& ) noexcept {
                return false;
        } );
        (void) l.unique();
        (void) l.unique( std::equal_to<>{} );

        auto it = l.begin();
        ++it;
        (void) it++;
        (void) it.get();
        (void) ( it == l.end() );
        for ( N& n : l )
                (void) n;
        auto const&                                     cl  = l;
        zll::ll_const_iterator< N, typename N::access > cit = it;
        (void) ( cit == cl.cend() );
        for ( N const& n : cl )
                (void) n;

        (void) l.front();
        (void) l.back();
        (void) cl.front();
        (void) cl.back();
        (void) l.empty();
        l.detach_front();
        l.detach_back();
        (void) l.take_front();
        (void) l.take_back();
        zll::ll_list< N > k = std::move( l );
        l                   = std::move( k );

        zll::detach( c );
        (void) zll::detached( c );
        zll::link_detached_as_next( a, c );
        zll::link_detached_as_prev( a, c );
        zll::link_detached_as_last( a, c );
        zll::link_detached_as_first( a, c );
        zll::detach_range( c, c );
        (void) zll::detached_range( c, c );
        zll::link_range_as_next( a, c, c );
        zll::link_range_as_prev( a, c, c );
        zll::move_from_to( b, c );
        zll::link_group< N >( { &a, &b } );
        (void) zll::merge_ranges< N, typename N::access >( a, a, b, b );
        (void) zll::first_node_of( a );
        (void) zll::last_node_of( a );
        zll::for_each_node( a, []( N& ) noexcept {} );
        (void) zll::find_if_node( a, []( N& ) noexcept {
                return false;
        } );
}

template < typename N >
void heap_functions( N& a, N& b, N& c, N& d )
{
        zll::sh_heap< N > h{ &a }, g;
        h.link( b );
        g.link( c );
        h.merge( std::move( g ) );
        h.pop();
        (void) h.top();
        auto const& ch = h;
        (void) ch.top();
        (void) h.take();
        (void) h.empty();
        zll::sh_heap< N > k = std::move( h );
        h                   = std::move( k );

        zll::link_detached( a, c );
        zll::link_detached_to( a, d );
        zll::detach( d, std::less<>{} );
        zll::move_from_to( c, d );
        (void) zll::detached( b );
        (void) zll::top_node_of( c );
        zll::inorder_traverse( a, []( N& ) noexcept {} );
        zll::preorder_traverse( a, []( N& ) noexcept {} );
        zll::postorder_traverse( a, []( N& ) noexcept {} );
}

template void list_functions< list_node >( list_node&, list_node&, list_node& );
template void list_functions< list_base_node >( list_base_node&, list_base_node&, list_base_node& );
template void heap_functions< heap_node >( heap_node&, heap_node&, heap_node&, heap_node& );
template void heap_functions< heap_base_node >(
    heap_base_node&,
    heap_base_node&,
    heap_base_node&,
    heap_base_node& );

void base_node_functions( list_base_node& l, heap_base_node& h )
{
        list_base_node l2{ l };
        list_base_node l3{ std::move( l2 ) };
        l2 = std::move( l3 );
        l3 = l;
        l.link_next( l2 );
        l.link_prev( l3 );
        (void) l.next();
        (void) l.prev();
        auto const& cl = l;
        (void) cl.next();
        (void) cl.prev();

        heap_base_node h2{ h };
        heap_base_node h3{ std::move( h2 ) };
        h2 = std::move( h3 );
        h3 = h;
}
