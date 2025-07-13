
#pragma once

#include <cassert>
#include <concepts>

namespace zll
{

template < typename Acc, typename T >
concept nothrow_access = noexcept( Acc::get( (T*) nullptr ) );

template < typename T, typename Acc >
struct ll_list;

template < typename T, typename Acc >
struct ll_header;

template < typename T, typename Acc >
struct ll_ptr
{
        static constexpr std::intptr_t mask            = 1;
        static constexpr bool          noexcept_access = nothrow_access< Acc, T >;

        std::intptr_t ptr = 0;

        constexpr ll_ptr( std::nullptr_t ) noexcept
        {
                ptr = reinterpret_cast< std::intptr_t >( nullptr );
        };

        constexpr ll_ptr( T* n ) noexcept
        {
                ptr = reinterpret_cast< std::intptr_t >( n );
        };

        constexpr ll_ptr( ll_list< T, Acc >& n ) noexcept
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

        constexpr ll_header< T, Acc >* hdr() noexcept( noexcept_access )
        {
                if ( auto* n = node() )
                        return &Acc::get( n );
                return nullptr;
        }

        constexpr ll_list< T, Acc >* list() noexcept
        {
                static_assert( alignof( T ) > 2 );
                return ptr && !is_node() ? reinterpret_cast< ll_list< T, Acc >* >( ptr & ~mask ) :
                                           nullptr;
        }

        constexpr ll_ptr* next() noexcept( noexcept_access )
        {
                if ( auto* n = hdr() )
                        return &n->next;
                if ( auto* l = list() )
                        return &l->first;
                return nullptr;
        }

        constexpr void front_try_set( ll_ptr p ) noexcept( noexcept_access )
        {
                if ( auto* x = next() )
                        *x = p;
        }

        constexpr ll_ptr* prev() noexcept( noexcept_access )
        {
                if ( auto* n = hdr() )
                        return &n->prev;
                if ( auto* l = list() )
                        return &l->last;
                return nullptr;
        }

        constexpr void back_try_set( ll_ptr p ) noexcept( noexcept_access )
        {
                if ( auto* x = prev() )
                        *x = p;
        }

        friend constexpr auto operator<=>( ll_ptr const& lh, ll_ptr const& rh ) noexcept = default;
};

template < typename T, typename Acc >
struct ll_header
{
        ll_ptr< T, Acc > next = nullptr;
        ll_ptr< T, Acc > prev = nullptr;

        constexpr ll_header() noexcept                     = default;
        ll_header( ll_header&& other ) noexcept            = delete;
        ll_header( ll_header const& other )                = delete;
        ll_header& operator=( ll_header&& other ) noexcept = delete;
        ll_header& operator=( ll_header const& other )     = delete;

        constexpr ~ll_header() noexcept( nothrow_access< Acc, T > )
        {
                if ( ll_ptr< T, Acc >* b = next.prev() )
                        *b = prev;

                if ( ll_ptr< T, Acc >* f = next.next() )
                        *f = next;
        }
};

template < typename T, typename Acc >
constexpr void unlink( T& node ) noexcept( nothrow_access< Acc, T > )
{
        auto& n_hdr = Acc::get( &node );

        n_hdr.next.back_try_set( n_hdr.prev );
        n_hdr.prev.front_try_set( n_hdr.next );
}

template < typename T, typename Acc >
constexpr void move_from_to( T& from, T& to ) noexcept( nothrow_access< Acc, T > )
{
        auto& from_hdr = Acc::get( &from );
        auto& to_hdr   = Acc::get( &to );

        to_hdr.next = from_hdr.next;
        to_hdr.prev = from_hdr.prev;

        to_hdr.next.back_try_set( &to );
        to_hdr.prev.front_try_set( &to );

        from_hdr.next = nullptr;
        from_hdr.prev = nullptr;
}

template < typename T, typename Acc >
constexpr void link_empty_as_next( T& n, T& empty ) noexcept( nothrow_access< Acc, T > )
{
        auto& e_hdr = Acc::get( &empty );
        auto& n_hdr = Acc::get( &n );

        e_hdr.next = n_hdr.next;
        e_hdr.next.back_try_set( &empty );

        n_hdr.next = &empty;
        e_hdr.prev = &n;
}

template < typename T, typename Acc >
constexpr void link_as_last( ll_ptr< T, Acc > p, T& rh ) noexcept( nothrow_access< Acc, T > )
{
        assert( p.node() != &rh );

        while ( auto&& m = p.next()->node() )
                p = m;

        *p.next()            = &rh;
        Acc::get( &rh ).prev = p;
}

template < typename T, typename Acc >
struct ll_list
{
        static constexpr bool noexcept_access = nothrow_access< Acc, T >;

        ll_ptr< T, Acc > first = *this;
        ll_ptr< T, Acc > last  = *this;

        ll_list() = default;

        ll_list( ll_list const& )                = delete;
        ll_list( ll_list&& ) noexcept            = delete;
        ll_list& operator=( ll_list const& )     = delete;
        ll_list& operator=( ll_list&& ) noexcept = delete;

        constexpr void link_as_last( T& node ) noexcept( noexcept_access )
        {
                link_as_last< T, Acc >( *this, node );
        }
};

template < typename Derived >
struct ll_base
{
        struct access
        {
                static auto& get( Derived* d ) noexcept
                {
                        return static_cast< ll_base* >( d )->hdr;
                }
        };

        Derived* next()
        {
                return hdr.next.node();
        }

        Derived const* next() const
        {
                return hdr.next.node();
        }

        Derived* prev()
        {
                return hdr.prev.node();
        }

        Derived const* prev() const
        {
                return hdr.prev.node();
        }

protected:
        Derived& derived()
        {
                return *static_cast< Derived* >( this );
        }

        Derived const& derived() const
        {
                return *static_cast< Derived const* >( this );
        }

private:
        ll_header< Derived, access > hdr;
};

template < typename Acc, typename T >
constexpr void for_each_node( T& n, std::invocable< T& > auto&& f ) noexcept(
    nothrow_access< Acc, T > && noexcept( f( n ) ) )
{
        auto& h = Acc::get( &n );
        for ( T* m = h.prev.node(); m; m = Acc::get( m ).prev.node() )
                f( *m );
        f( n );
        for ( T* m = h.next.node(); m; m = Acc::get( m ).next.node() )
                f( *m );
}

namespace bits
{

constexpr void for_each_node_base_impl( auto& n, auto&& f ) noexcept( noexcept( f( n ) ) )
{
        for ( auto* p = n.prev(); p; p = p->prev() )
                f( *p );
        f( n );
        for ( auto* p = n.next(); p; p = p->next() )
                f( *p );
}

}  // namespace bits

template < typename D, std::invocable< D& > F >
requires( std::derived_from< D, ll_base< D > > )
constexpr void for_each_node( D& n, F&& f ) noexcept( noexcept( f( n ) ) )
{
        bits::for_each_node_base_impl( n, (F&&) f );
}

template < typename D, std::invocable< D const& > F >
requires( std::derived_from< D, ll_base< D > > )
constexpr void for_each_node( D const& n, F&& f ) noexcept( noexcept( f( n ) ) )
{
        bits::for_each_node_base_impl( n, (F&&) f );
}

}  // namespace zll