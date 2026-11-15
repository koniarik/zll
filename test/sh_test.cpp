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

#include <algorithm>
#include <cstdint>
#include <doctest/doctest.h>
#include <functional>
#include <iostream>
#include <list>
#include <optional>
#include <random>
#include <set>
#include <stdexcept>
#include <vector>

namespace zll
{
namespace
{
struct hdr_access
{
        static auto& get( auto& item ) noexcept
        {
                return item.hdr;
        }

        // The header is the first member of a standard-layout node, so both share an address.
        template < typename T, typename Compare >
        static T& node( sh_header< T, hdr_access, Compare >& h ) noexcept
        {
                return *static_cast< T* >( static_cast< void* >( &h ) );
        }
};

struct node_t
{
        sh_header< node_t, hdr_access > hdr;

        using access = hdr_access;

        int x;

        node_t( int v = 0 )
          : x( v )
        {
        }

        node_t( node_t&& o ) noexcept
          : x( o.x )
        {
                move_from_to< node_t, hdr_access >( o, *this );
        }

        node_t( node_t& o ) noexcept
          : x( o.x )
        {
                link_detached_to< node_t, hdr_access >( o, *this, std::less<>{} );
        }

        node_t& operator=( node_t&& o ) noexcept
        {
                detach< node_t, hdr_access >( *this, std::less<>{} );
                x = o.x;
                move_from_to< node_t, hdr_access >( o, *this );
                return *this;
        }

        node_t& operator=( node_t& o ) noexcept
        {
                detach< node_t, hdr_access >( *this, std::less<>{} );
                x = o.x;
                link_detached_to< node_t, hdr_access >( o, *this, std::less<>{} );
                return *this;
        }

        bool operator<( node_t const& other ) const noexcept
        {
                return x < other.x;
        }

        ~node_t()
        {
                detach< node_t, hdr_access >( *this, std::less<>{} );
        }
};

static_assert( std::is_standard_layout_v< node_t > );

struct der : public sh_base< der >
{
        int x;

        der( int v = 0 )
          : x( v )
        {
        }

        bool operator<( der const& other ) const noexcept
        {
                return x < other.x;
        }
};

// The header sits behind other data, so mapping it back to its node has to undo an offset.
struct off_access
{
        static auto& get( auto& item ) noexcept
        {
                return item.hdr;
        }

        template < typename T, typename Compare >
        static T& node( sh_header< T, off_access, Compare >& h ) noexcept
        {
                void* p = reinterpret_cast< char* >( &h ) - offsetof( T, hdr );
                return *static_cast< T* >( p );
        }
};

struct off_node
{
        char                              pad[7] = {};
        sh_header< off_node, off_access > hdr;
        int                               x;

        using access = off_access;

        off_node( int v = 0 )
          : x( v )
        {
        }

        off_node( off_node&& o ) noexcept
          : x( o.x )
        {
                move_from_to< off_node, off_access >( o, *this );
        }

        off_node( off_node& o ) noexcept
          : x( o.x )
        {
                link_detached_to< off_node, off_access >( o, *this, std::less<>{} );
        }

        off_node& operator=( off_node&& o ) noexcept
        {
                detach< off_node, off_access >( *this, std::less<>{} );
                x = o.x;
                move_from_to< off_node, off_access >( o, *this );
                return *this;
        }

        off_node& operator=( off_node& o ) noexcept
        {
                detach< off_node, off_access >( *this, std::less<>{} );
                x = o.x;
                link_detached_to< off_node, off_access >( o, *this, std::less<>{} );
                return *this;
        }

        bool operator<( off_node const& other ) const noexcept
        {
                return x < other.x;
        }

        ~off_node()
        {
                detach< off_node, off_access >( *this, std::less<>{} );
        }
};

// `sh_base` behind a polymorphic base, so its header is not at the start of the node either.
struct poly_base
{
        virtual ~poly_base() = default;
        char tag             = 0;
};

struct off_der : poly_base, sh_base< off_der >
{
        int x;

        off_der( int v = 0 )
          : x( v )
        {
        }

        bool operator<( off_der const& other ) const noexcept
        {
                return x < other.x;
        }
};

template < typename T, typename Acc = typename T::access >
void check_links( T& node )
{

        auto& h = Acc::get( node );

        if ( h._left ) {
                CHECK( h._left->_parent == h );
                check_links( _node< T, Acc >( *h._left ) );
        }
        if ( h._right ) {
                CHECK( h._right->_parent == h );
                check_links( _node< T, Acc >( *h._right ) );
        }
}

std::ostream& operator<<( std::ostream& os, std::set< node_t const* > const& value )
{
        os << "{ ";
        for ( auto* v : value ) {
                auto& hdr = node_t::access::get( *v );
                os << (void const*) v << "(" << (void const*) hdr._left << ", "
                   << (void const*) hdr._right << ") ";
        }
        os << "}";
        return os;
}

std::ostream& operator<<( std::ostream& os, std::set< der const* > const& value )
{
        os << "{ ";
        for ( auto* v : value )
                os << (void const*) v << "(" << (void const*) der::access::get( *v )._left << ", "
                   << (void const*) der::access::get( *v )._right << ") ";
        os << "}";
        return os;
}

template < typename T, typename Acc = typename T::access >
void check_for_each_node( T& node, std::set< T const* > expected )
{
        std::set< T const* > s;
        inorder_traverse( node, [&]( T& m ) {
                s.insert( &m );
        } );
        CHECK_EQ( s, expected );
}

TEST_CASE_TEMPLATE( "single", T, node_t, der, off_node, off_der )
{
        using access = typename T::access;

        T d1{ 1 };

        SUBCASE( "nothing" )
        {
        }
        SUBCASE( "heap" )
        {
                sh_heap< T, access > l;
                l.link( d1 );

                CHECK_EQ( l.top(), &d1 );
        }

        check_links( d1 );
        check_for_each_node( d1, { &d1 } );
}

TEST_CASE_TEMPLATE( "dual", T, node_t, der, off_node, off_der )
{
        using access = typename T::access;

        T d1{ 1 }, d2{ 2 };

        SUBCASE( "link" )
        {
                link_detached< T, typename T::access >( d1, d2 );
                check_for_each_node( d1, { &d1, &d2 } );
        }
        SUBCASE( "move" )
        {
                sh_heap< T, access > h;
                h.link( d1 );
                h.link( d2 );
                T d3{ std::move( d1 ) };
                CHECK_EQ( h.top(), &d3 );
                check_for_each_node( d3, { &d2, &d3 } );
        }
        check_links( d1 );
}

TEST_CASE_TEMPLATE( "triple", T, node_t, der, off_node, off_der )
{
        T d1{ 1 }, d2{ 2 }, d3{ 3 };
        link_detached< T, typename T::access >( d1, d2 );
        link_detached< T, typename T::access >( d2, d3 );
        check_links( d1 );
        check_for_each_node( d1, { &d1, &d2, &d3 } );

        d2 = T{ 2 };
        check_links( d1 );
        check_for_each_node( d1, { &d1, &d3 } );
}

TEST_CASE_TEMPLATE( "cpy", T, node_t, der, off_node, off_der )
{
        T d1{ 1 };
        T d2{ d1 };

        SUBCASE( "two" )
        {
                check_links( d1 );
                check_for_each_node( d1, { &d1, &d2 } );
        }

        SUBCASE( "three" )
        {
                T d3{ d2 };

                check_links( d1 );
                check_for_each_node( d1, { &d1, &d2, &d3 } );
        }

        SUBCASE( "three assign" )
        {
                T d3;
                d3 = d2;

                check_links( d1 );
                check_for_each_node( d1, { &d1, &d2, &d3 } );
        }

        SUBCASE( "move it" )
        {
                T d3{ std::move( d2 ) };
                check_links( d1 );
                check_for_each_node( d1, { &d1, &d3 } );
        }

        SUBCASE( "move it assign" )
        {
                T d3;
                d3 = std::move( d2 );
                check_links( d1 );
                check_for_each_node( d1, { &d1, &d3 } );
        }
}

TEST_CASE_TEMPLATE( "vector", T, node_t, der, off_node, off_der )
{
        std::vector< T > d1;
        for ( int i = 0; i < 42; i++ ) {
                auto& last = d1.emplace_back();
                if ( i > 0 )
                        link_detached< T, typename T::access >( d1.front(), last );
                check_links( d1.front() );
        }
        std::set< T const* > s;
        for ( T& d : d1 )
                s.insert( &d );
        auto& n = top_node_of( d1.front() );
        check_for_each_node( n, s );
}

TEST_CASE( "cont" )
{
        der c1{ 1 }, c2{ 2 }, c3{ 3 };

        sh_heap< der > h = { &c1, &c2, &c3 };

        {
                std::vector< int > x;
                der                c20{ c2 };
                preorder_traverse( top_node_of( c1 ), [&]( der& c ) {
                        x.emplace_back( c.x );
                } );
                CHECK_EQ( x.size(), 4 );
                CHECK_EQ( x.front(), 1 );
        }
}

template < typename T, typename Acc = typename T::access >
void count_nodes( T& node, size_t& count )
{
        count++;
        auto& h = Acc::get( node );
        if ( h._left )
                count_nodes( _node< T, Acc >( *h._left ), count );
        if ( h._right )
                count_nodes( _node< T, Acc >( *h._right ), count );
}

template < typename T, typename Acc = typename T::access, typename Compare = std::less<> >
void check_heap_property( T& node, Compare comp = {} )
{
        auto& h = Acc::get( node );

        if ( h._left ) {
                T& l = _node< T, Acc >( *h._left );
                CHECK_FALSE( comp( l, node ) );  // parent should be <= child
                check_heap_property< T, Acc, Compare >( l, comp );
        }
        if ( h._right ) {
                T& r = _node< T, Acc >( *h._right );
                CHECK_FALSE( comp( r, node ) );  // parent should be <= child
                check_heap_property< T, Acc, Compare >( r, comp );
        }
}

template < typename T, typename Acc = typename T::access, typename Compare = std::less<> >
void check_heap_coherence( sh_heap< T, Acc, Compare > const& heap )
{
        if ( !heap.top() )
                return;

        // Check that top node's parent points back to the heap, whose untyped part is its first
        // base
        auto& top_hdr = Acc::get( *heap.top() );
        CHECK_EQ(
            static_cast< void const* >( top_hdr._parent.b() ),
            static_cast< void const* >( &heap ) );

        // Recursively check all nodes in the tree
        std::function< void( T const& ) > check_node = [&]( T const& node ) {
                auto& hdr = Acc::get( node );

                if ( hdr._left ) {
                        // Check that left child's parent points back to this node
                        CHECK_EQ( hdr._left->_parent.a(), &hdr );
                        check_node( _node< T, Acc >( *hdr._left ) );
                }

                if ( hdr._right ) {
                        // Check that right child's parent points back to this node
                        CHECK_EQ( hdr._right->_parent.a(), &hdr );
                        check_node( _node< T, Acc >( *hdr._right ) );
                }
        };

        check_node( *heap.top() );
}

TEST_CASE( "merge" )
{
        struct comparable_node : public sh_base< comparable_node >
        {
                int value;

                comparable_node( int v = 0 )
                  : value( v )
                {
                }

                bool operator<( comparable_node const& other ) const noexcept
                {
                        return value < other.value;
                }
        };

        SUBCASE( "merge empty heaps" )
        {
                sh_heap< comparable_node > h1, h2;
                h1.merge( std::move( h2 ) );
                CHECK( h1.empty() );
                CHECK( h2.empty() );
        }

        SUBCASE( "merge empty into non-empty" )
        {
                comparable_node            n1( 1 ), n2( 2 );
                sh_heap< comparable_node > h1 = { &n1, &n2 }, h2;

                h1.merge( std::move( h2 ) );
                CHECK( h2.empty() );
                CHECK_FALSE( h1.empty() );
                CHECK_EQ( h1.top()->value, 1 );
                check_heap_property( *h1.top() );
        }

        SUBCASE( "merge non-empty into empty" )
        {
                comparable_node            n1( 1 ), n2( 2 );
                sh_heap< comparable_node > h1, h2 = { &n1, &n2 };

                h1.merge( std::move( h2 ) );
                CHECK( h2.empty() );
                CHECK_FALSE( h1.empty() );
                CHECK_EQ( h1.top()->value, 1 );
                check_heap_property( *h1.top() );
        }

        SUBCASE( "merge self should be a no-op" )
        {
                comparable_node            n1( 1 ), n2( 2 );
                sh_heap< comparable_node > h1 = { &n1, &n2 };

                h1.merge( std::move( h1 ) );
                CHECK_FALSE( h1.empty() );
                CHECK_EQ( h1.top()->value, 1 );
                check_heap_property( *h1.top() );
        }
}

TEST_CASE( "heap_operations" )
{
        // Re-using the comparable_node from the "merge" test case
        struct comparable_node : public sh_base< comparable_node >
        {
                int value;

                comparable_node( int v = 0 )
                  : value( v )
                {
                }

                bool operator<( comparable_node const& other ) const noexcept
                {
                        return value < other.value;
                }
        };

        sh_heap< comparable_node > h;

        SUBCASE( "link (push) preserves heap property" )
        {
                comparable_node n5( 5 ), n2( 2 ), n4( 4 ), n1( 1 ), n3( 3 );
                h.link( n5 );
                CHECK_EQ( h.top()->value, 5 );
                h.link( n2 );
                CHECK_EQ( h.top()->value, 2 );
                h.link( n4 );
                CHECK_EQ( h.top()->value, 2 );
                h.link( n1 );
                CHECK_EQ( h.top()->value, 1 );
                h.link( n3 );
                CHECK_EQ( h.top()->value, 1 );

                size_t node_count = 0;
                count_nodes( *h.top(), node_count );
                CHECK_EQ( node_count, 5 );
                check_heap_property( *h.top() );
        }

        SUBCASE( "unlink (pop) extracts in sorted order" )
        {
                comparable_node nodes[] = { { 5 }, { 2 }, { 4 }, { 1 }, { 3 } };
                for ( auto& n : nodes )
                        h.link( n );

                CHECK_EQ( h.take().value, 1 );
                check_heap_property( *h.top() );
                CHECK_EQ( h.take().value, 2 );
                check_heap_property( *h.top() );
                CHECK_EQ( h.take().value, 3 );
                check_heap_property( *h.top() );
                CHECK_EQ( h.take().value, 4 );
                check_heap_property( *h.top() );
                CHECK_EQ( h.take().value, 5 );
                CHECK( h.empty() );
        }

        SUBCASE( "mixed link and unlink" )
        {
                comparable_node n5( 5 ), n2( 2 ), n4( 4 ), n1( 1 ), n3( 3 );
                h.link( n5 );
                h.link( n2 );
                CHECK_EQ( h.top()->value, 2 );

                CHECK_EQ( h.take().value, 2 );
                CHECK_EQ( h.top()->value, 5 );

                h.link( n4 );
                h.link( n1 );
                CHECK_EQ( h.top()->value, 1 );

                CHECK_EQ( h.take().value, 1 );
                CHECK_EQ( h.top()->value, 4 );

                h.link( n3 );
                CHECK_EQ( h.top()->value, 3 );

                CHECK_EQ( h.take().value, 3 );
                CHECK_EQ( h.take().value, 4 );
                CHECK_EQ( h.take().value, 5 );
                CHECK( h.empty() );
        }
}

TEST_CASE( "move_semantics" )
{
        struct comparable_node : public sh_base< comparable_node >
        {
                int value;

                comparable_node( int v = 0 )
                  : value( v )
                {
                }

                bool operator<( comparable_node const& other ) const noexcept
                {
                        return value < other.value;
                }
        };

        SUBCASE( "move construction" )
        {
                comparable_node            n1( 10 ), n2( 20 ), n3( 5 );
                sh_heap< comparable_node > h1;
                h1.link( n1 );
                h1.link( n2 );
                h1.link( n3 );

                CHECK_EQ( h1.top()->value, 5 );

                sh_heap< comparable_node > h2( std::move( h1 ) );

                CHECK( h1.empty() );
                CHECK_FALSE( h2.empty() );
                CHECK_EQ( h2.top()->value, 5 );

                size_t node_count = 0;
                count_nodes( *h2.top(), node_count );
                CHECK_EQ( node_count, 3 );
                check_heap_property( *h2.top() );
        }

        SUBCASE( "move assignment" )
        {
                comparable_node            n1( 10 ), n2( 20 );
                comparable_node            n3( 5 ), n4( 15 );
                sh_heap< comparable_node > h1 = { &n1, &n2 };
                sh_heap< comparable_node > h2 = { &n3, &n4 };

                CHECK_EQ( h1.top()->value, 10 );
                CHECK_EQ( h2.top()->value, 5 );

                h1 = std::move( h2 );

                CHECK( h2.empty() );
                CHECK_FALSE( h1.empty() );
                CHECK_EQ( h1.top()->value, 5 );

                size_t node_count = 0;
                count_nodes( *h1.top(), node_count );
                CHECK_EQ( node_count, 2 );
                check_heap_property( *h1.top() );
        }

        SUBCASE( "move assignment to self" )
        {
                comparable_node            n1( 10 ), n2( 5 );
                sh_heap< comparable_node > h1 = { &n1, &n2 };
                CHECK_EQ( h1.top()->value, 5 );
#pragma GCC diagnostic ignored "-Wself-move"
                h1 = std::move( h1 );  // This should be a no-op

                CHECK_FALSE( h1.empty() );
                CHECK_EQ( h1.top()->value, 5 );
                size_t node_count = 0;
                count_nodes( *h1.top(), node_count );
                CHECK_EQ( node_count, 2 );
                check_heap_property( *h1.top() );
        }
}

TEST_CASE( "edge_cases_and_traversal" )
{
        struct comparable_node : public sh_base< comparable_node >
        {
                int value;

                comparable_node( int v = 0 )
                  : value( v )
                {
                }

                bool operator<( comparable_node const& other ) const noexcept
                {
                        return value < other.value;
                }
        };

        SUBCASE( "operations on empty heap" )
        {
                sh_heap< comparable_node > h;
                CHECK( h.empty() );
                // The following line should assert/terminate if ZLL_DEFAULT_ASSERT is defined.
                // CHECK_THROWS( h.unlink() );
        }

        SUBCASE( "single element heap" )
        {
                sh_heap< comparable_node > h;
                comparable_node            n1( 42 );

                h.link( n1 );
                CHECK_FALSE( h.empty() );
                CHECK_EQ( h.top()->value, 42 );

                CHECK_EQ( h.take().value, 42 );
                CHECK( h.empty() );

                // Link again
                h.link( n1 );
                CHECK_FALSE( h.empty() );
                CHECK_EQ( h.top()->value, 42 );
        }

        SUBCASE( "traversal" )
        {
                comparable_node n4( 4 ), n2( 2 ), n6( 6 ), n1( 1 ), n3( 3 ), n5( 5 ), n7( 7 );
                sh_heap< comparable_node > h = { &n4, &n2, &n6, &n1, &n3, &n5, &n7 };

                // The exact traversal order depends on the heap's internal structure,
                // but we can verify the set of visited nodes.
                std::set< int > visited_values;
                inorder_traverse( *h.top(), [&]( comparable_node& n ) {
                        visited_values.insert( n.value );
                } );
                CHECK_EQ( visited_values, std::set< int >{ 1, 2, 3, 4, 5, 6, 7 } );

                visited_values.clear();
                preorder_traverse( *h.top(), [&]( comparable_node& n ) {
                        visited_values.insert( n.value );
                } );
                CHECK_EQ( visited_values, std::set< int >{ 1, 2, 3, 4, 5, 6, 7 } );

                visited_values.clear();
                postorder_traverse( *h.top(), [&]( comparable_node& n ) {
                        visited_values.insert( n.value );
                } );
                CHECK_EQ( visited_values, std::set< int >{ 1, 2, 3, 4, 5, 6, 7 } );
        }
}

TEST_CASE( "custom_comparator" )
{
        struct comparable_node : public sh_base< comparable_node, std::greater<> >
        {
                int value;

                comparable_node( int v = 0 )
                  : value( v )
                {
                }

                bool operator>( comparable_node const& other ) const noexcept
                {
                        return value > other.value;
                }
        };

        SUBCASE( "node functions with a custom comparator" )
        {
                comparable_node n1( 1 ), n2( 2 );
                sh_heap< comparable_node, typename comparable_node::access, std::greater<> > h{
                    &n1, &n2 };
                CHECK_EQ( h.top(), &n2 );

                comparable_node moved( std::move( n1 ) );
                comparable_node copy( n2 );
                CHECK_EQ( &top_node_of( moved ), h.top() );

                std::size_t count = 0;
                preorder_traverse( *h.top(), [&]( comparable_node& ) {
                        ++count;
                } );
                inorder_traverse( *h.top(), [&]( comparable_node& ) {
                        ++count;
                } );
                postorder_traverse( *h.top(), [&]( comparable_node& ) {
                        ++count;
                } );
                CHECK_EQ( count, 9 );
                check_heap_property( *h.top(), std::greater<>{} );
                check_heap_coherence( h );
        }

        SUBCASE( "max-heap link and take" )
        {
                sh_heap< comparable_node, typename comparable_node::access, std::greater<> >
                    max_heap( std::greater<>{} );

                comparable_node n1( 1 ), n5( 5 ), n3( 3 ), n8( 8 ), n2( 2 );
                max_heap.link( n1 );
                CHECK_EQ( max_heap.top()->value, 1 );
                max_heap.link( n5 );
                CHECK_EQ( max_heap.top()->value, 5 );
                max_heap.link( n3 );
                CHECK_EQ( max_heap.top()->value, 5 );
                max_heap.link( n8 );
                CHECK_EQ( max_heap.top()->value, 8 );
                max_heap.link( n2 );
                CHECK_EQ( max_heap.top()->value, 8 );

                check_heap_property( *max_heap.top(), std::greater<>{} );

                CHECK_EQ( max_heap.take().value, 8 );
                check_heap_property( *max_heap.top(), std::greater<>{} );
                CHECK_EQ( max_heap.take().value, 5 );
                check_heap_property( *max_heap.top(), std::greater<>{} );
                CHECK_EQ( max_heap.take().value, 3 );
                check_heap_property( *max_heap.top(), std::greater<>{} );
                CHECK_EQ( max_heap.take().value, 2 );
                CHECK_EQ( max_heap.take().value, 1 );
                CHECK( max_heap.empty() );
        }

        SUBCASE( "max-heap merge" )
        {
                sh_heap< comparable_node, typename comparable_node::access, std::greater<> > h1(
                    std::greater<>{} );
                sh_heap< comparable_node, typename comparable_node::access, std::greater<> > h2(
                    std::greater<>{} );

                comparable_node n1( 1 ), n3( 3 ), n5( 5 );
                comparable_node n2( 2 ), n4( 4 ), n6( 6 );
                h1.link( n1 );
                h1.link( n3 );
                h1.link( n5 );
                check_heap_coherence( h1 );

                h2.link( n2 );
                h2.link( n4 );
                h2.link( n6 );
                check_heap_coherence( h2 );

                CHECK_EQ( h1.top()->value, 5 );
                CHECK_EQ( h2.top()->value, 6 );

                h1.merge( std::move( h2 ) );
                check_heap_coherence( h1 );
                check_heap_coherence( h2 );

                CHECK( h2.empty() );
                CHECK_FALSE( h1.empty() );
                CHECK_EQ( h1.top()->value, 6 );

                size_t node_count = 0;
                count_nodes( *h1.top(), node_count );
                CHECK_EQ( node_count, 6 );
                check_heap_property( *h1.top(), std::greater<>{} );
                check_heap_coherence( h1 );
        }
}

struct may_throw_less
{
        bool operator()( auto const& a, auto const& b ) const
        {
                return a.x < b.x;
        }
};

struct may_throw_node
{
        sh_header< may_throw_node, hdr_access, may_throw_less > hdr;
        using access = hdr_access;
        int x        = 0;
};

TEST_CASE( "noexcept follows the accessor and the comparator" )
{
        node_t            a;
        sh_heap< node_t > h;
        CHECK( noexcept( detach( a, std::less<>{} ) ) );
        CHECK( noexcept( h.link( a ) ) );
        CHECK( noexcept( h.take() ) );

        may_throw_node                                        m;
        sh_heap< may_throw_node, hdr_access, may_throw_less > mh;
        CHECK_FALSE( noexcept( detach( m, may_throw_less{} ) ) );
        CHECK_FALSE( noexcept( mh.link( m ) ) );
        CHECK_FALSE( noexcept( mh.take() ) );

        auto const quiet = []( auto& ) noexcept {
                return true;
        };
        auto const loud = []( auto& ) {
                return true;
        };
        sh_bag< node_t > bag;
        CHECK( noexcept( h.extract_if( quiet ) ) );
        CHECK_FALSE( noexcept( h.extract_if( loud ) ) );
        CHECK( noexcept( h.link( std::move( bag ) ) ) );
        CHECK( (std::is_nothrow_constructible_v< sh_heap< node_t >, sh_bag< node_t >&& >) );

        sh_bag< may_throw_node, hdr_access > mbag;
        CHECK_FALSE( noexcept( mh.extract_if( quiet ) ) );
        CHECK_FALSE( noexcept( mh.link( std::move( mbag ) ) ) );
        CHECK_FALSE( (std::is_nothrow_constructible_v<
                      sh_heap< may_throw_node, hdr_access, may_throw_less >,
                      sh_bag< may_throw_node, hdr_access >&& >) );
}

TEST_CASE( "copies keep the heap property" )
{
        der            a{ 1 }, b{ 2 }, c{ 3 };
        sh_heap< der > h = { &a, &b, &c };

        der d{ 9 };
        d = a;
        der e{ a };

        check_heap_property( *h.top() );
        check_heap_coherence( h );
        std::size_t count = 0;
        count_nodes( *h.top(), count );
        CHECK_EQ( count, 5 );
}

struct flip_less
{
        bool greater = false;

        bool operator()( auto const& a, auto const& b ) const noexcept
        {
                return greater ? b.x < a.x : a.x < b.x;
        }
};

struct flip_node : sh_base< flip_node, flip_less >
{
        int x;

        flip_node( int v = 0 )
          : x( v )
        {
        }
};

TEST_CASE( "merging into an empty heap keeps its comparator" )
{
        flip_node                                          a{ 1 }, b{ 2 };
        sh_heap< flip_node, flip_node::access, flip_less > max_heap( flip_less{ true } );
        sh_heap< flip_node, flip_node::access, flip_less > min_heap( flip_less{ false } );
        min_heap.link( a );
        max_heap.merge( std::move( min_heap ) );
        max_heap.link( b );
        CHECK_EQ( max_heap.top(), &b );
}

struct dual_node : ll_base< dual_node >, sh_base< dual_node >
{
        using access = ll_base< dual_node >::access;

        int x = 0;

        bool operator<( dual_node const& other ) const noexcept
        {
                return x < other.x;
        }
};

template < typename T, typename Acc, typename Compare >
std::vector< int > drain_values( sh_heap< T, Acc, Compare >& h )
{
        std::vector< int > out;
        while ( !h.empty() )
                out.push_back( h.take().x );
        return out;
}

// The keys of every node in `bag`, sorted.
std::vector< int > bag_values( sh_bag< der > const& bag )
{
        std::vector< int > out;
        der const*         f = bag.front();
        if ( !f )
                return out;
        out.push_back( f->x );
        auto const&             fh = der::access::get( *f );
        std::vector< _sh_hdr* > todo{ fh._left, fh._right };
        while ( !todo.empty() ) {
                _sh_hdr* h = todo.back();
                todo.pop_back();
                if ( !h )
                        continue;
                out.push_back( _node< der, der::access >( *h ).x );
                todo.push_back( h->_left );
                todo.push_back( h->_right );
        }
        std::sort( out.begin(), out.end() );
        return out;
}

// ASan may give each call a frame of its own on a fake stack, where the address of a local says
// nothing about how deep the call is.
#if defined( __SANITIZE_ADDRESS__ )
constexpr bool fake_stack = true;
#elif defined( __has_feature )
#if __has_feature( address_sanitizer )
constexpr bool fake_stack = true;
#else
constexpr bool fake_stack = false;
#endif
#else
constexpr bool fake_stack = false;
#endif

// The range of stack addresses `note` was called at: a loop calls it at one depth, a recursion at
// a deeper one on each level.
struct stack_span
{
        std::uintptr_t lo = UINTPTR_MAX;
        std::uintptr_t hi = 0;

        void note()
        {
                int const            local = 0;
                std::uintptr_t const at    = reinterpret_cast< std::uintptr_t >( &local );
                lo                         = std::min( lo, at );
                hi                         = std::max( hi, at );
        }
};

TEST_CASE( "merging two long right spines runs in constant stack" )
{
        // Each spine is a valid heap: every node's only child is its right child, with a larger
        // key. The recursive form of the merge would descend once per node of both spines.
        std::size_t const  per = 50000;
        std::vector< der > a( per ), b( per );
        for ( std::size_t i = 0; i < per; ++i ) {
                a[i].x = static_cast< int >( 2 * i );
                b[i].x = static_cast< int >( 2 * i + 1 );
                if ( i ) {
                        _sh_attach_right( der::access::get( a[i - 1] ), der::access::get( a[i] ) );
                        _sh_attach_right( der::access::get( b[i - 1] ), der::access::get( b[i] ) );
                }
        }
        stack_span span;
        auto       cmp = [&]( der const& l, der const& r ) {
                span.note();
                return l.x < r.x;
        };
        _sh_hdr& root = _sh_merge< der, der::access >(
            der::access::get( a[0] ), der::access::get( b[0] ), cmp );
        if ( !fake_stack )
                CHECK_LT( span.hi - span.lo, 1024u );

        // The merged tree holds every node once, each no smaller than its parent.
        std::size_t             count = 0;
        std::vector< _sh_hdr* > todo{ &root };
        while ( !todo.empty() ) {
                _sh_hdr* h = todo.back();
                todo.pop_back();
                ++count;
                for ( _sh_hdr* c : { h->_left, h->_right } ) {
                        if ( !c )
                                continue;
                        CHECK_EQ( c->_parent.a(), h );
                        CHECK_FALSE(
                            _node< der, der::access >( *c ) < _node< der, der::access >( *h ) );
                        todo.push_back( c );
                }
        }
        CHECK_EQ( count, 2 * per );
        CHECK_EQ( _node< der, der::access >( root ).x, 0 );
}

struct counting_less
{
        static inline std::size_t calls = 0;

        bool operator()( auto const& a, auto const& b ) const noexcept
        {
                ++calls;
                return a.x < b.x;
        }
};

struct counted_node : sh_base< counted_node, counting_less >
{
        int x = 0;
};

using counted_heap = sh_heap< counted_node, counted_node::access, counting_less >;

TEST_CASE( "a skew heap makes O(log n) amortized comparisons per operation" )
{
        // A merge without the skew swap of children still builds a valid heap, but makes n / 2
        // comparisons per link of ascending keys.
        std::size_t const log2_n = 14;
        std::size_t const n      = std::size_t{ 1 } << log2_n;
        std::mt19937      rng( 3 );
        for ( int order : { 0, 1, 2 } ) {
                CAPTURE( order );
                std::vector< counted_node > nodes( n );
                for ( std::size_t i = 0; i < n; ++i )
                        nodes[i].x = static_cast< int >(
                            order == 0 ? i :
                            order == 1 ? n - i :
                                         rng() % n );
                counted_heap h;
                counting_less::calls = 0;
                for ( auto& c : nodes )
                        h.link( c );
                while ( !h.empty() )
                        h.take();
                CHECK_LT( counting_less::calls, 3 * n * log2_n );
        }
}

TEST_CASE( "extract_if makes O(log n) amortized comparisons per node it unlinks" )
{
        std::size_t const           log2_n = 14;
        std::size_t const           n      = std::size_t{ 1 } << log2_n;
        std::mt19937                rng( 5 );
        std::vector< counted_node > nodes( n );
        for ( auto& c : nodes )
                c.x = static_cast< int >( rng() % n );
        counted_heap h;
        for ( auto& c : nodes )
                h.link( c );
        std::size_t calls = 0, taken = 0;
        counting_less::calls = 0;
        auto out             = h.extract_if( [&]( counted_node& c ) {
                ++calls;
                if ( c.x % 2 )
                        return false;
                ++taken;
                return true;
        } );
        CHECK_EQ( calls, n );
        CHECK_GT( taken, n / 4 );
        CHECK_LT( counting_less::calls, 3 * taken * log2_n );
}

struct throwing_less
{
        static inline int calls    = 0;
        static inline int throw_at = 0;

        bool operator()( auto const& a, auto const& b ) const
        {
                if ( ++calls == throw_at )
                        throw std::runtime_error( "compare" );
                return a.x < b.x;
        }
};

struct throwing_node : sh_base< throwing_node, throwing_less >
{
        int x;

        throwing_node( int v = 0 )
          : x( v )
        {
        }
};

TEST_CASE( "a comparator that throws in a merge leaves two heaps with consistent links" )
{
        // Interleaved keys make the merge switch spines, so a throw can follow a swap.
        int throws = 0;
        for ( int at = 1;; ++at ) {
                CAPTURE( at );
                std::vector< throwing_node > nodes;
                nodes.reserve( 16 );
                for ( int i = 0; i < 16; ++i )
                        nodes.emplace_back( i );
                sh_heap< throwing_node, throwing_node::access, throwing_less > h, g;
                throwing_less::throw_at = 0;
                for ( auto& n : nodes )
                        ( n.x % 2 ? g : h ).link( n );

                throwing_less::calls    = 0;
                throwing_less::throw_at = at;
                bool threw              = false;
                try {
                        h.merge( std::move( g ) );
                }
                catch ( std::runtime_error const& ) {
                        threw = true;
                }
                throwing_less::throw_at = 0;

                std::size_t roots = 0;
                for ( auto& n : nodes ) {
                        _sh_hdr& hdr = throwing_node::access::get( n );
                        if ( hdr._left ) {
                                CHECK_EQ( hdr._left->_parent.a(), &hdr );
                                CHECK_NE( hdr._left, hdr._right );
                        }
                        if ( hdr._right )
                                CHECK_EQ( hdr._right->_parent.a(), &hdr );
                        if ( _sh_hdr* p = hdr._parent.a() )
                                CHECK( ( p->_left == &hdr || p->_right == &hdr ) );
                        if ( !hdr._parent ) {
                                ++roots;
                                check_heap_property<
                                    throwing_node,
                                    throwing_node::access,
                                    throwing_less >( n );
                        }
                }
                if ( !threw )
                        break;
                ++throws;
                CHECK( h.empty() );
                CHECK( g.empty() );
                CHECK_EQ( roots, 2u );
        }
        CHECK_GT( throws, 4 );
}

TEST_CASE( "extract_if" )
{
        SUBCASE( "an empty heap gives an empty heap" )
        {
                sh_heap< der > h;
                int            calls = 0;
                auto           out   = h.extract_if( [&]( der& ) {
                        ++calls;
                        return true;
                } );
                CHECK( out.empty() );
                CHECK( h.empty() );
                CHECK_EQ( calls, 0 );
        }

        SUBCASE( "no match leaves the heap whole" )
        {
                std::vector< der > nodes;
                for ( int i : { 5, 3, 8, 1, 9, 2 } )
                        nodes.emplace_back( i );
                sh_heap< der > h;
                for ( auto& n : nodes )
                        h.link( n );
                auto out = h.extract_if( []( der& ) {
                        return false;
                } );
                CHECK( out.empty() );
                check_heap_coherence( h );
                CHECK_EQ( drain_values( h ), std::vector< int >{ 1, 2, 3, 5, 8, 9 } );
        }

        SUBCASE( "everything matching moves every node" )
        {
                std::vector< der > nodes;
                for ( int i : { 5, 3, 8, 1, 9, 2 } )
                        nodes.emplace_back( i );
                sh_heap< der > h;
                for ( auto& n : nodes )
                        h.link( n );
                sh_heap< der > out( h.extract_if( []( der& ) {
                        return true;
                } ) );
                CHECK( h.empty() );
                check_heap_coherence( out );
                CHECK_EQ( drain_values( out ), std::vector< int >{ 1, 2, 3, 5, 8, 9 } );
        }

        SUBCASE( "a node's children are seen before the node" )
        {
                std::vector< der > nodes;
                for ( int i = 0; i < 32; ++i )
                        nodes.emplace_back( ( i * 7 ) % 32 );
                sh_heap< der > h;
                for ( auto& n : nodes )
                        h.link( n );
                std::vector< std::pair< _sh_hdr const*, _sh_hdr const* > > children;
                for ( auto& n : nodes ) {
                        auto& hdr = der::access::get( n );
                        children.emplace_back( hdr._left, hdr._right );
                }
                std::vector< _sh_hdr const* > order;
                auto                          out = h.extract_if( [&]( der& n ) {
                        order.push_back( &der::access::get( n ) );
                        return n.x % 2 == 0;
                } );
                auto                          pos = [&]( _sh_hdr const* x ) {
                        return std::find( order.begin(), order.end(), x ) - order.begin();
                };
                CHECK_EQ( order.size(), nodes.size() );
                for ( std::size_t i = 0; i < nodes.size(); ++i ) {
                        auto const self = pos( &der::access::get( nodes[i] ) );
                        if ( children[i].first )
                                CHECK_LT( pos( children[i].first ), self );
                        if ( children[i].second )
                                CHECK_LT( pos( children[i].second ), self );
                }
        }

        SUBCASE( "what is left stays ordered by the heap's comparator" )
        {
                // The nodes' own comparator orders the other way, so detaching with it would
                // break this heap.
                std::vector< flip_node > nodes;
                nodes.reserve( 40 );
                for ( int i = 0; i < 40; ++i )
                        nodes.emplace_back( ( i * 17 ) % 40 );
                sh_heap< flip_node, flip_node::access, flip_less > h( flip_less{ true } );
                for ( auto& n : nodes )
                        h.link( n );
                auto out = h.extract_if( []( flip_node& n ) {
                        return n.x % 3 == 0;
                } );
                check_heap_coherence( h );
                check_heap_property( *h.top(), flip_less{ true } );
                std::vector< int > left = drain_values( h );
                CHECK_EQ( left.size(), 26u );
                CHECK( std::is_sorted( left.rbegin(), left.rend() ) );
        }

        SUBCASE( "a predicate that throws leaves the nodes it did not unlink in a valid heap" )
        {
                std::vector< der > nodes;
                nodes.reserve( 20 );
                for ( int i = 0; i < 20; ++i )
                        nodes.emplace_back( ( i * 7 ) % 20 );
                sh_heap< der > h;
                for ( auto& n : nodes )
                        h.link( n );
                int         calls = 0;
                std::size_t taken = 0;
                auto        pred  = [&]( der& n ) {
                        if ( ++calls == 12 )
                                throw std::runtime_error( "predicate" );
                        if ( n.x % 2 )
                                return false;
                        ++taken;
                        return true;
                };
                CHECK_THROWS_AS( (void) h.extract_if( pred ), std::runtime_error );
                CHECK_GT( taken, 0u );
                check_heap_coherence( h );
                check_heap_property( *h.top() );
                CHECK_EQ( drain_values( h ).size(), nodes.size() - taken );
        }

        SUBCASE( "a heap one node wide is walked without recursion" )
        {
                // Linking ever smaller keys makes each new node the root above the previous one.
                std::vector< der > nodes( 100000 );
                for ( std::size_t i = 0; i < nodes.size(); ++i )
                        nodes[i].x = static_cast< int >( nodes.size() - i );
                sh_heap< der > h;
                for ( auto& n : nodes )
                        h.link( n );
                std::size_t calls = 0;
                stack_span  span;
                auto        out = h.extract_if( [&]( der& n ) {
                        span.note();
                        ++calls;
                        return n.x % 1000 == 0;
                } );
                CHECK_EQ( calls, nodes.size() );
                // Every call runs at one stack depth, which a recursive walk would not.
                if ( !fake_stack )
                        CHECK_LT( span.hi - span.lo, 1024u );
                std::size_t taken = 0;
                while ( !out.empty() ) {
                        CHECK_EQ( out.take().x % 1000, 0 );
                        ++taken;
                }
                CHECK_EQ( taken, nodes.size() / 1000 );
        }
}

TEST_CASE_TEMPLATE(
    "extract_if splits random heaps, each node seen once",
    T,
    node_t,
    der,
    off_node,
    off_der )
{
        std::mt19937 rng( 7 );
        for ( int round = 0; round < 200; ++round ) {
                std::size_t const   count = 1 + rng() % 64;
                std::vector< T >    nodes;
                std::vector< int >  kept, taken;
                std::multiset< T* > seen;
                nodes.reserve( count );
                for ( std::size_t i = 0; i < count; ++i )
                        nodes.emplace_back( static_cast< int >( rng() % 50 ) );
                sh_heap< T > h;
                for ( auto& n : nodes )
                        h.link( n );
                for ( auto& n : nodes )
                        ( n.x % 3 == 0 ? taken : kept ).push_back( n.x );
                sh_heap< T > out( h.extract_if( [&]( T& n ) {
                        seen.insert( &n );
                        return n.x % 3 == 0;
                } ) );
                CHECK_EQ( seen.size(), count );
                CHECK_EQ( std::set< T* >( seen.begin(), seen.end() ).size(), count );
                check_heap_coherence( h );
                check_heap_coherence( out );
                if ( h.top() )
                        check_heap_property( *h.top() );
                if ( out.top() )
                        check_heap_property( *out.top() );
                std::sort( kept.begin(), kept.end() );
                std::sort( taken.begin(), taken.end() );
                CHECK_EQ( drain_values( h ), kept );
                CHECK_EQ( drain_values( out ), taken );
        }
}

TEST_CASE_TEMPLATE(
    "a bag keeps every node through copies and destructions",
    T,
    node_t,
    der,
    off_node,
    off_der )
{
        // Copies and destructions relink nodes as in a heap: they give nodes left children and
        // merge subtrees.
        std::vector< std::optional< T > > nodes( 12 );
        sh_bag< T >                       bag;
        for ( int i = 0; i < 8; ++i ) {
                nodes[std::size_t( i )].emplace( i );
                bag.link( *nodes[std::size_t( i )] );
        }
        nodes[8].emplace( *nodes[7] );
        nodes[9].emplace( *nodes[3] );
        nodes[10].emplace( *nodes[0] );
        nodes[11].emplace( *nodes[9] );
        nodes[3].reset();
        nodes[7].reset();
        nodes[9].reset();

        std::set< T* > live, taken;
        for ( auto& n : nodes )
                if ( n )
                        live.insert( &*n );
        while ( !bag.empty() ) {
                T& t = bag.take();
                CHECK( detached( t ) );
                taken.insert( &t );
        }
        CHECK_EQ( taken, live );
}

TEST_CASE( "sh_bag" )
{
        SUBCASE( "takes its nodes in the reverse of the order they were linked" )
        {
                der                  a( 1 ), b( 2 ), c( 3 );
                sh_bag< der >        bag;
                sh_bag< der > const& view = bag;
                CHECK( bag.empty() );
                CHECK_EQ( bag.front(), nullptr );
                bag.link( a );
                bag.link( b );
                bag.link( c );
                CHECK_EQ( bag.front(), &c );
                CHECK_EQ( view.front(), &c );
                for ( der* n : { &c, &b, &a } ) {
                        der& t = bag.take();
                        CHECK_EQ( &t, n );
                        CHECK( detached( t ) );
                }
                CHECK( bag.empty() );
        }

        SUBCASE( "a node destroyed in the bag unlinks itself, at the front, inside or at the end" )
        {
                der                  c( 3 );
                std::optional< der > a( std::in_place, 1 );
                std::optional< der > b( std::in_place, 2 );
                std::optional< der > d( std::in_place, 4 );
                sh_bag< der >        bag;
                bag.link( *a );
                bag.link( *b );
                bag.link( c );
                bag.link( *d );
                d.reset();
                CHECK_EQ( bag_values( bag ), std::vector< int >{ 1, 2, 3 } );
                b.reset();
                CHECK_EQ( bag_values( bag ), std::vector< int >{ 1, 3 } );
                a.reset();
                CHECK_EQ( bag_values( bag ), std::vector< int >{ 3 } );
                CHECK_EQ( &bag.take(), &c );
                CHECK( bag.empty() );
        }

        SUBCASE( "a copy of a node in the bag is in the bag too" )
        {
                der           a( 1 ), b( 2 ), c( 3 );
                sh_bag< der > bag;
                bag.link( a );
                bag.link( b );
                der copy( b );
                c = a;
                CHECK_EQ( bag_values( bag ), std::vector< int >{ 1, 1, 2, 2 } );
                std::set< der* > taken;
                while ( !bag.empty() ) {
                        der& t = bag.take();
                        CHECK( detached( t ) );
                        taken.insert( &t );
                }
                CHECK_EQ( taken, std::set< der* >{ &a, &b, &c, &copy } );
        }

        SUBCASE( "a node moved while in the bag hands its place over" )
        {
                der           a( 1 ), b( 2 ), c( 3 );
                sh_bag< der > bag;
                bag.link( a );
                bag.link( b );
                bag.link( c );
                der moved( std::move( b ) );
                der assigned( 9 );
                assigned = std::move( c );
                CHECK( detached( b ) );
                CHECK( detached( c ) );
                for ( der* n : { &assigned, &moved, &a } )
                        CHECK_EQ( &bag.take(), n );
                CHECK( bag.empty() );
        }

        SUBCASE( "a destroyed bag leaves its nodes linked to each other" )
        {
                der a( 1 ), b( 2 );
                {
                        sh_bag< der > bag;
                        bag.link( a );
                        bag.link( b );
                }
                auto& hb = der::access::get( b );
                CHECK( !hb._parent );
                CHECK_EQ( hb._right, &der::access::get( a ) );
                CHECK_EQ( der::access::get( a )._parent.a(), &hb );
        }

        SUBCASE( "moving a bag moves its nodes" )
        {
                der           a( 1 ), b( 2 ), c( 3 ), d( 4 );
                sh_bag< der > bag;
                bag.link( a );
                bag.link( b );
                sh_bag< der > moved( std::move( bag ) );
                CHECK( bag.empty() );
                CHECK_EQ( bag_values( moved ), std::vector< int >{ 1, 2 } );

                // The nodes the assigned-to bag held stay linked to each other.
                sh_bag< der > assigned;
                assigned.link( c );
                assigned.link( d );
                assigned = std::move( moved );
                CHECK( moved.empty() );
                CHECK( !der::access::get( d )._parent );
                CHECK_EQ( der::access::get( d )._right, &der::access::get( c ) );

#pragma GCC diagnostic ignored "-Wself-move"
                assigned = std::move( assigned );
                CHECK_EQ( bag_values( assigned ), std::vector< int >{ 1, 2 } );
                CHECK_EQ( &assigned.take(), &b );
                CHECK_EQ( &assigned.take(), &a );
        }

        SUBCASE( "a heap built from a bag, or linking one, orders its nodes" )
        {
                der           n[6] = { 5, 3, 8, 1, 9, 2 };
                sh_bag< der > bag;
                for ( auto& x : n )
                        bag.link( x );
                sh_heap< der > h( std::move( bag ) );
                CHECK( bag.empty() );
                check_heap_coherence( h );
                CHECK_EQ( drain_values( h ), std::vector< int >{ 1, 2, 3, 5, 8, 9 } );

                for ( auto& x : n )
                        bag.link( x );
                sh_heap< der > h2;
                der            zero( 0 );
                h2.link( zero );
                h2.link( std::move( bag ) );
                CHECK_EQ( drain_values( h2 ), std::vector< int >{ 0, 1, 2, 3, 5, 8, 9 } );

                flip_node           f[3] = { 1, 3, 2 };
                sh_bag< flip_node > fbag;
                for ( auto& x : f )
                        fbag.link( x );
                sh_heap< flip_node, flip_node::access, flip_less > max_heap(
                    std::move( fbag ), flip_less{ true } );
                CHECK( fbag.empty() );
                CHECK_EQ( drain_values( max_heap ), std::vector< int >{ 3, 2, 1 } );
        }
}

TEST_CASE( "node in both a list and a heap" )
{
        dual_node a, b;
        a.x = 1;
        b.x = 2;
        ll_list< dual_node >                               l{ &a, &b };
        sh_heap< dual_node, sh_base< dual_node >::access > h{ &a, &b };

        dual_node c{ std::move( a ) };
        CHECK_EQ( &l.front(), &c );
        CHECK_EQ( h.top(), &c );

        dual_node d{ b };
        CHECK_EQ( &l.back(), &d );
        CHECK_EQ( h.top(), &c );
}

}  // namespace
}  // namespace zll
