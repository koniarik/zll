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
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "zll.hpp"

#include <doctest/doctest.h>
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
};

struct node_t
{
        ll_header< node_t, hdr_access > hdr;

        using access = hdr_access;

        node_t() noexcept = default;

        node_t( node_t&& o ) noexcept
        {
                move_from_to< node_t, hdr_access >( o, *this );
        }

        node_t( node_t& o ) noexcept
        {
                link_detached_as_next< node_t, hdr_access >( o, *this );
        }

        node_t& operator=( node_t&& o ) noexcept
        {
                detach< node_t, hdr_access >( *this );
                move_from_to< node_t, hdr_access >( o, *this );
                return *this;
        }

        node_t& operator=( node_t& o ) noexcept
        {
                detach< node_t, hdr_access >( *this );
                link_detached_as_next< node_t, hdr_access >( o, *this );
                return *this;
        }

        bool operator<( node_t const& other ) const noexcept
        {
                return this < &other;
        }
};

struct der : public ll_base< der >
{
        bool operator<( der const& other ) const noexcept
        {
                return this < &other;
        }
};

template < typename T, typename Acc = typename T::access >
void check_links( T& first )
{
        for ( T* p = _node( Acc::get( first ).next ); p; p = _node( Acc::get( *p ).next ) ) {
                CHECK( _node( Acc::get( *p ).prev ) );
                CHECK_EQ( p, _node( Acc::get( *_node( Acc::get( *p ).prev ) ).next ) );
        }
}

std::ostream& operator<<( std::ostream& os, std::set< node_t const* > const& value )
{
        os << "{ ";
        for ( auto* v : value )
                os << v << " ";
        os << "}";
        return os;
}

void check_for_each_node( node_t& n, std::set< node_t const* > const& expected )
{
        std::set< node_t const* > s;
        for_each_node< node_t >( n, [&]( node_t& m ) {
                s.insert( &m );
        } );
        CAPTURE( s.size() );
        CAPTURE( expected.size() );
        CHECK_EQ( s, expected );

        std::set< node_t const* > s2;
        for_each_node< node_t const >( std::as_const( n ), [&]( node_t const& m ) {
                s2.insert( &m );
        } );
        CHECK_EQ( s2, expected );
}

void check_for_each_node( der& n, std::set< der const* > const& expected )
{
        std::set< der const* > s;
        for_each_node( n, [&]( der& m ) {
                s.insert( &m );
        } );
        CHECK_EQ( s, expected );

        std::set< der const* > s2;
        for_each_node( std::as_const( n ), [&]( der const& m ) {
                s2.insert( &m );
        } );
        CHECK_EQ( s2, expected );
}

}  // namespace

TEST_CASE_TEMPLATE( "single", T, node_t, der )
{
        using access = typename T::access;

        T d1;

        SUBCASE( "nothing" )
        {
        }
        SUBCASE( "list" )
        {
                ll_list< T, access > l;
                l.link_back( d1 );

                CHECK_EQ( &l.front(), &d1 );
                CHECK_EQ( &l.back(), &d1 );
        }

        check_links( d1 );
        check_for_each_node( d1, { &d1 } );
}

TEST_CASE_TEMPLATE( "dual", T, node_t, der )
{
        using access = typename T::access;

        T d1, d2;

        SUBCASE( "link as last" )
        {
                link_detached_as_last< T, typename T::access >( d1, d2 );
                check_for_each_node( d1, { &d1, &d2 } );
        }
        SUBCASE( "move" )
        {
                ll_list< T, access > l;
                l.link_back( d1 );
                l.link_back( d2 );
                T d3{ std::move( d1 ) };
                CHECK_EQ( &l.front(), &d3 );
                CHECK_EQ( &l.back(), &d2 );
                check_for_each_node( d2, { &d2, &d3 } );
        }
        check_links( d1 );
}

TEST_CASE_TEMPLATE( "triple", T, node_t, der )
{
        T d1, d2, d3;
        SUBCASE( "link as last" )
        {
                link_detached_as_last< T, typename T::access >( d1, d2 );
                link_detached_as_last< T, typename T::access >( d2, d3 );
        }
        SUBCASE( "link as next" )
        {
                link_detached_as_next< T, typename T::access >( d1, d2 );
                link_detached_as_next< T, typename T::access >( d2, d3 );
        }
        check_links( d1 );
        check_for_each_node( d1, { &d1, &d2, &d3 } );

        d2 = T{};
        check_links( d1 );
        check_for_each_node( d1, { &d1, &d3 } );
}

TEST_CASE_TEMPLATE( "cpy", T, node_t, der )
{
        T d1;
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

TEST_CASE_TEMPLATE( "vector", T, node_t, der )
{
        std::vector< T > d1;
        for ( int i = 0; i < 42; i++ ) {
                auto& last = d1.emplace_back();
                if ( i > 0 )
                        link_detached_as_last< T, typename T::access >( d1.front(), last );
                check_links( d1.front() );
        }
        std::set< T const* > s;
        for ( T& d : d1 )
                s.insert( &d );
        check_for_each_node( d1.front(), s );
}

struct cont : public ll_base< cont >
{
        int i = 0;

        cont( int v )
          : i( v )
        {
        }
};

TEST_CASE( "cont" )
{
        cont c1{ 1 }, c2{ 2 }, c3{ 3 };

        ll_list< cont > l = { &c1, &c2, &c3 };

        {
                std::vector< int > x;
                cont               c20{ c2 };
                for_each_node( c1, [&]( cont& c ) {
                        x.emplace_back( c.i );
                } );
                CHECK_EQ( x, std::vector< int >{ 1, 2, 2, 3 } );
        }
}

TEST_CASE_TEMPLATE( "iters", T, node_t, der )
{
        T            c1, c2, c3;
        ll_list< T > l = { &c1, &c2, &c3 };

        static_assert( std::forward_iterator< ll_iterator< T > > );

        std::vector< T const* > expected = { &c1, &c2, &c3 };
        std::vector< T const* > s;
        for ( T& c : l )
                s.push_back( &c );

        CHECK_EQ( s, expected );

        std::vector< T const* > s2;
        for ( T const& c : std::as_const( l ) )
                s2.push_back( &c );

        CHECK_EQ( s2, expected );
}

template < typename T >
void check_list_ptr( ll_list< T > const& l, std::vector< T const* > const& expected )
{
        check_links( l.front() );
        std::vector< T const* > result;
        for ( T const& item : l )
                result.push_back( &item );
        CHECK_EQ( result, expected );
};

template < typename T, typename Acc = typename T::access >
void check_nodes_ptr( T const& l, std::vector< T const* > const& expected )
{
        check_links( l );
        std::vector< T const* > result;
        for ( auto* n = &l; n; n = _node( Acc::get( *n ).next ) )
                result.push_back( n );
        CHECK_EQ( result, expected );
};

template < typename U, typename T >
void check_list_values( ll_list< T > const& l, std::vector< U > const& expected )
{
        check_links( l.front() );
        std::vector< U > result;
        for ( T const& item : l )
                result.push_back( item.value );
        CHECK_EQ( result, expected );
};

TEST_CASE_TEMPLATE( "merge", T, node_t, der )
{
        using access = typename T::access;

        SUBCASE( "merge empty lists" )
        {
                ll_list< T > l1, l2;
                l1.merge( std::move( l2 ) );
                CHECK( l1.empty() );
                CHECK( l2.empty() );
        }

        SUBCASE( "merge empty into non-empty" )
        {
                T            d1, d2;
                ll_list< T > l1 = { &d1, &d2 }, l2;

                l1.merge( std::move( l2 ) );
                CHECK( l2.empty() );
                CHECK_EQ( &l1.front(), &d1 );
                CHECK_EQ( &l1.back(), &d2 );

                check_list_ptr( l1, { &d1, &d2 } );
        }

        SUBCASE( "merge non-empty into empty" )
        {
                T            d1, d2;
                ll_list< T > l1, l2 = { &d1, &d2 };

                l1.merge( std::move( l2 ) );
                CHECK( l2.empty() );
                CHECK_EQ( &l1.front(), &d1 );
                CHECK_EQ( &l1.back(), &d2 );

                check_list_ptr( l1, { &d1, &d2 } );
        }

        SUBCASE( "merge self - should be no-op" )
        {
                T            d1, d2;
                ll_list< T > l1 = { &d1, &d2 };

                l1.merge( std::move( l1 ) );
                CHECK_EQ( &l1.front(), &d1 );
                CHECK_EQ( &l1.back(), &d2 );

                check_list_ptr( l1, { &d1, &d2 } );
        }

        SUBCASE( "merge move semantics" )
        {
                T                    d1, d2, d3;
                ll_list< T, access > l1 = { &d1 }, l2 = { &d2, &d3 };

                l1.merge( std::move( l2 ) );
                CHECK( l2.empty() );

                std::vector< T const* > result;
                for ( T const& item : l1 )
                        result.push_back( &item );
                // Order depends on implementation, but should contain all elements
                CHECK_EQ( result.size(), 3 );
        }
}

TEST_CASE( "merge_comparable" )
{
        struct comparable_node : public ll_base< comparable_node >
        {
                int value;

                comparable_node( int v )
                  : value( v )
                {
                }

                bool operator<( comparable_node const& other ) const
                {
                        return value < other.value;
                }

                bool operator>( comparable_node const& other ) const
                {
                        return value > other.value;
                }

                bool operator==( comparable_node const& other ) const
                {
                        return value == other.value;
                }
        };

        SUBCASE( "merge sorted lists - interleaved" )
        {
                comparable_node n1{ 1 }, n2{ 3 }, n3{ 5 };
                comparable_node m1{ 2 }, m2{ 4 }, m3{ 6 };

                ll_list< comparable_node > l1 = { &n1, &n2, &n3 }, l2 = { &m1, &m2, &m3 };

                l1.merge( std::move( l2 ) );
                CHECK( l2.empty() );

                check_list_ptr( l1, { &n1, &m1, &n2, &m2, &n3, &m3 } );
        }

        SUBCASE( "merge sorted lists - all from first list smaller" )
        {
                comparable_node n1( 1 ), n2( 2 ), n3( 3 );
                comparable_node m1( 4 ), m2( 5 ), m3( 6 );

                ll_list< comparable_node > l1 = { &n1, &n2, &n3 }, l2 = { &m1, &m2, &m3 };

                l1.merge( std::move( l2 ) );
                CHECK( l2.empty() );
                check_list_ptr( l1, { &n1, &n2, &n3, &m1, &m2, &m3 } );
        }

        SUBCASE( "merge sorted lists - all from second list smaller" )
        {
                comparable_node n1( 4 ), n2( 5 ), n3( 6 );
                comparable_node m1( 1 ), m2( 2 ), m3( 3 );

                ll_list< comparable_node > l1 = { &n1, &n2, &n3 }, l2 = { &m1, &m2, &m3 };

                l1.merge( std::move( l2 ) );
                CHECK( l2.empty() );

                check_list_ptr( l1, { &m1, &m2, &m3, &n1, &n2, &n3 } );
        }

        SUBCASE( "merge with equal elements" )
        {
                comparable_node n1( 1 ), n2( 3 ), n3( 3 );
                comparable_node m1( 2 ), m2( 3 ), m3( 4 );

                ll_list< comparable_node > l1 = { &n1, &n2, &n3 }, l2 = { &m1, &m2, &m3 };

                l1.merge( std::move( l2 ) );
                CHECK( l2.empty() );

                check_list_values< int >( l1, { 1, 2, 3, 3, 3, 4 } );
        }

        SUBCASE( "merge single element lists" )
        {
                comparable_node n1( 2 );
                comparable_node m1( 1 );

                ll_list< comparable_node > l1 = { &n1 }, l2 = { &m1 };

                l1.merge( std::move( l2 ) );
                CHECK( l2.empty() );

                check_list_ptr( l1, { &m1, &n1 } );
        }

        SUBCASE( "merge different sized lists" )
        {
                comparable_node n1( 1 ), n2( 5 );
                comparable_node m1( 2 ), m2( 3 ), m3( 4 ), m4( 6 ), m5( 7 );

                ll_list< comparable_node > l1 = { &n1, &n2 }, l2 = { &m1, &m2, &m3, &m4, &m5 };

                l1.merge( std::move( l2 ) );
                CHECK( l2.empty() );

                check_list_ptr( l1, { &n1, &m1, &m2, &m3, &n2, &m4, &m5 } );
        }

        SUBCASE( "merge with custom comparator - reverse order" )
        {
                comparable_node n1( 5 ), n2( 3 ), n3( 1 );  // descending
                comparable_node m1( 6 ), m2( 4 ), m3( 2 );  // descending

                ll_list< comparable_node > l1 = { &n1, &n2, &n3 }, l2 = { &m1, &m2, &m3 };

                l1.merge( std::move( l2 ), std::greater<>{} );  // Using custom comparator
                CHECK( l2.empty() );

                check_list_ptr( l1, { &m1, &n1, &m2, &n2, &m3, &n3 } );
        }

        SUBCASE( "merge stability test - equal elements preserve order" )
        {
                // Create nodes with same value but different addresses to test stability
                comparable_node n1( 2 ), n2( 2 );
                comparable_node m1( 2 ), m2( 2 );

                ll_list< comparable_node > l1 = { &n1, &n2 }, l2 = { &m1, &m2 };

                l1.merge( std::move( l2 ) );
                CHECK( l2.empty() );

                std::vector< comparable_node const* > result;
                for ( comparable_node const& item : l1 )
                        result.push_back( &item );

                // Should preserve relative order: first list elements should come first
                // when values are equal (stable merge)
                CHECK_EQ( result.size(), 4 );
                CHECK_EQ( result[0], &n1 );  // First from l1
                CHECK_EQ( result[1], &n2 );  // Second from l1
                CHECK_EQ( result[2], &m1 );  // First from l2
                CHECK_EQ( result[3], &m2 );  // Second from l2
        }

        SUBCASE( "merge edge case - one element vs many" )
        {
                comparable_node n1( 3 );
                comparable_node m1( 1 ), m2( 2 ), m3( 4 ), m4( 5 );

                ll_list< comparable_node > l1 = { &n1 }, l2 = { &m1, &m2, &m3, &m4 };

                l1.merge( std::move( l2 ) );
                CHECK( l2.empty() );

                check_list_ptr( l1, { &m1, &m2, &n1, &m3, &m4 } );
        }

        SUBCASE( "merge undetached" )
        {
                comparable_node d1{ 100 }, d2{ 2 }, d3{ 4 }, d4{ 3 }, d5{ 5 }, d6{ 0 };
                link_group( { &d1, &d2, &d3, &d4, &d5, &d6 } );
                auto [a, b] = merge_ranges< comparable_node, typename comparable_node::access >(
                    d2, d3, d4, d5 );
                check_links( d1 );
                CHECK( a == &d2 );
                CHECK( b == &d5 );
                check_nodes_ptr( d1, { &d1, &d2, &d4, &d3, &d5, &d6 } );
        }
}

TEST_CASE( "iterator_edge_cases" )
{
        SUBCASE( "iterator on empty list" )
        {
                ll_list< node_t > l;
                auto              it = l.begin();
                CHECK_EQ( it, l.end() );
        }

        SUBCASE( "iterator increment/decrement on null" )
        {
                ll_iterator< node_t > it( nullptr );
                CHECK_EQ( it.get(), nullptr );

                // Incrementing null iterator should remain null
                ++it;
                CHECK_EQ( it.get(), nullptr );
        }

        SUBCASE( "iterator boundary conditions" )
        {
                node_t            n1, n2, n3;
                ll_list< node_t > l = { &n1, &n2, &n3 };

                auto it = l.begin();
                CHECK_EQ( it.get(), &n1 );

                // Increment to end
                ++it;
                CHECK_EQ( it.get(), &n2 );
                ++it;
                CHECK_EQ( it.get(), &n3 );
                ++it;
                CHECK_EQ( it, l.end() );
        }

        SUBCASE( "const iterator edge cases" )
        {
                ll_list< node_t > l;
                auto const&       const_l = l;
                auto              it      = const_l.begin();
                CHECK_EQ( it, const_l.end() );

                // Test const iterator increment on null
                ++it;
                CHECK_EQ( it, const_l.end() );
        }

        SUBCASE( "iterator comparison edge cases" )
        {
                ll_iterator< node_t > it1( nullptr );
                ll_iterator< node_t > it2( nullptr );
                CHECK_EQ( it1, it2 );

                node_t                n1;
                ll_iterator< node_t > it3( &n1 );
                CHECK_NE( it1, it3 );
        }
}

TEST_CASE( "ll_list_edge_cases" )
{
        SUBCASE( "move constructor and assignment" )
        {
                node_t            n1, n2;
                ll_list< node_t > l1 = { &n1, &n2 };

                // Test move constructor
                ll_list< node_t > l2( std::move( l1 ) );
                CHECK( l1.empty() );
                CHECK_EQ( &l2.front(), &n1 );
                CHECK_EQ( &l2.back(), &n2 );

                // Test move assignment
                ll_list< node_t > l3;
                l3 = std::move( l2 );
                CHECK( l2.empty() );
                CHECK_EQ( &l3.front(), &n1 );
                CHECK_EQ( &l3.back(), &n2 );

                // Test self-move assignment
                auto& l3_ref = l3;
                l3           = std::move( l3_ref );
                CHECK_EQ( &l3.front(), &n1 );
                CHECK_EQ( &l3.back(), &n2 );
        }

        SUBCASE( "link_front edge cases" )
        {
                ll_list< node_t > l;
                node_t            n1;

                // Test link_front on empty list
                l.link_front( n1 );
                CHECK_EQ( &l.front(), &n1 );
                CHECK_EQ( &l.back(), &n1 );

                // Test link_front when node already in another list
                ll_list< node_t > l2;
                node_t            n2;
                l2.link_back( n2 );
                l.link_front( n2 );  // Should unlink from l2 first
                CHECK( l2.empty() );
                CHECK_EQ( &l.front(), &n2 );
                CHECK_EQ( &l.back(), &n1 );
        }

        SUBCASE( "link_back edge cases" )
        {
                ll_list< node_t > l;
                node_t            n1;

                // Test link_back on empty list
                l.link_back( n1 );
                CHECK_EQ( &l.front(), &n1 );
                CHECK_EQ( &l.back(), &n1 );

                // Test link_back when node already in same list
                l.link_back( n1 );  // Should unlink first, then relink
                CHECK_EQ( &l.front(), &n1 );
                CHECK_EQ( &l.back(), &n1 );
        }

        SUBCASE( "unlink_front/back edge cases" )
        {
                node_t            n1, n2;
                ll_list< node_t > l = { &n1, &n2 };

                // Test unlink_back
                l.detach_back();
                CHECK_EQ( &l.front(), &n1 );
                CHECK_EQ( &l.back(), &n1 );

                // Test unlink_front on single element
                l.detach_front();
                CHECK( l.empty() );
        }

        SUBCASE( "front/back on empty list" )
        {
                ll_list< node_t > l;
        }
}

TEST_CASE( "ll_base_edge_cases" )
{
        SUBCASE( "self assignment" )
        {
                der d1;

                // Test self copy assignment
                der& d1_ref = d1;
                d1          = d1_ref;
                CHECK_EQ( d1.next(), nullptr );
                CHECK_EQ( d1.prev(), nullptr );

                // Test self move assignment
                auto& d1_ref2 = d1;
                d1            = std::move( d1_ref2 );
                CHECK_EQ( d1.next(), nullptr );
                CHECK_EQ( d1.prev(), nullptr );
        }

        SUBCASE( "move from linked node" )
        {
                der d1, d2, d3;
                d1.link_next( d2 );
                d2.link_next( d3 );

                // Move constructor should transfer position
                der d4( std::move( d2 ) );
                CHECK_EQ( d1.next(), &d4 );
                CHECK_EQ( d4.prev(), &d1 );
                CHECK_EQ( d4.next(), &d3 );
                CHECK_EQ( d3.prev(), &d4 );

                // d2 should be isolated
                CHECK_EQ( d2.next(), nullptr );
                CHECK_EQ( d2.prev(), nullptr );
        }

        SUBCASE( "copy from linked node" )
        {
                der d1, d2, d3;
                d1.link_next( d2 );

                // Copy constructor should link after original
                der d4( d2 );
                CHECK_EQ( d1.next(), &d2 );
                CHECK_EQ( d2.prev(), &d1 );
                CHECK_EQ( d2.next(), &d4 );
                CHECK_EQ( d4.prev(), &d2 );
                CHECK_EQ( d4.next(), nullptr );
        }

        SUBCASE( "next/prev on isolated node" )
        {
                der d1;
                CHECK_EQ( d1.next(), nullptr );
                CHECK_EQ( d1.prev(), nullptr );

                // Test const versions
                der const& d1_const = d1;
                CHECK_EQ( d1_const.next(), nullptr );
                CHECK_EQ( d1_const.prev(), nullptr );
        }
}

TEST_CASE( "splice_functionality" )
{
        SUBCASE( "splice basic functionality" )
        {
                der            d1, d2, d3, d4;
                ll_list< der > l1 = { &d1, &d2 }, l2 = { &d3, &d4 };

                // This should splice l2 before d2 (std::list behavior)
                auto pos = l1.begin();
                ++pos;  // Position at d2

                l1.splice( pos, std::move( l2 ) );
                CHECK( l2.empty() );

                check_list_ptr( l1, { &d1, &d3, &d4, &d2 } );
        }

        SUBCASE( "splice empty list" )
        {
                der            d1, d2;
                ll_list< der > l1 = { &d1, &d2 }, l2;

                auto pos = l1.begin();
                ++pos;  // Position at d2

                l1.splice( pos, std::move( l2 ) );
                CHECK( l2.empty() );
                check_list_ptr( l1, { &d1, &d2 } );
        }

        SUBCASE( "splice into empty list" )
        {
                der            d1, d2;
                ll_list< der > l1, l2 = { &d1, &d2 };

                l1.splice( l1.begin(), std::move( l2 ) );
                CHECK( l2.empty() );
                check_list_ptr( l1, { &d1, &d2 } );
        }

        SUBCASE( "splice at beginning" )
        {
                der            d1, d2, d3, d4;
                ll_list< der > l1 = { &d1, &d2 }, l2 = { &d3, &d4 };

                l1.splice( l1.begin(), std::move( l2 ) );
                CHECK( l2.empty() );
                check_list_ptr( l1, { &d3, &d4, &d1, &d2 } );
        }

        SUBCASE( "splice at end" )
        {
                der            d1, d2, d3, d4;
                ll_list< der > l1 = { &d1, &d2 }, l2 = { &d3, &d4 };

                l1.splice( l1.end(), std::move( l2 ) );
                CHECK( l2.empty() );
                check_list_ptr( l1, { &d1, &d2, &d3, &d4 } );
        }

        SUBCASE( "splice in middle" )
        {
                der            d1, d2, d3, d4, d5;
                ll_list< der > l1 = { &d1, &d2, &d3 }, l2 = { &d4, &d5 };

                auto pos = l1.begin();
                ++pos;  // Position at d2
                ++pos;  // Position at d3

                l1.splice( pos, std::move( l2 ) );
                CHECK( l2.empty() );
                check_list_ptr( l1, { &d1, &d2, &d4, &d5, &d3 } );
        }

        SUBCASE( "splice single element" )
        {
                der            d1, d2, d3;
                ll_list< der > l1 = { &d1, &d2 }, l2 = { &d3 };

                auto pos = l1.begin();
                ++pos;  // Position at d2

                l1.splice( pos, std::move( l2 ) );
                CHECK( l2.empty() );
                check_list_ptr( l1, { &d1, &d3, &d2 } );
        }

        SUBCASE( "splice self - should be no-op" )
        {
                der            d1, d2;
                ll_list< der > l1 = { &d1, &d2 };

                auto pos = l1.begin();
                ++pos;  // Position at d2

                l1.splice( pos, std::move( l1 ) );
                check_list_ptr( l1, { &d1, &d2 } );
        }

        SUBCASE( "splice maintains order" )
        {
                der            d1, d2, d3, d4, d5, d6;
                ll_list< der > l1 = { &d1, &d2, &d3 }, l2 = { &d4, &d5, &d6 };

                auto pos = l1.begin();
                ++pos;  // Position at d2

                l1.splice( pos, std::move( l2 ) );
                CHECK( l2.empty() );
                check_list_ptr( l1, { &d1, &d4, &d5, &d6, &d2, &d3 } );
        }

        SUBCASE( "splice preserves iterator validity" )
        {
                der            d1, d2, d3, d4;
                ll_list< der > l1 = { &d1, &d2 }, l2 = { &d3, &d4 };

                auto pos = l1.begin();
                ++pos;  // Position at d2
                auto d2_iter = pos;

                l1.splice( pos, std::move( l2 ) );

                // Iterator to d2 should still be valid
                CHECK_EQ( &( *d2_iter ), &d2 );
                check_list_ptr( l1, { &d1, &d3, &d4, &d2 } );
        }

        SUBCASE( "splice with different node types" )
        {
                // Test with node_t as well
                node_t            n1, n2, n3, n4;
                ll_list< node_t > l1 = { &n1, &n2 }, l2 = { &n3, &n4 };

                auto pos = l1.begin();
                ++pos;  // Position at n2

                l1.splice( pos, std::move( l2 ) );
                CHECK( l2.empty() );

                check_list_ptr( l1, { &n1, &n3, &n4, &n2 } );
        }

        SUBCASE( "splice large lists" )
        {
                ll_list< der >     l1, l2;
                std::vector< der > nodes1( 100 );
                std::vector< der > nodes2( 150 );

                for ( auto& node : nodes1 )
                        l1.link_back( node );
                for ( auto& node : nodes2 )
                        l2.link_back( node );

                auto pos = l1.begin();
                std::advance( pos, 50 );  // Position at middle of l1

                l1.splice( pos, std::move( l2 ) );
                CHECK( l2.empty() );

                // Check that we have all elements
                size_t count = 0;
                for ( [[maybe_unused]] auto const& item : l1 )
                        ++count;
                CHECK_EQ( count, 250 );

                // Verify that the spliced elements are in the right position
                auto it = l1.begin();
                std::advance( it, 50 );  // Should be at first element from l2
                CHECK_EQ( it.get(), &nodes2[0] );
        }

        SUBCASE( "splice preserves links" )
        {
                der            d1, d2, d3, d4;
                ll_list< der > l1 = { &d1, &d2 }, l2 = { &d3, &d4 };

                auto pos = l1.begin();

                l1.splice( pos, std::move( l2 ) );
                CHECK( l2.empty() );

                // Verify that the links are correct
                check_links( d3 );  // Check from first element
        }

        SUBCASE( "splice entire list at various positions" )
        {
                der            d1, d2, d3, d4, d5;
                ll_list< der > l1, l2;
                l1.link_back( d1 );
                l1.link_back( d2 );
                l1.link_back( d3 );
                l2.link_back( d4 );
                l2.link_back( d5 );

                // Test splicing at each position
                auto pos = l1.begin();
                l1.splice( pos, std::move( l2 ) );  // Splice at beginning
                CHECK( l2.empty() );
                check_list_ptr( l1, { &d4, &d5, &d1, &d2, &d3 } );

                // Reset for next test
                l2.link_back( d4 );
                l2.link_back( d5 );
                l1.link_back( d1 );
                l1.link_back( d2 );
                l1.link_back( d3 );

                pos = l1.begin();
                ++pos;  // Position at d2
                l1.splice( pos, std::move( l2 ) );
                CHECK( l2.empty() );
                check_list_ptr( l1, { &d1, &d4, &d5, &d2, &d3 } );
        }

        SUBCASE( "splice stress test - multiple operations" )
        {
                std::vector< der > nodes( 20 );
                ll_list< der >     l1, l2;

                // Build initial lists
                for ( std::size_t i = 0; i < 10; ++i )
                        l1.link_back( nodes[i] );
                for ( std::size_t i = 10; i < 20; ++i )
                        l2.link_back( nodes[i] );

                // Splice at different positions
                auto pos = l1.begin();
                std::advance( pos, 5 );
                l1.splice( pos, std::move( l2 ) );
                CHECK( l2.empty() );

                // Verify all elements are present
                std::set< der const* > expected;
                for ( auto const& node : nodes )
                        expected.insert( &node );

                std::set< der const* > actual;
                for ( auto const& item : l1 )
                        actual.insert( &item );

                CHECK_EQ( actual, expected );
        }

        SUBCASE( "splice maintains front/back pointers" )
        {
                der            d1, d2, d3, d4;
                ll_list< der > l1, l2;
                l1.link_back( d1 );
                l1.link_back( d2 );
                l2.link_back( d3 );
                l2.link_back( d4 );

                // Splice at beginning - should change front
                l1.splice( l1.begin(), std::move( l2 ) );
                CHECK( l2.empty() );
                CHECK_EQ( &l1.front(), &d3 );
                CHECK_EQ( &l1.back(), &d2 );

                // Reset and test splice at end
                l2.link_back( d3 );
                l2.link_back( d4 );
                l1.link_back( d1 );
                l1.link_back( d2 );
                l1.link_back( d3 );

                l1.splice( l1.end(), std::move( l2 ) );
                CHECK( l2.empty() );
                CHECK_EQ( &l1.front(), &d1 );
                CHECK_EQ( &l1.back(), &d4 );
        }

        SUBCASE( "splice with single element in destination" )
        {
                der            d1, d2, d3;
                ll_list< der > l1 = { &d1 }, l2 = { &d2, &d3 };

                // Splice before single element
                l1.splice( l1.begin(), std::move( l2 ) );
                CHECK( l2.empty() );
                check_list_ptr( l1, { &d2, &d3, &d1 } );

                // Reset and splice after single element
                l2.link_back( d2 );
                l2.link_back( d3 );
                l1.link_back( d1 );

                l1.splice( l1.end(), std::move( l2 ) );
                CHECK( l2.empty() );
                check_list_ptr( l1, { &d1, &d2, &d3 } );
        }
}

TEST_CASE( "remove_functionality" )
{
        struct removable_node : public ll_base< removable_node >
        {
                int value;

                removable_node( int v )
                  : value( v )
                {
                }

                removable_node( removable_node const& other ) = delete;

                bool operator==( removable_node const& other ) const
                {
                        return value == other.value;
                }
        };

        using access = typename removable_node::access;

        SUBCASE( "remove from empty list" )
        {
                ll_list< removable_node > l;
                auto                      count = l.remove_if( []( auto& ) {
                        return true;
                } );
                CHECK_EQ( count, 0 );
                CHECK( l.empty() );
        }

        SUBCASE( "remove non-existent element" )
        {
                removable_node            n1( 1 ), n2( 2 );
                ll_list< removable_node > l = { &n1, &n2 };

                auto count = l.remove_if( []( auto& node ) {
                        return node.value == 3;
                } );
                CHECK_EQ( count, 0 );
                check_list_ptr( l, { &n1, &n2 } );
        }

        SUBCASE( "remove first element" )
        {
                removable_node            n1( 1 ), n2( 2 ), n3( 3 );
                ll_list< removable_node > l = { &n1, &n2, &n3 };

                auto count = l.remove_if( []( auto& node ) {
                        return node.value == 1;
                } );
                CHECK_EQ( count, 1 );
                check_list_ptr( l, { &n2, &n3 } );
                CHECK_EQ( &l.front(), &n2 );
        }

        SUBCASE( "remove last element" )
        {
                removable_node            n1( 1 ), n2( 2 ), n3( 3 );
                ll_list< removable_node > l = { &n1, &n2, &n3 };

                auto count = l.remove_if( []( auto& node ) {
                        return node.value == 3;
                } );
                CHECK_EQ( count, 1 );
                check_list_ptr( l, { &n1, &n2 } );
                CHECK_EQ( &l.back(), &n2 );
        }

        SUBCASE( "remove middle element" )
        {
                removable_node            n1( 1 ), n2( 2 ), n3( 3 );
                ll_list< removable_node > l = { &n1, &n2, &n3 };

                auto count = l.remove_if( []( auto& node ) {
                        return node.value == 2;
                } );
                CHECK_EQ( count, 1 );
                check_list_ptr( l, { &n1, &n3 } );
        }

        SUBCASE( "remove multiple elements" )
        {
                removable_node            n1( 1 ), n2( 2 ), n3( 1 ), n4( 3 );
                ll_list< removable_node > l = { &n1, &n2, &n3, &n4 };

                auto count = l.remove_if( []( auto& node ) {
                        return node.value == 1;
                } );
                CHECK_EQ( count, 2 );
                check_list_ptr( l, { &n2, &n4 } );
        }

        SUBCASE( "remove all elements" )
        {
                removable_node            n1( 1 ), n2( 1 ), n3( 1 );
                ll_list< removable_node > l = { &n1, &n2, &n3 };

                auto count = l.remove_if( []( auto& node ) {
                        return node.value == 1;
                } );
                CHECK_EQ( count, 3 );
                CHECK( l.empty() );
        }

        SUBCASE( "remove by value" )
        {
                removable_node            n1( 1 ), n2( 2 ), n3( 1 ), n4( 3 );
                ll_list< removable_node > l = { &n1, &n2, &n3, &n4 };

                auto count = l.remove( removable_node( 1 ) );
                CHECK_EQ( count, 2 );
                check_list_ptr( l, { &n2, &n4 } );
        }

        SUBCASE( "remove_if with stateful predicate" )
        {
                removable_node            n1( 1 ), n2( 2 ), n3( 3 ), n4( 4 );
                ll_list< removable_node > l = { &n1, &n2, &n3, &n4 };

                struct Pred
                {
                        bool flag = false;

                        bool operator()( removable_node& )
                        {
                                flag = !flag;
                                return flag;
                        }
                };

                auto count = l.remove_if( Pred{} );
                CHECK_EQ( count, 2 );
                check_list_ptr( l, { &n2, &n4 } );
        }

        SUBCASE( "remove nodes" )
        {
                removable_node n1( 2 ), n2( 2 ), n3( 2 ), n4( 2 );
                link_group( { &n1, &n2, &n3, &n4 } );

                range_remove< removable_node, access >( n2, n3, [&]( auto& node ) {
                        return node.value == 2;
                } );

                check_nodes_ptr( n1, { &n1, &n4 } );
        }
}

TEST_CASE( "find_if_node_functionality" )
{
        struct findable_node : public ll_base< findable_node >
        {
                int value;

                findable_node( int v )
                  : value( v )
                {
                }
        };

        SUBCASE( "find in list with single element" )
        {
                findable_node n1( 1 );
                auto*         found = find_if_node( n1, []( auto& ) {
                        return true;
                } );
                CHECK_EQ( found, &n1 );
        }

        SUBCASE( "find non-existent" )
        {
                findable_node            n1( 1 ), n2( 2 ), n3( 3 );
                ll_list< findable_node > l = { &n1, &n2, &n3 };

                auto* found = find_if_node( n1, []( auto& node ) {
                        return node.value == 4;
                } );
                CHECK_EQ( found, nullptr );
        }

        SUBCASE( "find first element" )
        {
                findable_node            n1( 1 ), n2( 2 ), n3( 3 );
                ll_list< findable_node > l = { &n1, &n2, &n3 };

                auto* found = find_if_node( n2, []( auto& node ) {
                        return node.value == 1;
                } );
                CHECK_EQ( found, &n1 );
        }

        SUBCASE( "find last element" )
        {
                findable_node            n1( 1 ), n2( 2 ), n3( 3 );
                ll_list< findable_node > l = { &n1, &n2, &n3 };

                auto* found = find_if_node( n1, []( auto& node ) {
                        return node.value == 3;
                } );
                CHECK_EQ( found, &n3 );
        }

        SUBCASE( "find middle element" )
        {
                findable_node            n1( 1 ), n2( 2 ), n3( 3 );
                ll_list< findable_node > l = { &n1, &n2, &n3 };

                auto* found = find_if_node( n1, []( auto& node ) {
                        return node.value == 2;
                } );
                CHECK_EQ( found, &n2 );
        }

        SUBCASE( "find with stateful predicate" )
        {
                findable_node            n1( 1 ), n2( 2 ), n3( 3 ), n4( 4 );
                ll_list< findable_node > l = { &n1, &n2, &n3, &n4 };

                struct Pred
                {
                        int count = 0;

                        bool operator()( findable_node& )
                        {
                                return ++count == 3;
                        }
                };

                auto* found = find_if_node( n1, Pred{} );
                CHECK_EQ( found, &n3 );
        }
}

TEST_CASE( "reverse_functionality" )
{
        struct reversible_node : public ll_base< reversible_node >
        {
                int value;

                reversible_node( int v )
                  : value( v )
                {
                }
        };

        SUBCASE( "reverse empty list" )
        {
                ll_list< reversible_node > l;
                l.reverse();
                CHECK( l.empty() );
        }

        SUBCASE( "reverse single element list" )
        {
                reversible_node            n1( 1 );
                ll_list< reversible_node > l = { &n1 };
                l.reverse();
                check_list_ptr( l, { &n1 } );
                CHECK_EQ( &l.front(), &n1 );
                CHECK_EQ( &l.back(), &n1 );
        }

        SUBCASE( "reverse two elements" )
        {
                reversible_node            n1( 1 ), n2( 2 );
                ll_list< reversible_node > l = { &n1, &n2 };
                l.reverse();
                check_list_ptr( l, { &n2, &n1 } );
                CHECK_EQ( &l.front(), &n2 );
                CHECK_EQ( &l.back(), &n1 );
        }

        SUBCASE( "reverse multiple elements" )
        {
                reversible_node            n1( 1 ), n2( 2 ), n3( 3 ), n4( 4 );
                ll_list< reversible_node > l = { &n1, &n2, &n3, &n4 };
                l.reverse();
                check_list_ptr( l, { &n4, &n3, &n2, &n1 } );
                CHECK_EQ( &l.front(), &n4 );
                CHECK_EQ( &l.back(), &n1 );
        }

        SUBCASE( "reverse subrange" )
        {
                reversible_node n1( 1 ), n2( 2 ), n3( 3 ), n4( 4 );
                link_group( { &n1, &n2, &n3, &n4 } );
                range_reverse( n2, n3 );

                check_nodes_ptr( n1, { &n1, &n3, &n2, &n4 } );
        }
}

TEST_CASE( "unique_functionality" )
{
        struct unique_node : public ll_base< unique_node >
        {
                int value;

                unique_node( int v )
                  : value( v )
                {
                }

                bool operator==( unique_node const& other ) const
                {
                        return value == other.value;
                }
        };

        SUBCASE( "unique empty list" )
        {
                ll_list< unique_node > l;
                auto                   count = l.unique();
                CHECK_EQ( count, 0 );
                CHECK( l.empty() );
        }

        SUBCASE( "unique single element list" )
        {
                unique_node            n1( 1 );
                ll_list< unique_node > l     = { &n1 };
                auto                   count = l.unique();
                CHECK_EQ( count, 0 );
                check_list_ptr( l, { &n1 } );
                CHECK_EQ( &l.front(), &n1 );
                CHECK_EQ( &l.back(), &n1 );
        }

        SUBCASE( "unique no duplicates" )
        {
                unique_node            n1( 1 ), n2( 2 ), n3( 3 );
                ll_list< unique_node > l     = { &n1, &n2, &n3 };
                auto                   count = l.unique();
                CHECK_EQ( count, 0 );
                check_list_ptr( l, { &n1, &n2, &n3 } );
                CHECK_EQ( &l.front(), &n1 );
                CHECK_EQ( &l.back(), &n3 );
        }

        SUBCASE( "unique all duplicates" )
        {
                unique_node            n1( 1 ), n2( 1 ), n3( 1 );
                ll_list< unique_node > l     = { &n1, &n2, &n3 };
                auto                   count = l.unique();
                CHECK_EQ( count, 2 );
                check_list_ptr( l, { &n1 } );
                CHECK_EQ( &l.front(), &n1 );
                CHECK_EQ( &l.back(), &n1 );
        }

        SUBCASE( "unique duplicates at beginning" )
        {
                unique_node            n1( 1 ), n2( 1 ), n3( 2 ), n4( 3 );
                ll_list< unique_node > l     = { &n1, &n2, &n3, &n4 };
                auto                   count = l.unique();
                CHECK_EQ( count, 1 );
                check_list_ptr( l, { &n1, &n3, &n4 } );
                CHECK_EQ( &l.front(), &n1 );
                CHECK_EQ( &l.back(), &n4 );
        }

        SUBCASE( "unique duplicates at end" )
        {
                unique_node            n1( 1 ), n2( 2 ), n3( 3 ), n4( 3 );
                ll_list< unique_node > l     = { &n1, &n2, &n3, &n4 };
                auto                   count = l.unique();
                CHECK_EQ( count, 1 );
                check_list_ptr( l, { &n1, &n2, &n3 } );
                CHECK_EQ( &l.front(), &n1 );
                CHECK_EQ( &l.back(), &n3 );
        }

        SUBCASE( "unique duplicates in middle" )
        {
                unique_node            n1( 1 ), n2( 2 ), n3( 2 ), n4( 3 );
                ll_list< unique_node > l     = { &n1, &n2, &n3, &n4 };
                auto                   count = l.unique();
                CHECK_EQ( count, 1 );
                check_list_ptr( l, { &n1, &n2, &n4 } );
                CHECK_EQ( &l.front(), &n1 );
                CHECK_EQ( &l.back(), &n4 );
        }

        SUBCASE( "unique multiple groups of duplicates" )
        {
                unique_node n1( 1 ), n2( 1 ), n3( 2 ), n4( 3 ), n5( 3 ), n6( 3 ), n7( 4 );
                ll_list< unique_node > l     = { &n1, &n2, &n3, &n4, &n5, &n6, &n7 };
                auto                   count = l.unique();
                CHECK_EQ( count, 3 );  // n2, n5, n6 removed
                check_list_ptr( l, { &n1, &n3, &n4, &n7 } );
                CHECK_EQ( &l.front(), &n1 );
                CHECK_EQ( &l.back(), &n7 );
        }

        SUBCASE( "unique with custom predicate" )
        {
                unique_node            n1( 1 ), n2( 2 ), n3( 3 ), n4( 4 );
                ll_list< unique_node > l = { &n1, &n2, &n3, &n4 };

                // Custom predicate: consecutive elements are "equal" if their sum is even
                auto count = l.unique( []( unique_node const& a, unique_node const& b ) {
                        return ( a.value + b.value ) % 2 == 0;
                } );
                // 1+2=3 (odd), 2+3=5 (odd), 3+4=7 (odd) - no removals
                CHECK_EQ( count, 0 );
                check_list_ptr( l, { &n1, &n2, &n3, &n4 } );
        }

        SUBCASE( "unique with custom predicate removing all" )
        {
                unique_node            n1( 1 ), n2( 2 ), n3( 3 );
                ll_list< unique_node > l = { &n1, &n2, &n3 };

                // Custom predicate: always true (all consecutive elements are "equal")
                auto count = l.unique( []( unique_node const&, unique_node const& ) {
                        return true;
                } );
                CHECK_EQ( count, 2 );  // n2, n3 removed
                check_list_ptr( l, { &n1 } );
                CHECK_EQ( &l.front(), &n1 );
                CHECK_EQ( &l.back(), &n1 );
        }

        SUBCASE( "unique large list with patterns" )
        {
                unique_node n1( 1 ), n2( 1 ), n3( 1 ), n4( 2 ), n5( 3 ), n6( 3 ), n7( 4 ), n8( 4 ),
                    n9( 4 ), n10( 5 );
                ll_list< unique_node > l = { &n1, &n2, &n3, &n4, &n5, &n6, &n7, &n8, &n9, &n10 };
                auto                   count = l.unique();
                CHECK_EQ( count, 5 );  // n2, n3, n6, n8, n9 removed
                check_list_ptr( l, { &n1, &n4, &n5, &n7, &n10 } );
                CHECK_EQ( &l.front(), &n1 );
                CHECK_EQ( &l.back(), &n10 );
        }

        SUBCASE( "unique two element list with duplicates" )
        {
                unique_node            n1( 1 ), n2( 1 );
                ll_list< unique_node > l     = { &n1, &n2 };
                auto                   count = l.unique();
                CHECK_EQ( count, 1 );
                check_list_ptr( l, { &n1 } );
                CHECK_EQ( &l.front(), &n1 );
                CHECK_EQ( &l.back(), &n1 );
        }

        SUBCASE( "unique two element list without duplicates" )
        {
                unique_node            n1( 1 ), n2( 2 );
                ll_list< unique_node > l     = { &n1, &n2 };
                auto                   count = l.unique();
                CHECK_EQ( count, 0 );
                check_list_ptr( l, { &n1, &n2 } );
                CHECK_EQ( &l.front(), &n1 );
                CHECK_EQ( &l.back(), &n2 );
        }

        SUBCASE( "unique subrange" )
        {
                unique_node n1( 1 ), n2( 1 ), n3( 1 ), n4( 3 );
                link_group( { &n1, &n2, &n3, &n4 } );

                range_unique< unique_node, typename unique_node::access >( n2, n3 );

                check_nodes_ptr( n1, { &n1, &n2, &n4 } );
        }
}

TEST_CASE( "sort_functionality" )
{
        struct sortable_node : public ll_base< sortable_node >
        {
                int value;

                sortable_node( int v )
                  : value( v )
                {
                }

                bool operator<( sortable_node const& other ) const
                {
                        return value < other.value;
                }

                bool operator>( sortable_node const& other ) const
                {
                        return value > other.value;
                }

                bool operator==( sortable_node const& other ) const
                {
                        return value == other.value;
                }
        };

        SUBCASE( "sort empty list" )
        {
                ll_list< sortable_node > l;
                l.sort();
                CHECK( l.empty() );
        }

        SUBCASE( "sort single element list" )
        {
                sortable_node            n1( 5 );
                ll_list< sortable_node > l = { &n1 };
                l.sort();
                check_list_ptr( l, { &n1 } );
                CHECK_EQ( &l.front(), &n1 );
                CHECK_EQ( &l.back(), &n1 );
        }

        SUBCASE( "sort two elements - already sorted" )
        {
                sortable_node            n1( 1 ), n2( 2 );
                ll_list< sortable_node > l = { &n1, &n2 };
                l.sort();
                check_list_ptr( l, { &n1, &n2 } );
                CHECK_EQ( &l.front(), &n1 );
                CHECK_EQ( &l.back(), &n2 );
        }

        SUBCASE( "sort two elements - reverse order" )
        {
                sortable_node            n1( 2 ), n2( 1 );
                ll_list< sortable_node > l = { &n1, &n2 };
                l.sort();
                check_list_ptr( l, { &n2, &n1 } );
                CHECK_EQ( &l.front(), &n2 );
                CHECK_EQ( &l.back(), &n1 );
        }

        SUBCASE( "sort already sorted list" )
        {
                sortable_node            n1( 1 ), n2( 2 ), n3( 3 ), n4( 4 );
                ll_list< sortable_node > l = { &n1, &n2, &n3, &n4 };
                l.sort();
                check_list_ptr( l, { &n1, &n2, &n3, &n4 } );
                CHECK_EQ( &l.front(), &n1 );
                CHECK_EQ( &l.back(), &n4 );
        }

        SUBCASE( "sort reverse sorted list" )
        {
                sortable_node            n1( 4 ), n2( 3 ), n3( 2 ), n4( 1 );
                ll_list< sortable_node > l = { &n1, &n2, &n3, &n4 };
                l.sort();
                check_list_ptr( l, { &n4, &n3, &n2, &n1 } );
                CHECK_EQ( &l.front(), &n4 );
                CHECK_EQ( &l.back(), &n1 );
        }

        SUBCASE( "sort random order" )
        {
                sortable_node            n1( 3 ), n2( 1 ), n3( 4 ), n4( 2 );
                ll_list< sortable_node > l = { &n1, &n2, &n3, &n4 };
                l.sort();
                check_list_ptr( l, { &n2, &n4, &n1, &n3 } );
                CHECK_EQ( &l.front(), &n2 );
                CHECK_EQ( &l.back(), &n3 );
        }

        SUBCASE( "sort with duplicates" )
        {
                sortable_node            n1( 3 ), n2( 1 ), n3( 3 ), n4( 2 ), n5( 1 );
                ll_list< sortable_node > l = { &n1, &n2, &n3, &n4, &n5 };
                l.sort();

                // Check that values are sorted
                std::vector< int > sorted_values;
                for ( auto const& node : l )
                        sorted_values.push_back( node.value );
                CHECK_EQ( sorted_values, std::vector< int >{ 1, 1, 2, 3, 3 } );

                // First and last should be correct
                CHECK_EQ( l.front().value, 1 );
                CHECK_EQ( l.back().value, 3 );
        }

        SUBCASE( "sort all same elements" )
        {
                sortable_node            n1( 5 ), n2( 5 ), n3( 5 );
                ll_list< sortable_node > l = { &n1, &n2, &n3 };
                l.sort();

                // Check that all values are still 5
                for ( auto const& node : l )
                        CHECK_EQ( node.value, 5 );

                CHECK_EQ( l.front().value, 5 );
                CHECK_EQ( l.back().value, 5 );
        }

        SUBCASE( "sort with custom comparator - descending" )
        {
                sortable_node            n1( 1 ), n2( 3 ), n3( 2 ), n4( 4 );
                ll_list< sortable_node > l = { &n1, &n2, &n3, &n4 };

                l.sort( std::greater<>{} );

                check_list_values< int >( l, { 4, 3, 2, 1 } );

                CHECK_EQ( l.front().value, 4 );
                CHECK_EQ( l.back().value, 1 );
        }

        SUBCASE( "sort with custom comparator - by absolute value" )
        {
                struct abs_node : public ll_base< abs_node >
                {
                        int value;

                        abs_node( int v )
                          : value( v )
                        {
                        }
                };

                abs_node            n1( -3 ), n2( 1 ), n3( -2 ), n4( 4 );
                ll_list< abs_node > l = { &n1, &n2, &n3, &n4 };

                l.sort( []( abs_node const& a, abs_node const& b ) {
                        return std::abs( a.value ) < std::abs( b.value );
                } );

                check_list_values< int >( l, { 1, -2, -3, 4 } );
        }

        SUBCASE( "sort large list" )
        {
                std::vector< sortable_node > nodes;
                std::vector< int > values = { 15, 3, 9, 1, 5, 8, 2, 6, 4, 7, 10, 14, 12, 11, 13 };

                for ( int val : values )
                        nodes.emplace_back( val );

                ll_list< sortable_node > l;
                for ( auto& node : nodes )
                        l.link_back( node );

                l.sort();

                std::vector< int > sorted_values;
                for ( auto const& node : l )
                        sorted_values.push_back( node.value );

                std::vector< int > expected = values;
                std::sort( expected.begin(), expected.end() );
                CHECK_EQ( sorted_values, expected );

                CHECK_EQ( l.front().value, 1 );
                CHECK_EQ( l.back().value, 15 );
        }

        SUBCASE( "sort stability test - equal elements preserve relative order" )
        {
                struct stable_node : public ll_base< stable_node >
                {
                        int value;
                        int id;  // For tracking original order

                        stable_node( int v, int i )
                          : value( v )
                          , id( i )
                        {
                        }
                };

                stable_node            n1( 2, 1 ), n2( 1, 2 ), n3( 2, 3 ), n4( 1, 4 );
                ll_list< stable_node > l = { &n1, &n2, &n3, &n4 };

                l.sort( []( stable_node const& a, stable_node const& b ) {
                        return a.value < b.value;
                } );

                std::vector< int > values, ids;
                for ( auto const& node : l ) {
                        values.push_back( node.value );
                        ids.push_back( node.id );
                }

                // Values should be sorted
                CHECK_EQ( values, std::vector< int >{ 1, 1, 2, 2 } );

                // Check that nodes with value 1 come before nodes with value 2
                auto it = l.begin();
                CHECK_EQ( it->value, 1 );
                ++it;
                CHECK_EQ( it->value, 1 );
                ++it;
                CHECK_EQ( it->value, 2 );
                ++it;
                CHECK_EQ( it->value, 2 );
        }

        SUBCASE( "sort maintains list integrity" )
        {
                sortable_node            n1( 3 ), n2( 1 ), n3( 4 ), n4( 2 );
                ll_list< sortable_node > l = { &n1, &n2, &n3, &n4 };

                l.sort();

                // Verify list links are correct
                auto it = l.begin();
                CHECK_EQ( it->value, 1 );
                ++it;
                CHECK_EQ( it->value, 2 );
                ++it;
                CHECK_EQ( it->value, 3 );
                ++it;
                CHECK_EQ( it->value, 4 );
                ++it;
                CHECK_EQ( it, l.end() );

                // Verify front/back pointers
                CHECK_EQ( l.front().value, 1 );
                CHECK_EQ( l.back().value, 4 );

                // Verify all nodes are still present
                std::set< sortable_node const* > expected_nodes = { &n1, &n2, &n3, &n4 };
                std::set< sortable_node const* > actual_nodes;
                for ( auto const& node : l )
                        actual_nodes.insert( &node );
                CHECK_EQ( actual_nodes, expected_nodes );
        }

        SUBCASE( "sort edge case - alternating pattern" )
        {
                sortable_node            n1( 1 ), n2( 10 ), n3( 2 ), n4( 9 ), n5( 3 ), n6( 8 );
                ll_list< sortable_node > l = { &n1, &n2, &n3, &n4, &n5, &n6 };

                l.sort();

                check_list_values< int >( l, { 1, 2, 3, 8, 9, 10 } );
        }
}

}  // namespace zll
