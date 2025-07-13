
#pragma once

#include <concepts>

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

        constexpr _ll_ptr* next() noexcept( noexcept_access )
        {
                if ( auto* n = hdr() )
                        return &n->next;
                if ( auto* l = list() )
                        return &l->first;
                return nullptr;
        }

        constexpr void front_try_set( _ll_ptr p ) noexcept( noexcept_access )
        {
                if ( auto* x = next() )
                        *x = p;
        }

        constexpr _ll_ptr* prev() noexcept( noexcept_access )
        {
                if ( auto* n = hdr() )
                        return &n->prev;
                if ( auto* l = list() )
                        return &l->last;
                return nullptr;
        }

        constexpr void back_try_set( _ll_ptr p ) noexcept( noexcept_access )
        {
                if ( auto* x = prev() )
                        *x = p;
        }

        friend constexpr auto
        operator<=>( _ll_ptr const& lh, _ll_ptr const& rh ) noexcept = default;
};

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
                next.back_try_set( prev );
                prev.front_try_set( next );
        }
};

template < typename T, typename Acc >
constexpr void unlink( T& node ) noexcept( _nothrow_access< Acc, T > )
{
        auto& n_hdr = Acc::get( &node );

        n_hdr.next.back_try_set( n_hdr.prev );
        n_hdr.prev.front_try_set( n_hdr.next );
}

template < typename T, typename Acc >
constexpr void move_from_to( T& from, T& to ) noexcept( _nothrow_access< Acc, T > )
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
constexpr void link_empty_as_next( T& n, T& empty ) noexcept( _nothrow_access< Acc, T > )
{
        auto& e_hdr = Acc::get( &empty );
        auto& n_hdr = Acc::get( &n );

        e_hdr.next = n_hdr.next;
        e_hdr.next.back_try_set( &empty );

        n_hdr.next = &empty;
        e_hdr.prev = &n;
}

template < typename T, typename Acc >
constexpr void link_as_last( _ll_ptr< T, Acc > p, T& rh ) noexcept( _nothrow_access< Acc, T > )
{
        while ( auto&& m = p.next()->node() )
                p = m;

        *p.next()            = &rh;
        Acc::get( &rh ).prev = p;
}

template < typename T, typename Acc >
struct ll_list
{
        static constexpr bool noexcept_access = _nothrow_access< Acc, T >;

        _ll_ptr< T, Acc > first = *this;
        _ll_ptr< T, Acc > last  = *this;

        ll_list() = default;

        ll_list( ll_list const& )                = delete;
        ll_list( ll_list&& ) noexcept            = delete;
        ll_list& operator=( ll_list const& )     = delete;
        ll_list& operator=( ll_list&& ) noexcept = delete;

        constexpr void link_as_last( T& node ) noexcept( noexcept_access )
        {
                link_as_last< T, Acc >( *this, node );
        }

        ~ll_list() noexcept( noexcept_access )
        {
                if ( first )
                        first.back_try_set( nullptr );
                if ( last )
                        last.front_try_set( nullptr );
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

        ll_base() noexcept = default;

        ll_base( ll_base&& o ) noexcept
        {
                move_from_to< Derived, access >( o.derived(), derived() );
        }

        ll_base( ll_base& o ) noexcept
        {
                link_empty_as_next< Derived, access >( o.derived(), derived() );
        }

        ll_base& operator=( ll_base&& o ) noexcept
        {
                if ( this == &o )
                        return *this;
                unlink< Derived, access >( derived() );
                move_from_to< Derived, access >( o.derived(), derived() );
                return *this;
        }

        ll_base& operator=( ll_base& o ) noexcept
        {
                if ( this == &o )
                        return *this;
                unlink< Derived, access >( derived() );
                link_empty_as_next< Derived, access >( o.derived(), derived() );
                return *this;
        }

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