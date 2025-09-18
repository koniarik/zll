

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
        sh_header< node_t, hdr_access > hdr;

        using access = hdr_access;

        int x;

        node_t( int v = 0 )
          : x( v )
        {
        }

        node_t( node_t&& o ) noexcept
        {
                move_from_to< node_t, hdr_access >( o, *this );
        }

        node_t( node_t& o ) noexcept
        {
                link_detached_copy_of< node_t, hdr_access >( o, *this );
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
                link_detached_copy_of< node_t, hdr_access >( o, *this );
                return *this;
        }

        bool operator<( node_t const& other ) const noexcept
        {
                return this < &other;
        }

        ~node_t()
        {
                detach< node_t, hdr_access >( *this );
        }
};

struct der : public sh_base< der >
{
        int x;

        der( int v = 0 )
          : x( v )
        {
        }

        bool operator<( der const& other ) const noexcept
        {
                return this < &other;
        }
};

template < typename T, typename Acc = typename T::access >
void check_links( T& node )
{

        auto& h = Acc::get( node );

        if ( h.left ) {
                CHECK( Acc::get( *h.left ).parent == node );
                check_links( *h.left );
        }
        if ( h.right ) {
                CHECK( Acc::get( *h.right ).parent == node );
                check_links( *h.right );
        }
}

std::ostream& operator<<( std::ostream& os, std::set< node_t const* > const& value )
{
        os << "{ ";
        for ( auto* v : value )
                os << v << "(" << v->hdr.left << ", " << v->hdr.right << ") ";
        os << "}";
        return os;
}

std::ostream& operator<<( std::ostream& os, std::set< der const* > const& value )
{
        os << "{ ";
        for ( auto* v : value )
                os << v << "(" << der::access::get( *v ).left << ", "
                   << der::access::get( *v ).right << ") ";
        os << "}";
        return os;
}

template < typename T, typename Acc = typename T::access >
void check_for_each_node( T& node, std::set< T const* > expected )
{
        std::set< T const* > s;
        for_each_node( node, [&]( T& m ) {
                s.insert( &m );
        } );
        CHECK_EQ( s, expected );
}

TEST_CASE_TEMPLATE( "single", T, node_t, der )
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

                CHECK_EQ( &l.top(), &d1 );
        }

        check_links( d1 );
        check_for_each_node( d1, { &d1 } );
}

TEST_CASE_TEMPLATE( "dual", T, node_t, der )
{
        using access = typename T::access;

        T d1{ 1 }, d2{ 2 };

        SUBCASE( "link as last" )
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
                CHECK_EQ( &h.top(), &d3 );
                check_for_each_node( d3, { &d2, &d3 } );
        }
        check_links( d1 );
}

}  // namespace
}  // namespace zll