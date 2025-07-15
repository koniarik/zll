/// MIT License
///
/// Copyright (c) 2024 koniarik
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

#pragma once

#include <concepts>
#include <cstdint>

namespace zll
{

template < typename Acc, typename T >
concept _nothrow_access = noexcept( Acc::get( (T*) nullptr ) );

template < typename T, typename Acc >
struct ll_list;

template < typename T, typename Acc >
struct ll_header;

template < typename T, typename Acc >
struct _ll_ptr
{
        static constexpr std::intptr_t mask            = 1;
        static constexpr bool          noexcept_access = _nothrow_access< Acc, T >;

        std::intptr_t ptr = 0;

        constexpr _ll_ptr( std::nullptr_t ) noexcept
        {
                ptr = reinterpret_cast< std::intptr_t >( nullptr );
        };

        constexpr _ll_ptr( T* n ) noexcept
        {
                ptr = reinterpret_cast< std::intptr_t >( n );
        };

        constexpr _ll_ptr( ll_list< T, Acc >& n ) noexcept
        {
                ptr = reinterpret_cast< std::intptr_t >( &n ) | mask;
        };

        constexpr operator bool() noexcept
        {
                return !!ptr;
        }

        constexpr bool is_node() const noexcept
        {
                return !( ptr & mask );
        }

        constexpr T* node() const noexcept
        {
                static_assert( alignof( T ) > 2 );
                return ptr && is_node() ? reinterpret_cast< T* >( ptr ) : nullptr;
        }

        constexpr T* node() noexcept
        {
                static_assert( alignof( T ) > 2 );
                return ptr && is_node() ? reinterpret_cast< T* >( ptr ) : nullptr;
        }

        constexpr ll_list< T, Acc >* list() noexcept
        {
                static_assert( alignof( T ) > 2 );
                return ptr && !is_node() ? reinterpret_cast< ll_list< T, Acc >* >( ptr & ~mask ) :
                                           nullptr;
        }

        friend constexpr auto
        operator<=>( _ll_ptr const& lh, _ll_ptr const& rh ) noexcept = default;
};

template < typename T, typename Acc >
constexpr void _prev_or_last_set( _ll_ptr< T, Acc > p, _ll_ptr< T, Acc > n )
{
        if ( T* x = p.node() )
                Acc::get( x ).prev = n;
        else if ( auto* h = p.list() )
                h->last = n.node();
}

template < typename T, typename Acc >
constexpr void _next_or_first_set( _ll_ptr< T, Acc > p, _ll_ptr< T, Acc > n )
{
        if ( T* x = p.node() )
                Acc::get( x ).next = n;
        else if ( auto* h = p.list() )
                h->first = n.node();
}

template < typename T, typename Acc >
struct ll_header
{
        _ll_ptr< T, Acc > next = nullptr;
        _ll_ptr< T, Acc > prev = nullptr;

        constexpr ll_header() noexcept                     = default;
        ll_header( ll_header&& other ) noexcept            = delete;
        ll_header( ll_header const& other )                = delete;
        ll_header& operator=( ll_header&& other ) noexcept = delete;
        ll_header& operator=( ll_header const& other )     = delete;

        constexpr ~ll_header() noexcept( _nothrow_access< Acc, T > )
        {
                _prev_or_last_set( next, prev );
                _next_or_first_set( prev, next );
        }
};

template < typename T, typename Acc >
constexpr void unlink( T& node ) noexcept( _nothrow_access< Acc, T > )
{
        auto& n_hdr = Acc::get( &node );

        _prev_or_last_set( n_hdr.next, n_hdr.prev );
        _next_or_first_set( n_hdr.prev, n_hdr.next );

        n_hdr.next = nullptr;
        n_hdr.prev = nullptr;
}

template < typename T, typename Acc >
constexpr void move_from_to( T& from, T& to ) noexcept( _nothrow_access< Acc, T > )
{
        auto& from_hdr = Acc::get( &from );
        auto& to_hdr   = Acc::get( &to );

        to_hdr.next = from_hdr.next;
        to_hdr.prev = from_hdr.prev;

        _prev_or_last_set< T, Acc >( to_hdr.next, &to );
        _next_or_first_set< T, Acc >( to_hdr.prev, &to );

        from_hdr.next = nullptr;
        from_hdr.prev = nullptr;
}

template < typename T, typename Acc >
constexpr void link_empty_as_next( T& n, T& empty ) noexcept( _nothrow_access< Acc, T > )
{
        auto& e_hdr = Acc::get( &empty );
        auto& n_hdr = Acc::get( &n );

        e_hdr.next = n_hdr.next;
        _prev_or_last_set< T, Acc >( e_hdr.next, &empty );

        n_hdr.next = &empty;
        e_hdr.prev = &n;
}

template < typename T, typename Acc >
constexpr void link_empty_as_prev( T& n, T& empty ) noexcept( _nothrow_access< Acc, T > )
{
        auto& e_hdr = Acc::get( &empty );
        auto& n_hdr = Acc::get( &n );

        e_hdr.prev = n_hdr.prev;
        _next_or_first_set< T, Acc >( e_hdr.prev, &empty );

        n_hdr.prev = &empty;
        e_hdr.next = &n;
}

template < typename T, typename Acc >
constexpr void link_empty_as_last( T& n, T& rh ) noexcept( _nothrow_access< Acc, T > )
{
        auto* p = &n;
        while ( p ) {
                auto* pp = Acc::get( p ).next.node();
                if ( !pp )
                        break;
                p = pp;
        }

        link_empty_as_next< T, Acc >( *p, rh );
}

template < typename T, typename Acc >
constexpr void link_empty_as_first( T& n, T& rh ) noexcept( _nothrow_access< Acc, T > )
{
        auto* p = &n;
        while ( p ) {
                auto* pp = Acc::get( p ).prev.node();
                if ( !pp )
                        break;
                p = pp;
        }

        link_empty_as_prev< T, Acc >( *p, rh );
}

template < typename T, typename Acc = typename T::access >
struct ll_list
{
        static constexpr bool noexcept_access = _nothrow_access< Acc, T >;

        T* first = nullptr;
        T* last  = nullptr;

        constexpr ll_list() = default;

        ll_list( ll_list const& )                = delete;
        ll_list( ll_list&& ) noexcept            = delete;
        ll_list& operator=( ll_list const& )     = delete;
        ll_list& operator=( ll_list&& ) noexcept = delete;

        constexpr void link_front( T& node ) noexcept( noexcept_access )
        {
                unlink< T, Acc >( node );
                if ( first )
                        link_empty_as_prev< T, Acc >( *last, node );
                else
                        link_first( node );
        }

        constexpr void unlink_front() noexcept( noexcept_access )
        {
                unlink< T, Acc >( *last );
        }

        constexpr bool empty() const noexcept
        {
                return !first;
        }

        constexpr void link_back( T& node ) noexcept( noexcept_access )
        {
                unlink< T, Acc >( node );
                if ( last )
                        link_empty_as_next< T, Acc >( *last, node );
                else
                        link_first( node );
        }

        constexpr void unlink_back() noexcept( noexcept_access )
        {
                unlink< T, Acc >( *last );
        }

        constexpr ~ll_list() noexcept( noexcept_access )
        {
                if ( first )
                        Acc::get( first ).prev = nullptr;
                if ( last )
                        Acc::get( last ).next = nullptr;
        }

private:
        constexpr void link_first( T& node ) noexcept( noexcept_access )
        {
                first = &node;
                last  = &node;

                Acc::get( &node ).next = *this;
                Acc::get( &node ).prev = *this;
        }
};

template < typename Derived >
struct ll_base
{
        struct access
        {
                constexpr static auto& get( Derived* d ) noexcept
                {
                        return static_cast< ll_base* >( d )->hdr;
                }
        };

        constexpr ll_base() noexcept = default;

        constexpr ll_base( ll_base&& o ) noexcept
        {
                move_from_to< Derived, access >( o.derived(), derived() );
        }

        constexpr ll_base( ll_base& o ) noexcept
        {
                link_empty_as_next< Derived, access >( o.derived(), derived() );
        }

        constexpr ll_base& operator=( ll_base&& o ) noexcept
        {
                if ( this == &o )
                        return *this;
                unlink< Derived, access >( derived() );
                move_from_to< Derived, access >( o.derived(), derived() );
                return *this;
        }

        constexpr ll_base& operator=( ll_base& o ) noexcept
        {
                if ( this == &o )
                        return *this;
                unlink< Derived, access >( derived() );
                link_empty_as_next< Derived, access >( o.derived(), derived() );
                return *this;
        }

        constexpr void link_next( Derived& empty ) noexcept
        {
                unlink< Derived, access >( empty );
                link_empty_as_next< Derived, access >( derived(), empty );
        }

        constexpr void link_prev( Derived& empty ) noexcept
        {
                unlink< Derived, access >( empty );
                link_empty_as_prev< Derived, access >( derived(), empty );
        }

        constexpr Derived* next()
        {
                return hdr.next.node();
        }

        constexpr Derived const* next() const
        {
                return hdr.next.node();
        }

        constexpr Derived* prev()
        {
                return hdr.prev.node();
        }

        constexpr Derived const* prev() const
        {
                return hdr.prev.node();
        }

protected:
        constexpr Derived& derived()
        {
                return *static_cast< Derived* >( this );
        }

        constexpr Derived const& derived() const
        {
                return *static_cast< Derived const* >( this );
        }

private:
        ll_header< Derived, access > hdr;
};

template < typename Acc, typename T >
constexpr void for_each_node( T& n, std::invocable< T& > auto&& f ) noexcept(
    _nothrow_access< Acc, T > && noexcept( f( n ) ) )
{
        auto& h = Acc::get( &n );
        for ( T* m = h.prev.node(); m; m = Acc::get( m ).prev.node() )
                f( *m );
        f( n );
        for ( T* m = h.next.node(); m; m = Acc::get( m ).next.node() )
                f( *m );
}

constexpr void _for_each_node_base_impl( auto& n, auto&& f ) noexcept( noexcept( f( n ) ) )
{
        for ( auto* p = n.prev(); p; p = p->prev() )
                f( *p );
        f( n );
        for ( auto* p = n.next(); p; p = p->next() )
                f( *p );
}

template < typename D, std::invocable< D& > F >
requires( std::derived_from< D, ll_base< D > > )
constexpr void for_each_node( D& n, F&& f ) noexcept( noexcept( f( n ) ) )
{
        _for_each_node_base_impl( n, (F&&) f );
}

template < typename D, std::invocable< D const& > F >
requires( std::derived_from< D, ll_base< D > > )
constexpr void for_each_node( D const& n, F&& f ) noexcept( noexcept( f( n ) ) )
{
        _for_each_node_base_impl( n, (F&&) f );
}

}  // namespace zll