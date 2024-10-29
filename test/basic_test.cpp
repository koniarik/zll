
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "zll.h"

#include <doctest/doctest.h>
#include <set>
#include <vector>

namespace zll
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

        node_t() noexcept = default;
        node_t( node_t&& o ) noexcept
        {
                move_from_to< hdr_access >( o, *this );
        }
        node_t( node_t& o ) noexcept
        {
                link_empty_as_next< hdr_access >( o, *this );
        }
        node_t& operator=( node_t&& o ) noexcept
        {
                unlink< hdr_access >( *this );
                move_from_to< hdr_access >( o, *this );
                return *this;
        }
        node_t& operator=( node_t& o ) noexcept
        {
                unlink< hdr_access >( *this );
                link_empty_as_next< hdr_access >( o, *this );
                return *this;
        }
};

struct der : public ll_base< der >
{
};

void check_links( node_t& first )
{
        for ( auto* p = &first; p->hdr.next; p = p->hdr.next )
                CHECK_EQ( p, p->hdr.next->hdr.prev );
}
void check_links( der& first )
{
        for ( auto* p = &first; p->next(); p = p->next() )
                CHECK_EQ( p, p->next()->prev() );
}

void check_for_each( node_t& n, std::set< node_t const* > expected )
{
        std::set< node_t const* > s;
        for_each_node< hdr_access >( n, [&]( node_t& m ) {
                s.insert( &m );
        } );
        CHECK_EQ( s, expected );

        std::set< node_t const* > s2;
        for_each_node< hdr_access >( std::as_const( n ), [&]( node_t const& m ) {
                s2.insert( &m );
        } );
        CHECK_EQ( s2, expected );
}

void check_for_each( der& n, std::set< der const* > expected )
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

TEST_CASE_TEMPLATE( "single", T, node_t, der )
{
        T d1;
        check_links( d1 );
        check_for_each( d1, { &d1 } );
}

TEST_CASE( "dual" )
{
        node_t d1, d2;
        link_as_last< hdr_access >( d1, d2 );
        check_links( d1 );
        check_for_each( d1, { &d1, &d2 } );
}

TEST_CASE( "triple" )
{
        node_t d1, d2, d3;
        SUBCASE( "link as last" )
        {
                link_as_last< hdr_access >( d1, d2 );
                link_as_last< hdr_access >( d2, d3 );
        }
        SUBCASE( "link as next" )
        {
                link_empty_as_next< hdr_access >( d1, d2 );
                link_empty_as_next< hdr_access >( d2, d3 );
        }
        check_links( d1 );
        check_for_each( d1, { &d1, &d2, &d3 } );

        d2 = node_t{};
        check_links( d1 );
        check_for_each( d1, { &d1, &d3 } );
}

TEST_CASE( "cpy" )
{
        node_t d1;
        node_t d2{ d1 };

        SUBCASE( "two" )
        {
                check_links( d1 );
                check_for_each( d1, { &d1, &d2 } );
        }

        SUBCASE( "three" )
        {
                node_t d3{ d2 };

                check_links( d1 );
                check_for_each( d1, { &d1, &d2, &d3 } );
        }

        SUBCASE( "three assign" )
        {
                node_t d3;
                d3 = d2;

                check_links( d1 );
                check_for_each( d1, { &d1, &d2, &d3 } );
        }

        SUBCASE( "move it" )
        {
                node_t d3{ std::move( d2 ) };
                check_links( d1 );
                check_for_each( d1, { &d1, &d3 } );
        }

        SUBCASE( "move it assign" )
        {
                node_t d3;
                d3 = std::move( d2 );
                check_links( d1 );
                check_for_each( d1, { &d1, &d3 } );
        }
}

TEST_CASE( "vector" )
{
        std::vector< node_t > d1;
        for ( int i = 0; i < 42; i++ ) {
                auto& last = d1.emplace_back();
                if ( i > 0 )
                        link_as_last< hdr_access >( d1.front(), last );
                check_links( d1.front() );
        }
        std::set< node_t const* > s;
        for ( node_t& d : d1 )
                s.insert( &d );
        check_for_each( d1.front(), s );
}

}  // namespace zll