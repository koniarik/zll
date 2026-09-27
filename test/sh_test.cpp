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

#include <doctest/doctest.h>
#include <functional>
#include <iostream>
#include <list>
#include <set>
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
