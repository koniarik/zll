
#pragma once

#include <cassert>
#include <concepts>

namespace zll
{

template < typename T, typename Acc >
struct ll_header
{
        T* next = nullptr;
        T* prev = nullptr;

        constexpr ll_header() noexcept                     = default;
        ll_header( ll_header&& other ) noexcept            = delete;
        ll_header( ll_header const& other )                = delete;
        ll_header& operator=( ll_header&& other ) noexcept = delete;
        ll_header& operator=( ll_header const& other )     = delete;

        constexpr ~ll_header() noexcept
        {
                if ( next )
                        Acc::get( next ).prev = prev;
                if ( prev )
                        Acc::get( prev ).next = next;
        }
};

template < typename Acc, typename T >
concept nothrow_access = noexcept( Acc::get( (T*) nullptr ) );

template < typename Acc, typename T >
constexpr void unlink( T& node ) noexcept( nothrow_access< Acc, T > )
{
        auto& n_hdr = Acc::get( &node );
        if ( n_hdr.next )
                Acc::get( n_hdr.next ).prev = n_hdr.prev;
        if ( n_hdr.prev )
                Acc::get( n_hdr.prev ).next = n_hdr.next;
}

template < typename Acc, typename T >
constexpr void move_from_to( T& from, T& to ) noexcept( nothrow_access< Acc, T > )
{
        auto& from_hdr = Acc::get( &from );
        auto& to_hdr   = Acc::get( &to );

        to_hdr.next = from_hdr.next;
        to_hdr.prev = from_hdr.prev;

        if ( to_hdr.next )
                Acc::get( to_hdr.next ).prev = &to;

        if ( to_hdr.prev )
                Acc::get( to_hdr.prev ).next = &to;

        from_hdr.next = nullptr;
        from_hdr.prev = nullptr;
}

template < typename Acc, typename T >
constexpr void link_empty_as_next( T& n, T& empty ) noexcept( nothrow_access< Acc, T > )
{
        auto& e_hdr = Acc::get( &empty );
        auto& n_hdr = Acc::get( &n );

        e_hdr.next = n_hdr.next;
        if ( e_hdr.next )
                Acc::get( e_hdr.next ).prev = &empty;

        n_hdr.next = &empty;
        e_hdr.prev = &n;
}

template < typename Acc, typename T >
constexpr void link_as_last( T& lh, T& rh ) noexcept( nothrow_access< Acc, T > )
{
        assert( &lh != &rh );

        T* p = &lh;
        while ( auto* n = Acc::get( p ).next )
                p = n;

        Acc::get( p ).next   = &rh;
        Acc::get( &rh ).prev = p;
}

template < typename Acc, typename T >
constexpr void
for_each_node( T& n, auto&& f ) noexcept( nothrow_access< Acc, T >&& noexcept( f( n ) ) )
{
        for ( T* p = Acc::get( &n ).prev; p; p = Acc::get( p ).prev )
                f( *p );
        f( n );
        for ( T* p = Acc::get( &n ).next; p; p = Acc::get( p ).next )
                f( *p );
}

}  // namespace zll