
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "zll.h"

#include <doctest/doctest.h>
#include <set>
#include <vector>

namespace zll
{
namespace
{
struct hdr_access
{
        static auto& get( auto* item ) noexcept
        {
                return item->hdr;
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
                link_empty_as_next< node_t, hdr_access >( o, *this );
        }

        node_t& operator=( node_t&& o ) noexcept
        {
                unlink< node_t, hdr_access >( *this );
                move_from_to< node_t, hdr_access >( o, *this );
                return *this;
        }

        node_t& operator=( node_t& o ) noexcept
        {
                unlink< node_t, hdr_access >( *this );
                link_empty_as_next< node_t, hdr_access >( o, *this );
                return *this;
        }
};

struct der : public ll_base< der >
{
};

void check_links( node_t& first )
{
        for ( node_t* p = first.hdr.next.node(); p; p = p->hdr.next.node() ) {
                CHECK( p->hdr.prev.node() );
                CHECK_EQ( p, hdr_access::get( p->hdr.prev.node() ).next.node() );
        }
}

void check_links( der& first )
{
        using A = typename der::access;
        for ( der* p = first.next(); p; p = p->next() ) {
                CHECK( p->prev() );
                CHECK_EQ( p, p->prev()->next() );
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
        for_each_node< hdr_access >( n, [&]( node_t& m ) {
                s.insert( &m );
        } );
        CAPTURE( s.size() );
        CAPTURE( expected.size() );
        CHECK_EQ( s, expected );

        std::set< node_t const* > s2;
        for_each_node< hdr_access >( std::as_const( n ), [&]( node_t const& m ) {
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
                l.link_empty_back( d1 );

                CHECK_EQ( l.first, &d1 );
                CHECK_EQ( l.last, &d1 );
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
                link_empty_as_last< T, typename T::access >( d1, d2 );
                check_for_each_node( d1, { &d1, &d2 } );
        }
        SUBCASE( "move" )
        {
                ll_list< T, access > l;
                l.link_back( d1 );
                l.link_back( d2 );
                T d3{ std::move( d1 ) };
                CHECK_EQ( l.first, &d3 );
                CHECK_EQ( l.last, &d2 );
                check_for_each_node( d2, { &d2, &d3 } );
        }
        check_links( d1 );
}

TEST_CASE_TEMPLATE( "triple", T, node_t, der )
{
        node_t d1, d2, d3;
        SUBCASE( "link as last" )
        {
                link_empty_as_last< node_t, hdr_access >( d1, d2 );
                link_empty_as_last< node_t, hdr_access >( d2, d3 );
        }
        SUBCASE( "link as next" )
        {
                link_empty_as_next< node_t, hdr_access >( d1, d2 );
                link_empty_as_next< node_t, hdr_access >( d2, d3 );
        }
        check_links( d1 );
        check_for_each_node( d1, { &d1, &d2, &d3 } );

        d2 = node_t{};
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
                        link_empty_as_last< T, typename T::access >( d1.front(), last );
                check_links( d1.front() );
        }
        std::set< T const* > s;
        for ( T& d : d1 )
                s.insert( &d );
        check_for_each_node( d1.front(), s );
}

}  // namespace zll