
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

struct der
{
        ll_header< der, hdr_access > hdr;

        der() noexcept = default;
        der( der&& o ) noexcept
        {
                move_from_to< hdr_access >( o, *this );
        }
        der( der& o ) noexcept
        {
                link_empty_as_next< hdr_access >( o, *this );
        }
        der& operator=( der&& o ) noexcept
        {
                unlink< hdr_access >( *this );
                move_from_to< hdr_access >( o, *this );
                return *this;
        }
        der& operator=( der& o ) noexcept
        {
                unlink< hdr_access >( *this );
                link_empty_as_next< hdr_access >( o, *this );
                return *this;
        }
};

void check_links( der& first )
{
        for ( der* p = &first; p->hdr.next; p = p->hdr.next )
                CHECK_EQ( p, p->hdr.next->hdr.prev );
}

void check_for_each( der& n, std::set< der const* > expected )
{
        std::set< der const* > s;
        for_each_node< hdr_access >( n, [&]( der& m ) {
                s.insert( &m );
        } );
        CHECK_EQ( s, expected );

        std::set< der const* > s2;
        for_each_node< hdr_access >( std::as_const( n ), [&]( der const& m ) {
                s2.insert( &m );
        } );
        CHECK_EQ( s2, expected );
}

TEST_CASE( "single" )
{
        der d1;
        check_links( d1 );
        check_for_each( d1, { &d1 } );
}

TEST_CASE( "dual" )
{
        der d1, d2;
        link_as_last< hdr_access >( d1, d2 );
        check_links( d1 );
        check_for_each( d1, { &d1, &d2 } );
}

TEST_CASE( "triple" )
{
        der d1, d2, d3;
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

        d2 = der{};
        check_links( d1 );
        check_for_each( d1, { &d1, &d3 } );
}

TEST_CASE( "cpy" )
{
        der d1;
        der d2{ d1 };

        SUBCASE( "two" )
        {
                check_links( d1 );
                check_for_each( d1, { &d1, &d2 } );
        }

        SUBCASE( "three" )
        {
                der d3{ d2 };

                check_links( d1 );
                check_for_each( d1, { &d1, &d2, &d3 } );
        }

        SUBCASE( "three assign" )
        {
                der d3;
                d3 = d2;

                check_links( d1 );
                check_for_each( d1, { &d1, &d2, &d3 } );
        }

        SUBCASE( "move it" )
        {
                der d3{ std::move( d2 ) };
                check_links( d1 );
                check_for_each( d1, { &d1, &d3 } );
        }

        SUBCASE( "move it assign" )
        {
                der d3;
                d3 = std::move( d2 );
                check_links( d1 );
                check_for_each( d1, { &d1, &d3 } );
        }
}

TEST_CASE( "vector" )
{
        std::vector< der > d1;
        for ( int i = 0; i < 42; i++ ) {
                auto& last = d1.emplace_back();
                if ( i > 0 )
                        link_as_last< hdr_access >( d1.front(), last );
                check_links( d1.front() );
        }
        std::set< der const* > s;
        for ( der& d : d1 )
                s.insert( &d );
        check_for_each( d1.front(), s );
}

}  // namespace zll