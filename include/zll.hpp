/// MIT License
///
/// Copyright (c) 2024-2026 koniarik
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

#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <type_traits>
#include <utility>

#ifdef ZLL_DEFAULT_ASSERT

#include <cassert>
#define ZLL_ASSERT( expr ) assert( expr )

#else

#ifndef ZLL_ASSERT
#define ZLL_ASSERT( expr ) ( (void) ( ( expr ) ) )
#endif

#endif

namespace zll
{

template < typename T, typename Acc = typename T::access >
struct ll_list;

template < typename T, typename Acc = typename T::access >
struct ll_header;

template < typename Acc, typename T >
concept _nothrow_access = noexcept( Acc::get( std::declval< T& >() ) );

template < typename Acc, typename T, typename Compare >
concept _nothrow_access_compare =
    _nothrow_access< Acc, T > &&
    noexcept( std::declval< Compare& >()( std::declval< T& >(), std::declval< T& >() ) );

template < typename T, typename Acc >
using _hdr_of = std::remove_cvref_t< decltype( Acc::get( std::declval< T& >() ) ) >;

template < typename T, typename Acc >
concept _provides_ll_header = requires( T& t, ll_header< std::remove_const_t< T >, Acc >& h ) {
        {
                Acc::get( t )
        } -> std::convertible_to< ll_header< std::remove_const_t< T >, Acc > const& >;
        { Acc::node( h ) } noexcept -> std::same_as< std::remove_const_t< T >& >;
};

template < typename A, typename B >
struct _vptr
{
        static constexpr std::intptr_t mask = 1;

        std::intptr_t ptr = 0;

        _vptr( std::nullptr_t ) noexcept
        {
                ptr = std::bit_cast< std::intptr_t >( nullptr );
        };

        _vptr( A& n ) noexcept
        {
                ptr = std::bit_cast< std::intptr_t >( &n );
        };

        _vptr( B& n ) noexcept
        {
                ptr = std::bit_cast< std::intptr_t >( &n ) | mask;
        };

        operator bool() noexcept
        {
                return !!ptr;
        }

        bool is_a() const noexcept
        {
                return !( ptr & mask );
        }

        A* a() const noexcept
        {
                static_assert( alignof( A ) > 2 );
                return is_a() ? std::bit_cast< A* >( ptr ) : nullptr;
        }

        B* b() const noexcept
        {
                static_assert( alignof( B ) > 2 );
                return !is_a() ? std::bit_cast< B* >( ptr & ~mask ) : nullptr;
        }

        friend auto operator<=>( _vptr const& lh, _vptr const& rh ) noexcept = default;
};

struct _ll_hdr;
struct _raw_list;

using _ll_word = _vptr< _ll_hdr, _raw_list >;

struct _ll_hdr
{
        _ll_word _next = nullptr;
        _ll_word _prev = nullptr;

        _ll_hdr() noexcept                             = default;
        _ll_hdr( _ll_hdr&& other ) noexcept            = delete;
        _ll_hdr( _ll_hdr const& other )                = delete;
        _ll_hdr& operator=( _ll_hdr&& other ) noexcept = delete;
        _ll_hdr& operator=( _ll_hdr const& other )     = delete;

        ~_ll_hdr() noexcept;
};

struct _raw_list
{
        _ll_hdr* first = nullptr;
        _ll_hdr* last  = nullptr;

        /// Detaches all nodes; they stay linked together, but not to this list.
        void detach_nodes() noexcept
        {
                if ( first ) {
                        first->_prev = nullptr;
                        last->_next  = nullptr;
                }
                first = nullptr;
                last  = nullptr;
        }

        /// Takes over the nodes of `other`, which ends up empty.
        void take( _raw_list& other ) noexcept
        {
                detach_nodes();
                first       = other.first;
                last        = other.last;
                other.first = nullptr;
                other.last  = nullptr;
                if ( first ) {
                        first->_prev = *this;
                        last->_next  = *this;
                }
        }
};

inline void _prev_or_last_set( _ll_word p, _ll_word n ) noexcept
{
        if ( _ll_hdr* x = p.a() )
                x->_prev = n;
        else if ( p )
                p.b()->last = n.a();
}

inline void _next_or_first_set( _ll_word p, _ll_word n ) noexcept
{
        if ( _ll_hdr* x = p.a() )
                x->_next = n;
        else if ( p )
                p.b()->first = n.a();
}

/// Unlinks range [first, last], linking its predecessor and successor together.
inline void _unlink( _ll_hdr& first, _ll_hdr& last ) noexcept
{
        _prev_or_last_set( last._next, first._prev );
        _next_or_first_set( first._prev, last._next );
        first._prev = nullptr;
        last._next  = nullptr;
}

/// Links detached range [first, last] between `prev` and `next`.
inline void _link( _ll_hdr& first, _ll_hdr& last, _ll_word prev, _ll_word next ) noexcept
{
        first._prev = prev;
        last._next  = next;
        _prev_or_last_set( next, last );
        _next_or_first_set( prev, first );
}

/// Moves the links of `from` to the detached `to`; `from` ends up detached.
inline void _ll_move( _ll_hdr& from, _ll_hdr& to ) noexcept
{
        _link( to, to, from._prev, from._next );
        from._next = nullptr;
        from._prev = nullptr;
}

inline _ll_hdr::~_ll_hdr() noexcept
{
        _unlink( *this, *this );
}

/// Linked-list header containing links to the next and previous headers or the list itself.
/// Will detach itself from the linked list on destruction.
///
/// Type `T` is the type of the node that contains this header.
/// Type `Acc` maps a node to its header (`get`) and a header back to its node (`node`).
template < typename T, typename Acc >
struct ll_header : _ll_hdr
{
};

/// Node that owns header `h`.
template < typename T, typename Acc >
T& _node( _ll_hdr& h ) noexcept
{
        return Acc::node( static_cast< _hdr_of< T, Acc >& >( h ) );
}

/// Node that owns header `h`. `Acc::node` only maps the address, nothing is written through it.
template < typename T, typename Acc >
T const& _node( _ll_hdr const& h ) noexcept
{
        return _node< T, Acc >( const_cast< _ll_hdr& >( h ) );
}

/// Node that owns header `h`, or null if there is none.
template < typename T, typename Acc >
T* _node( _ll_hdr* h ) noexcept
{
        return h ? &_node< T, Acc >( *h ) : nullptr;
}

template < typename T, typename Acc >
T* _node( _ll_word p ) noexcept
{
        return _node< T, Acc >( p.a() );
}

/// Unlink a node from the list. Previous or following node are linked together instead.
/// Node itself does not keep any connections.
template < typename T, typename Acc = typename T::access >
requires( _provides_ll_header< T, Acc > )
void detach( T& node ) noexcept( _nothrow_access< Acc, T > )
{
        _ll_hdr& n_hdr = Acc::get( node );
        _unlink( n_hdr, n_hdr );
}

/// Returns true if the node is detached from list.
template < typename T, typename Acc = typename T::access >
requires( _provides_ll_header< T, Acc > )
bool detached( T& node ) noexcept( _nothrow_access< Acc, T > )
{
        auto& n_hdr = Acc::get( node );
        return !n_hdr._next && !n_hdr._prev;
}

/// Detaches subrange [first, last] from the list. The range is not linked to any other node after
/// detachment. Successor of `last` and predecessor of `first` are linked together.
template < typename T, typename Acc = typename T::access >
requires( _provides_ll_header< T, Acc > )
void detach_range( T& first, T& last ) noexcept( _nothrow_access< Acc, T > )
{
        _unlink( Acc::get( first ), Acc::get( last ) );
}

/// Returns true if the range is detached from list.
template < typename T, typename Acc = typename T::access >
requires( _provides_ll_header< T, Acc > )
bool detached_range( T& first, T& last ) noexcept( _nothrow_access< Acc, T > )
{
        return !Acc::get( first )._prev && !Acc::get( last )._next;
}

/// Links predecessor and successor of `from` node as predecessor and successor of `to` node.
/// The `to` node must be detached.
template < typename T, typename Acc = typename T::access >
requires( _provides_ll_header< T, Acc > )
void move_from_to( T& from, T& to ) noexcept( _nothrow_access< Acc, T > )
{
        ZLL_ASSERT( ( detached< T, Acc >( to ) ) );
        _ll_move( Acc::get( from ), Acc::get( to ) );
}

/// Link detached node `d` after node `n`, any successor of `n` will be successor of `d`.
/// Undefined behavior if `d` is not detached.
template < typename T, typename Acc = typename T::access >
requires( _provides_ll_header< T, Acc > )
void link_detached_as_next( T& n, T& d ) noexcept( _nothrow_access< Acc, T > )
{
        ZLL_ASSERT( ( detached< T, Acc >( d ) ) );
        _ll_hdr& d_hdr = Acc::get( d );
        _ll_hdr& n_hdr = Acc::get( n );
        _link( d_hdr, d_hdr, n_hdr, n_hdr._next );
}

/// Link detached node `d` before node `n`, any predecessor of `n` will be predecessor of `d`.
/// Undefined behavior if `d` is not detached.
template < typename T, typename Acc = typename T::access >
requires( _provides_ll_header< T, Acc > )
void link_detached_as_prev( T& n, T& d ) noexcept( _nothrow_access< Acc, T > )
{
        ZLL_ASSERT( ( detached< T, Acc >( d ) ) );
        _ll_hdr& d_hdr = Acc::get( d );
        _ll_hdr& n_hdr = Acc::get( n );
        _link( d_hdr, d_hdr, n_hdr._prev, n_hdr );
}

/// Iterate over predecessors of node `n` and return the first node in the list.
template < typename T, typename Acc = typename T::access >
requires( _provides_ll_header< T, Acc > )
T& first_node_of( T& n ) noexcept( _nothrow_access< Acc, T > )
{
        _ll_hdr* h = Acc::get( n )._prev.a();
        if ( !h )
                return n;
        while ( _ll_hdr* p = h->_prev.a() )
                h = p;
        return _node< T, Acc >( *h );
}

/// Iterate over successors of node `n` and return the last node in the list.
template < typename T, typename Acc = typename T::access >
requires( _provides_ll_header< T, Acc > )
T& last_node_of( T& n ) noexcept( _nothrow_access< Acc, T > )
{
        _ll_hdr* h = Acc::get( n )._next.a();
        if ( !h )
                return n;
        while ( _ll_hdr* p = h->_next.a() )
                h = p;
        return _node< T, Acc >( *h );
}

/// Link detached node `d` as last element of the list accessed by node `n`.
/// Undefined behavior if `d` is not detached.
template < typename T, typename Acc = typename T::access >
requires( _provides_ll_header< T, Acc > )
void link_detached_as_last( T& n, T& d ) noexcept( _nothrow_access< Acc, T > )
{
        ZLL_ASSERT( ( detached< T, Acc >( d ) ) );
        T& last = last_node_of< T, Acc >( n );
        link_detached_as_next< T, Acc >( last, d );
}

/// Link detached node `d` as first element of the list accessed by node `n`.
/// Undefined behavior if `d` is not detached.
template < typename T, typename Acc = typename T::access >
requires( _provides_ll_header< T, Acc > )
void link_detached_as_first( T& n, T& d ) noexcept( _nothrow_access< Acc, T > )
{
        ZLL_ASSERT( ( detached< T, Acc >( d ) ) );
        T& first = first_node_of< T, Acc >( n );
        link_detached_as_prev< T, Acc >( first, d );
}

/// Link detached range [first, last] as successor of node `n`.
/// Undefined behavior if sublist [first, last] is not detached.
/// The range is linked as successor of `n` and the last element of the range is linked as
/// predecessor of previous `n` successor.
template < typename T, typename Acc = typename T::access >
requires( _provides_ll_header< T, Acc > )
void link_range_as_next( T& n, T& first, T& last ) noexcept( _nothrow_access< Acc, T > )
{
        ZLL_ASSERT( ( detached_range< T, Acc >( first, last ) ) );
        _link( Acc::get( first ), Acc::get( last ), Acc::get( n ), Acc::get( n )._next );
}

/// Link detached range [first, last] as predecessor of node `n`.
/// Undefined behavior if sublist [first, last] is not detached.
/// The range is linked as predecessor of `n` and the first element of the range is linked as
/// successor of previous `n` predecessor.
template < typename T, typename Acc = typename T::access >
requires( _provides_ll_header< T, Acc > )
void link_range_as_prev( T& n, T& first, T& last ) noexcept( _nothrow_access< Acc, T > )
{
        ZLL_ASSERT( ( detached_range< T, Acc >( first, last ) ) );
        _link( Acc::get( first ), Acc::get( last ), Acc::get( n )._prev, Acc::get( n ) );
}

/// Link nodes in `nodes` in order as successors of each other.
/// Undefined behavior if any of the nodes is not detached.
template < typename T, typename Acc = typename T::access >
requires( _provides_ll_header< T, Acc > )
void link_group( std::initializer_list< T* > nodes )
{
        if ( nodes.size() == 0 )
                return;
        auto  b = nodes.begin();
        auto* n = *b++;
        for ( auto e = nodes.end(); b != e; ++b ) {
                ZLL_ASSERT( ( detached< T, Acc >( **b ) ) );
                link_detached_as_next< T, Acc >( *n, **b );
                n = *b;
        }
}

/// Merge two ranges [lhf, lhl] and [rhf, rhl] into one range. Uses `comp` to determine the order of
/// the elements in the resulting range. Pointers to the first and last elements of the
/// resulting range are returned.
template < typename T, typename Acc, typename Compare = std::less<> >
requires( _provides_ll_header< T, Acc > )
std::pair< T*, T* >
merge_ranges( T& lhf, T& lhl, T& rhf, T& rhl, Compare&& comp = std::less<>{} ) noexcept(
    _nothrow_access< Acc, T > && noexcept( comp( lhf, rhf ) ) )
{
        _ll_hdr& ll = Acc::get( lhl );
        _ll_hdr& rf = Acc::get( rhf );
        _ll_hdr& rl = Acc::get( rhl );
        _unlink( rf, rl );
        _ll_hdr*       lh    = &Acc::get( lhf );
        _ll_hdr*       rh    = &rf;
        _ll_hdr*       first = nullptr;
        _ll_hdr*       last  = nullptr;
        _ll_word const pred  = lh->_prev;
        _ll_word       succ  = ll._next;
        while ( lh && rh ) {
                _ll_hdr* tmp = nullptr;
                if ( comp( _node< T, Acc >( *rh ), _node< T, Acc >( *lh ) ) ) {
                        tmp = rh;
                        rh  = rh->_next.a();
                } else {
                        tmp = lh;
                        lh  = lh == &ll ? nullptr : lh->_next.a();
                }
                _unlink( *tmp, *tmp );
                if ( last )
                        _link( *tmp, *tmp, *last, last->_next );
                else
                        first = tmp;
                last = tmp;
        }
        ZLL_ASSERT( first );
        ZLL_ASSERT( last );
        if ( lh ) {
                last->_next = *lh;
                lh->_prev   = *last;
                last        = &ll;
        } else if ( rh ) {
                last->_next = *rh;
                rh->_prev   = *last;
                last        = &rl;
        }
        _link( *first, *last, pred, succ );

        return { &_node< T, Acc >( *first ), &_node< T, Acc >( *last ) };
}

/// Remove all nodes in the range [first, last] for which `p` returns true. Returns the number of
/// removed nodes.
template < typename T, typename Acc, typename Pred >
requires( _provides_ll_header< T, Acc > )
std::size_t range_remove( T& first, T& last, Pred&& p ) noexcept(
    _nothrow_access< Acc, T > && noexcept( p( first ) ) )
{
        _ll_hdr*       n     = &Acc::get( first );
        _ll_hdr* const l     = &Acc::get( last );
        std::size_t    count = 0;

        for ( ;; ) {
                _ll_hdr* tmp = n->_next.a();
                if ( p( _node< T, Acc >( *n ) ) ) {
                        _unlink( *n, *n );
                        ++count;
                }
                if ( n == l )
                        break;
                n = tmp;
        }

        return count;
}

/// Reverse order of nodes in the range [first, last]. The first node in the range will become the
/// last node and the last node will become the first node.
template < typename T, typename Acc = typename T::access >
requires( _provides_ll_header< T, Acc > )
void range_reverse( T& first, T& last ) noexcept( _nothrow_access< Acc, T > )
{
        _ll_hdr* const f = &Acc::get( first );
        _ll_hdr&       l = Acc::get( last );
        _ll_hdr*       n = &l;
        while ( n != f ) {
                _ll_hdr* p = l._prev.a();
                ZLL_ASSERT( p != nullptr );
                _unlink( *p, *p );
                _link( *p, *p, *n, n->_next );
                n = p;
        }
}

/// Removes all consecutive nodes in the range [first, last] for which `p` returns true. Only first
/// element in each group of equal elements is left.
template < typename T, typename Acc, typename BinPred = std::equal_to<> >
requires( _provides_ll_header< T, Acc > )
std::size_t range_unique( T& first, T& last, BinPred&& p = std::equal_to<>{} ) noexcept(
    _nothrow_access< Acc, T > && noexcept( p( first, last ) ) )
{
        std::size_t    count = 0;
        _ll_hdr* const l     = &Acc::get( last );

        for ( _ll_hdr* m = &Acc::get( first ); m != l; ) {
                _ll_hdr* n = m->_next.a();
                if ( !n )
                        break;
                if ( p( _node< T, Acc >( *m ), _node< T, Acc >( *n ) ) ) {
                        _unlink( *n, *n );
                        ++count;
                } else {
                        m = n;
                }
        }
        return count;
}

/// Sort the range [first, last] using quicksort algorithm. The `cmp` is used to compare two nodes.
template < typename T, typename Acc, typename Compare = std::less<> >
requires( _provides_ll_header< T, Acc > )
void range_qsort( T& first, T& last, Compare&& cmp = std::less<>{} ) noexcept(
    _nothrow_access< Acc, T > && noexcept( cmp( first, last ) ) )
{
        if ( &first == &last )
                return;
        _ll_hdr&       pivot     = Acc::get( first );
        _ll_hdr* const l         = &Acc::get( last );
        _ll_hdr*       n         = pivot._next.a();
        _ll_hdr*       new_first = nullptr;
        _ll_hdr*       new_last  = nullptr;
        for ( ;; ) {
                _ll_hdr* next = n->_next.a();
                if ( cmp( _node< T, Acc >( *n ), first ) ) {
                        _unlink( *n, *n );
                        _link( *n, *n, pivot._prev, pivot );
                        if ( !new_first )
                                new_first = n;
                } else {
                        new_last = n;
                }
                if ( n == l )
                        break;
                n = next;
        }
        if ( new_last )
                range_qsort< T, Acc >(
                    _node< T, Acc >( *pivot._next.a() ), _node< T, Acc >( *new_last ), cmp );
        if ( new_first )
                range_qsort< T, Acc >(
                    _node< T, Acc >( *new_first ), _node< T, Acc >( *pivot._prev.a() ), cmp );
}

/// Standard linked list iterator, holds a pointer to the header of the node.
template < typename T, typename Acc = typename T::access >
requires( _provides_ll_header< T, Acc > )
struct ll_iterator
{
        using iterator_category = std::forward_iterator_tag;
        using value_type        = T;
        using difference_type   = std::ptrdiff_t;
        using pointer           = T*;
        using reference         = T&;

        ll_iterator() noexcept = default;

        ll_iterator( T* n ) noexcept
          : _h( n ? &Acc::get( *n ) : nullptr )
        {
        }

        reference operator*() const noexcept
        {
                ZLL_ASSERT( _h );
                return _node< T, Acc >( *_h );
        }

        pointer operator->() const noexcept
        {
                ZLL_ASSERT( _h );
                return &_node< T, Acc >( *_h );
        }

        ll_iterator& operator++() noexcept
        {
                _h = _h ? _h->_next.a() : nullptr;
                return *this;
        }

        ll_iterator operator++( int ) noexcept
        {
                ll_iterator tmp = *this;
                ++( *this );
                return tmp;
        }

        bool operator==( ll_iterator const& other ) const noexcept
        {
                return _h == other._h;
        }

        T* get() const noexcept
        {
                return _node< T, Acc >( _h );
        }

private:
        _ll_hdr* _h = nullptr;
};

/// Standard linked list const-iterator, holds a pointer to the header of the node.
template < typename T, typename Acc = typename T::access >
requires( _provides_ll_header< T, Acc > )
struct ll_const_iterator
{
        using iterator_category = std::forward_iterator_tag;
        using value_type        = T;
        using difference_type   = std::ptrdiff_t;
        using pointer           = T const*;
        using reference         = T const&;

        ll_const_iterator() noexcept = default;

        ll_const_iterator( T const* n ) noexcept
          : _h( n ? &Acc::get( *n ) : nullptr )
        {
        }

        ll_const_iterator( ll_iterator< T, Acc > const& it ) noexcept
          : ll_const_iterator( it.get() )
        {
        }

        reference operator*() const noexcept
        {
                ZLL_ASSERT( _h );
                return _node< T, Acc >( *_h );
        }

        pointer operator->() const noexcept
        {
                ZLL_ASSERT( _h );
                return &_node< T, Acc >( *_h );
        }

        ll_const_iterator& operator++() noexcept
        {
                _h = _h ? _h->_next.a() : nullptr;
                return *this;
        }

        ll_const_iterator operator++( int ) noexcept
        {
                ll_const_iterator tmp = *this;
                ++( *this );
                return tmp;
        }

        bool operator==( ll_const_iterator const& other ) const noexcept
        {
                return _h == other._h;
        }

private:
        _ll_hdr const* _h = nullptr;
};

/// Non-owning linked list container, expects nodes to contain ll_header as member.
/// The nodes are linked together in a doubly linked list, with the first and last nodes
/// accessible through the `front()` and `back()` methods.
///
/// Type `T` is the type of the node that contains the header.
/// Type `Acc` specifies how to access the node's header.
template < typename T, typename Acc >
struct ll_list : private _raw_list
{
        using value_type     = T;
        using iterator       = ll_iterator< T, Acc >;
        using const_iterator = ll_const_iterator< T, Acc >;

        static constexpr bool noexcept_access = _nothrow_access< Acc, T >;

        /// Default constructor creates an empty list.
        ll_list() = default;

        /// Constructs a list with nodes provided in the initializer list.
        /// Undefined behavior if any of the nodes is not detached.
        ll_list( std::initializer_list< T* > il ) noexcept( noexcept_access )
        {
                for ( auto* n : il ) {
                        ZLL_ASSERT( n );
                        ZLL_ASSERT( ( detached< T, Acc >( *n ) ) );
                        link_back( *n );
                }
        }

        /// Copy is not allowed
        ll_list( ll_list const& )            = delete;
        ll_list& operator=( ll_list const& ) = delete;

        /// Move constructor. Moved-from list is empty after move.
        ll_list( ll_list&& other ) noexcept
        {
                *this = std::move( other );
        }

        /// Move assignment operator. Moved-from list is empty after move.
        ll_list& operator=( ll_list&& other ) noexcept
        {
                if ( this != &other )
                        take( other );
                return *this;
        }

        T& front() noexcept
        {
                return _node< T, Acc >( *first );
        }

        T& back() noexcept
        {
                return _node< T, Acc >( *last );
        }

        T const& front() const noexcept
        {
                return _node< T, Acc >( *first );
        }

        T const& back() const noexcept
        {
                return _node< T, Acc >( *last );
        }

        iterator begin() noexcept
        {
                return iterator{ _node< T, Acc >( first ) };
        }

        const_iterator begin() const noexcept
        {
                return const_iterator{ _node< T, Acc >( first ) };
        }

        const_iterator cbegin() const noexcept
        {
                return const_iterator{ _node< T, Acc >( first ) };
        }

        iterator end() noexcept
        {
                return iterator{ nullptr };
        }

        const_iterator end() const noexcept
        {
                return const_iterator{ nullptr };
        }

        const_iterator cend() const noexcept
        {
                return const_iterator{ nullptr };
        }

        /// Merge two lists together, seeh `merge_ranges` for details. Uses std::less<>{} for
        /// comparison.
        void merge( ll_list&& other ) noexcept( noexcept_access )
        {
                merge( std::move( other ), std::less<>{} );
        }

        /// Merge two lists together, see `merge_ranges` for details.
        template < typename Compare >
        void merge( ll_list&& other, Compare comp ) noexcept(
            noexcept_access && noexcept( comp( front(), back() ) ) )
        {
                if ( this == &other || other.empty() )
                        return;
                if ( empty() ) {
                        *this = std::move( other );
                        return;
                }

                merge_ranges< T, Acc >( front(), back(), other.front(), other.back(), comp );
        }

        /// Removes all nodes compared equal to value `value` from the list. Returns the number of
        /// removed nodes.
        std::size_t
        remove( T const& value ) noexcept( noexcept_access && noexcept( front() == back() ) )
        {
                if ( empty() )
                        return 0;
                return range_remove< T, Acc >( front(), back(), [&value]( T& n ) noexcept {
                        return n == value;
                } );
        }

        /// Removes all nodes for which `p` returns true from the list. Returns the number of
        /// removed nodes.
        template < typename Pred >
        std::size_t remove_if( Pred&& p ) noexcept( noexcept_access && noexcept( p( front() ) ) )
        {
                if ( empty() )
                        return 0;
                return range_remove< T, Acc >( front(), back(), std::forward< Pred >( p ) );
        }

        /// Inserts the nodes from `other` into this list before position `pos`. If `pos` is equal
        /// to `end()`, the nodes are appended to the end of the list.
        void splice( iterator pos, ll_list&& other ) noexcept( noexcept_access )
        {
                if ( this == &other || other.empty() )
                        return;

                if ( empty() ) {
                        *this = std::move( other );
                } else if ( pos == end() ) {
                        auto* f = other.first;
                        auto* l = other.last;
                        other.detach_nodes();
                        _link( *f, *l, *last, last->_next );
                } else {
                        auto* f = other.first;
                        auto* l = other.last;
                        other.detach_nodes();
                        _ll_hdr& p = Acc::get( *pos );
                        _link( *f, *l, p._prev, p );
                }
        }

        /// Reverses the order of nodes in the list. The first node becomes the last and the last
        /// node becomes the first.
        void reverse() noexcept( noexcept_access )
        {
                if ( empty() )
                        return;
                range_reverse< T, Acc >( front(), back() );
        }

        /// Removes all consecutive nodes in the list for which `p` returns true. Only first
        /// element / in each group of equal elements is left. Returns the number of removed nodes.
        template < typename BinPred >
        std::size_t
        unique( BinPred p ) noexcept( noexcept_access && noexcept( p( front(), back() ) ) )
        {
                if ( empty() )
                        return 0;
                return range_unique< T, Acc >( front(), back(), std::move( p ) );
        }

        /// Removes all consecutive nodes in the list for which `std::equal_to<>` returns true.
        /// Only first element in each group of equal elements is left. Returns the number of
        /// removed nodes.
        std::size_t
        unique() noexcept( noexcept_access && noexcept( std::equal_to<>{}( front(), back() ) ) )
        {
                return unique( std::equal_to<>{} );
        }

        /// Sorts the nodes in the list. The `cmp` is used to compare two nodes.
        template < typename Compare >
        void sort( Compare&& cmp ) noexcept( noexcept_access && noexcept( cmp( front(), back() ) ) )
        {
                if ( empty() )
                        return;
                range_qsort< T, Acc >( front(), back(), std::forward< Compare >( cmp ) );
        }

        /// Sorts the nodes in the list. Uses `std::less<>` for comparison.
        void sort() noexcept( noexcept_access && noexcept( std::less<>{}( front(), back() ) ) )
        {
                sort( std::less<>{} );
        }

        /// Links the node `node` as the first element of the list. The previous first element
        /// becomes the second element. Detaches `node` from any other list it might be
        /// attached to.
        void link_front( T& node ) noexcept( noexcept_access )
        {
                detach< T, Acc >( node );
                _ll_hdr& h = Acc::get( node );
                _link( h, h, *this, first ? _ll_word( *first ) : _ll_word( *this ) );
        }

        /// Detaches the first element of the list. The second element becomes the first element.
        /// Undefined behavior if the list is empty.
        void detach_front() noexcept
        {
                _unlink( *first, *first );
        }

        /// Detaches and returns the first element of the list. The second element becomes the first
        /// element. Undefined behavior if the list is empty.
        T& take_front() noexcept
        {
                T& node = front();
                _unlink( *first, *first );
                return node;
        }

        /// Returns true if the list is empty, i.e. contains no elements.
        bool empty() const noexcept
        {
                return !first;
        }

        /// Links the node `node` as the last element of the list. The previous last element
        /// becomes the second last element. Detaches `node` from any other list it might
        /// be attached to.
        void link_back( T& node ) noexcept( noexcept_access )
        {
                detach< T, Acc >( node );
                _ll_hdr& h = Acc::get( node );
                _link( h, h, last ? _ll_word( *last ) : _ll_word( *this ), *this );
        }

        /// Detaches the last element of the list. The second last element becomes the last
        /// element. Undefined behavior if the list is empty.
        void detach_back() noexcept
        {
                _unlink( *last, *last );
        }

        /// Detaches and returns the last element of the list. The second last element becomes the
        /// last element. Undefined behavior if the list is empty.
        T& take_back() noexcept
        {
                T& node = back();
                _unlink( *last, *last );
                return node;
        }

        /// Detaches all nodes in the list. The list becomes empty after this operation.
        /// The nodes themselves are still linked together, but not to this list.
        ~ll_list() noexcept
        {
                detach_nodes();
        }
};

template < typename Derived >
struct ll_base;

/// Access type to the header of ll_base.
template < typename Derived >
struct _ll_base_access
{
        using header = ll_header< Derived, _ll_base_access >;

        static header& get( Derived& d ) noexcept
        {
                return static_cast< header& >( static_cast< ll_base< Derived >& >( d ) );
        }

        static header const& get( Derived const& d ) noexcept
        {
                return static_cast< header const& >(
                    static_cast< ll_base< Derived > const& >( d ) );
        }

        static Derived& node( header& h ) noexcept
        {
                return static_cast< Derived& >( static_cast< ll_base< Derived >& >( h ) );
        }
};

/// CRTP base class for linked list nodes containing `ll_header`. Provides access type to the header
/// of the node and implements move and copy semantics for the node. Provides basic API for the
/// node.
///
/// Note that for copy construction to work it has to use non-const reference to the node. This is
/// so we can re-link the copied node into the list.
template < typename Derived >
struct ll_base : private ll_header< Derived, _ll_base_access< Derived > >
{
        /// Access type to the header of ll_base.
        using access = _ll_base_access< Derived >;
        friend struct _ll_base_access< Derived >;

        /// Default constructor node is detached
        ll_base() noexcept = default;

        /// Move constructor, moved-from node is detached. The new node is linked to the
        /// list of the moved-from node instead of it.
        ll_base( ll_base&& o ) noexcept
        {
                _ll_move( o, *this );
        }

        /// Copy constructor, copied node is linked to the list of the copied node after it.
        ll_base( ll_base& o ) noexcept
        {
                _ll_hdr& n = o;
                _link( *this, *this, n, n._next );
        }

        /// Move assignment operator, moved-from node is detached. The new node is linked to the
        /// list of the moved-from node instead of it.
        /// If the moved-from node is the same as the current node, nothing happens.
        ll_base& operator=( ll_base&& o ) noexcept
        {
                if ( this == &o )
                        return *this;
                detach< Derived, access >( derived() );
                move_from_to< Derived, access >( o.derived(), derived() );
                return *this;
        }

        /// Copy assignment operator, copied node is linked to the list of the copied node after
        /// it. If the copied node is the same as the current node, nothing happens.
        // NOLINTNEXTLINE(misc-unconventional-assign-operator)
        ll_base& operator=( ll_base& o ) noexcept
        {
                if ( this == &o )
                        return *this;
                detach< Derived, access >( derived() );
                link_detached_as_next< Derived, access >( o.derived(), derived() );
                return *this;
        }

        /// Link node `n` as successor of the current node. Node `n` is detached if it was already
        /// linked.
        void link_next( Derived& n ) noexcept
        {
                detach< Derived, access >( n );
                link_detached_as_next< Derived, access >( derived(), n );
        }

        /// Link node `n` as predecessor of the current node. Node `n` is detached if it was
        /// already linked.
        void link_prev( Derived& n ) noexcept
        {
                detach< Derived, access >( n );
                link_detached_as_prev< Derived, access >( derived(), n );
        }

        Derived* next()
        {
                return _node< Derived, access >( _ll_hdr::_next );
        }

        Derived const* next() const
        {
                return _node< Derived, access >( _ll_hdr::_next );
        }

        Derived* prev()
        {
                return _node< Derived, access >( _ll_hdr::_prev );
        }

        Derived const* prev() const
        {
                return _node< Derived, access >( _ll_hdr::_prev );
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
};

/// Iterate over all nodes in the list starting from `n` and call `f` for each node.
/// The order of the nodes is: predecessors, `n`, successors.
template < typename T, typename Acc = typename T::access >
requires( _provides_ll_header< T, Acc > )
void for_each_node( T& n, std::invocable< T& > auto&& f ) noexcept(
    _nothrow_access< Acc, T > && noexcept( f( n ) ) )
{
        auto& h = Acc::get( n );
        for ( _ll_hdr* m = h._prev.a(); m; m = m->_prev.a() )
                f( _node< T, Acc >( *m ) );
        f( n );
        for ( _ll_hdr* m = h._next.a(); m; m = m->_next.a() )
                f( _node< T, Acc >( *m ) );
}

/// Iterate over all nodes in the list starting from `n` until node for which `f` returns true is
/// found. Return pointer to such node, nullptr otherwise.
///
/// The order of the nodes is: predecessors, `n`, successors.
template < typename T, typename Acc = typename T::access >
requires( _provides_ll_header< T, Acc > )
T* find_if_node( T& n, std::invocable< T& > auto&& f ) noexcept(
    _nothrow_access< Acc, T > && noexcept( f( n ) ) )
{
        auto& h = Acc::get( n );
        for ( _ll_hdr* m = h._prev.a(); m; m = m->_prev.a() )
                if ( T& x = _node< T, Acc >( *m ); f( x ) )
                        return &x;
        if ( f( n ) )
                return &n;
        for ( _ll_hdr* m = h._next.a(); m; m = m->_next.a() )
                if ( T& x = _node< T, Acc >( *m ); f( x ) )
                        return &x;
        return nullptr;
}

template < typename T, typename Acc = typename T::access, typename Compare = std::less<> >
struct sh_header;

template < typename T, typename Acc = typename T::access, typename Compare = std::less<> >
struct sh_heap;

struct _sh_hdr;
struct _raw_heap;

using _sh_word = _vptr< _sh_hdr, _raw_heap >;

struct _sh_hdr
{
        _sh_hdr* _left   = nullptr;
        _sh_hdr* _right  = nullptr;
        _sh_word _parent = nullptr;

        _sh_hdr() noexcept                       = default;
        _sh_hdr( _sh_hdr const& )                = delete;
        _sh_hdr( _sh_hdr&& ) noexcept            = delete;
        _sh_hdr& operator=( _sh_hdr const& )     = delete;
        _sh_hdr& operator=( _sh_hdr&& ) noexcept = delete;
};

struct _raw_heap
{
        _sh_hdr* root = nullptr;
};

template < typename T, typename Acc >
concept _provides_sh_header = requires( T& t, _hdr_of< T, Acc >& h ) {
        { Acc::get( t ) } -> std::convertible_to< _sh_hdr const& >;
        { Acc::node( h ) } noexcept -> std::same_as< std::remove_const_t< T >& >;
};

inline _sh_hdr& _sh_detach_left( _sh_hdr& n ) noexcept
{
        _sh_hdr& c = *n._left;
        c._parent  = nullptr;
        n._left    = nullptr;
        return c;
}

inline _sh_hdr& _sh_detach_right( _sh_hdr& n ) noexcept
{
        _sh_hdr& c = *n._right;
        c._parent  = nullptr;
        n._right   = nullptr;
        return c;
}

inline _sh_hdr& _sh_detach_top( _raw_heap& h ) noexcept
{
        _sh_hdr& t = *h.root;
        t._parent  = nullptr;
        h.root     = nullptr;
        return t;
}

inline _sh_word _sh_detach_parent( _sh_hdr& n ) noexcept
{
        _sh_word p = n._parent;
        if ( _sh_hdr* x = p.a() )
                if ( x->_left == &n )
                        x->_left = nullptr;
                else
                        x->_right = nullptr;
        else if ( p )
                p.b()->root = nullptr;
        n._parent = nullptr;
        return p;
}

inline void _sh_attach_left( _sh_hdr& parent, _sh_hdr& n ) noexcept
{
        parent._left = &n;
        n._parent    = parent;
}

inline void _sh_attach_right( _sh_hdr& parent, _sh_hdr& n ) noexcept
{
        parent._right = &n;
        n._parent     = parent;
}

inline void _sh_attach_top( _raw_heap& h, _sh_hdr& n ) noexcept
{
        h.root    = &n;
        n._parent = h;
}

inline void _sh_attach_parent( _sh_hdr& n, _sh_word p ) noexcept
{
        n._parent = p;
        if ( _sh_hdr* x = p.a() )
                if ( !x->_left )
                        x->_left = &n;
                else
                        x->_right = &n;
        else if ( p )
                p.b()->root = &n;
}

inline void _sh_replace_in_parent( _sh_hdr& n, _sh_hdr& new_n ) noexcept
{
        if ( _sh_hdr* x = n._parent.a() )
                if ( x->_left == &n )
                        _sh_attach_left( *x, new_n );
                else
                        _sh_attach_right( *x, new_n );
        else if ( n._parent )
                _sh_attach_top( *n._parent.b(), new_n );
        n._parent = nullptr;
}

/// Links detached `copy` right below `original`. A copy compares equal to the original, so the
/// heap stays valid without calling the comparator, which could not see the copy's value yet.
inline void _sh_link_copy( _sh_hdr& original, _sh_hdr& copy ) noexcept
{
        if ( original._right )
                _sh_attach_left( copy, _sh_detach_right( original ) );
        _sh_attach_right( original, copy );
}

inline void _sh_move( _sh_hdr& from, _sh_hdr& to ) noexcept
{
        if ( from._left )
                _sh_attach_left( to, _sh_detach_left( from ) );
        if ( from._right )
                _sh_attach_right( to, _sh_detach_right( from ) );
        if ( from._parent )
                _sh_replace_in_parent( from, to );
}

/// Node that owns header `h`.
template < typename T, typename Acc >
T& _node( _sh_hdr& h ) noexcept
{
        return Acc::node( static_cast< _hdr_of< T, Acc >& >( h ) );
}

template < typename T, typename Acc, typename Compare >
_sh_hdr& _sh_merge( _sh_hdr& left, _sh_hdr& right, Compare&& comp ) noexcept(
    _nothrow_access_compare< Acc, T, Compare > );

template < typename T, typename Acc, typename Compare >
_sh_hdr& _sh_merge_impl( _sh_hdr& left, _sh_hdr& right, Compare&& comp ) noexcept(
    _nothrow_access_compare< Acc, T, Compare > )
{
        _sh_hdr* left_left = nullptr;
        if ( left._left )
                left_left = &_sh_detach_left( left );

        if ( left._right ) {
                auto& left_right = _sh_detach_right( left );
                auto& new_left   = _sh_merge< T, Acc >( left_right, right, comp );
                _sh_attach_left( left, new_left );
        } else {
                _sh_attach_left( left, right );
        }
        if ( left_left )
                _sh_attach_right( left, *left_left );

        return left;
}

template < typename T, typename Acc, typename Compare >
_sh_hdr& _sh_merge( _sh_hdr& left, _sh_hdr& right, Compare&& comp ) noexcept(
    _nothrow_access_compare< Acc, T, Compare > )
{
        ZLL_ASSERT( !left._parent );
        ZLL_ASSERT( !right._parent );

        if ( comp( _node< T, Acc >( right ), _node< T, Acc >( left ) ) )
                return _sh_merge_impl< T, Acc >( right, left, comp );
        else
                return _sh_merge_impl< T, Acc >( left, right, comp );
}

template < typename T, typename Acc, typename Compare >
_sh_hdr*
_sh_pop( _sh_hdr& h, Compare&& comp ) noexcept( _nothrow_access_compare< Acc, T, Compare > )
{
        if ( h._left && h._right ) {
                auto& l = _sh_detach_left( h );
                auto& r = _sh_detach_right( h );
                return &_sh_merge< T, Acc >( l, r, comp );
        }
        if ( h._left )
                return &_sh_detach_left( h );
        if ( h._right )
                return &_sh_detach_right( h );
        return nullptr;
}

template < typename T, typename Acc, typename Compare >
void _sh_detach( _sh_hdr& h, Compare&& comp ) noexcept( _nothrow_access_compare< Acc, T, Compare > )
{
        if ( _sh_hdr* n = _sh_pop< T, Acc >( h, comp ) )
                _sh_replace_in_parent( h, *n );
        else
                _sh_detach_parent( h );
}

/// Returns true if the node is detached from heap.
template < typename T, typename Acc = typename T::access >
requires( _provides_sh_header< T, Acc > )
bool detached( T& node ) noexcept( _nothrow_access< Acc, T > )
{
        auto& n_hdr = Acc::get( node );
        return !n_hdr._left && !n_hdr._right && !n_hdr._parent;
}

/// Links all children from `from` node to `to` node. The `to` node must be detached.
template < typename T, typename Acc = typename T::access >
requires( _provides_sh_header< T, Acc > )
void move_from_to( T& from, T& to ) noexcept( _nothrow_access< Acc, T > )
{
        ZLL_ASSERT( ( detached< T, Acc >( to ) ) );
        _sh_move( Acc::get( from ), Acc::get( to ) );
}

/// Link a detached node `other` to `node`. Maintains the heap property using `comp`. The `other`
/// node must be detached before calling this function.
template < typename T, typename Acc = typename T::access, typename Compare = std::less<> >
requires( _provides_sh_header< T, Acc > )
void link_detached_to( T& node, T& other, Compare&& comp = std::less<>{} ) noexcept(
    _nothrow_access_compare< Acc, T, Compare > )
{
        ZLL_ASSERT( ( detached< T, Acc >( other ) ) );

        _sh_hdr& n = Acc::get( node );
        _sh_hdr* o = &Acc::get( other );
        if ( n._right )
                o = &_sh_merge< T, Acc >( _sh_detach_right( n ), *o, comp );
        _sh_attach_right( n, *o );
}

/// Unlink a node from the heap. If the node has two children, they are merged using `comp` and the
/// result is linked to the parent of the detached node. If the node has one child, that child is
/// linked to the parent of the detached node. If the node has no children, the parent pointer is
/// set to nullptr.
template < typename T, typename Acc = typename T::access, typename Compare >
requires( _provides_sh_header< T, Acc > )
void detach( T& node, Compare&& comp ) noexcept( _nothrow_access_compare< Acc, T, Compare > )
{
        _sh_detach< T, Acc >( Acc::get( node ), comp );
}

template < typename T, typename Acc >
void _sh_inorder( _sh_hdr* h, auto& f )
{
        if ( !h )
                return;
        _sh_inorder< T, Acc >( h->_left, f );
        f( _node< T, Acc >( *h ) );
        _sh_inorder< T, Acc >( h->_right, f );
}

template < typename T, typename Acc >
void _sh_preorder( _sh_hdr* h, auto& f )
{
        if ( !h )
                return;
        f( _node< T, Acc >( *h ) );
        _sh_preorder< T, Acc >( h->_left, f );
        _sh_preorder< T, Acc >( h->_right, f );
}

template < typename T, typename Acc >
void _sh_postorder( _sh_hdr* h, auto& f )
{
        if ( !h )
                return;
        _sh_postorder< T, Acc >( h->_left, f );
        _sh_postorder< T, Acc >( h->_right, f );
        f( _node< T, Acc >( *h ) );
}

/// Traverse the heap in-order and call `f` for each node. The order of the nodes is: left child,
/// node, right child.
template < typename T, typename Acc = typename T::access >
requires( _provides_sh_header< T, Acc > )
void inorder_traverse( T& n, std::invocable< T& > auto&& f ) noexcept(
    _nothrow_access< Acc, T > && noexcept( f( n ) ) )
{
        auto& h = Acc::get( n );
        _sh_inorder< T, Acc >( h._left, f );
        f( n );
        _sh_inorder< T, Acc >( h._right, f );
}

/// Traverse the heap pre-order and call `f` for each node. The order of the nodes is: node, left
/// child, right child.
template < typename T, typename Acc = typename T::access >
requires( _provides_sh_header< T, Acc > )
void preorder_traverse( T& n, std::invocable< T& > auto&& f ) noexcept(
    _nothrow_access< Acc, T > && noexcept( f( n ) ) )
{
        auto& h = Acc::get( n );
        f( n );
        _sh_preorder< T, Acc >( h._left, f );
        _sh_preorder< T, Acc >( h._right, f );
}

/// Traverse the heap post-order and call `f` for each node. The order of the nodes is: left child,
/// right child, node.
template < typename T, typename Acc = typename T::access >
requires( _provides_sh_header< T, Acc > )
void postorder_traverse( T& n, std::invocable< T& > auto&& f ) noexcept(
    _nothrow_access< Acc, T > && noexcept( f( n ) ) )
{
        auto& h = Acc::get( n );
        _sh_postorder< T, Acc >( h._left, f );
        _sh_postorder< T, Acc >( h._right, f );
        f( n );
}

/// Link a detached node `n2` to the parent of `n1`. Maintains the heap property using `comp`. The
/// `n2` node must be detached before calling this function.
template < typename T, typename Acc = typename T::access, typename Compare = std::less<> >
requires( _provides_sh_header< T, Acc > )
void link_detached( T& n1, T& n2, Compare&& comp = std::less<>{} ) noexcept(
    _nothrow_access_compare< Acc, T, Compare > )
{
        ZLL_ASSERT( ( detached< T, Acc >( n2 ) ) );

        _sh_hdr& h1 = Acc::get( n1 );
        _sh_word p  = _sh_detach_parent( h1 );
        _sh_attach_parent( _sh_merge< T, Acc >( h1, Acc::get( n2 ), comp ), p );
}

/// Returns the top node of the heap that `node` is in. The top node is the node that has no parent
/// and is an ancestor of `node`. If `node` is detached, it is returned.
template < typename T, typename Acc = typename T::access >
requires( _provides_sh_header< T, Acc > )
T& top_node_of( T& node ) noexcept( _nothrow_access< Acc, T > )
{
        _sh_hdr* h = Acc::get( node )._parent.a();
        if ( !h )
                return node;
        while ( _sh_hdr* p = h->_parent.a() )
                h = p;
        return _node< T, Acc >( *h );
}

/// Skew heap header containing links to the headers of the left and right children and to the
/// parent header or the heap.
///
/// Type `T` is the type of the node that contains the header.
/// Type `Acc` maps a node to its header (`get`) and a header back to its node (`node`).
template < typename T, typename Acc, typename Compare >
struct sh_header : _sh_hdr
{
};

template < typename Derived, typename Compare >
struct sh_base;

/// Access type to the header of sh_base.
template < typename Derived, typename Compare >
struct _sh_base_access
{
        using header = sh_header< Derived, _sh_base_access, Compare >;

        static header& get( Derived& d ) noexcept
        {
                return static_cast< header& >( static_cast< sh_base< Derived, Compare >& >( d ) );
        }

        static header const& get( Derived const& d ) noexcept
        {
                return static_cast< header const& >(
                    static_cast< sh_base< Derived, Compare > const& >( d ) );
        }

        static Derived& node( header& h ) noexcept
        {
                return static_cast< Derived& >( static_cast< sh_base< Derived, Compare >& >( h ) );
        }
};

/// CRTP base class for skew heap nodes containing `sh_header`. Provides access type to the header
/// of the node and implements move and copy semantics for the node. Provides basic API for the node
template < typename Derived, typename Compare = std::less<> >
struct sh_base : private sh_header< Derived, _sh_base_access< Derived, Compare >, Compare >
{
        using access = _sh_base_access< Derived, Compare >;
        friend struct _sh_base_access< Derived, Compare >;

        sh_base() noexcept = default;

        sh_base( sh_base&& o ) noexcept
        {
                _sh_move( o, *this );
        }

        sh_base& operator=( sh_base&& o ) noexcept
        {
                if ( this == &o )
                        return *this;
                detach< Derived, access >( derived(), _comp );
                move_from_to< Derived, access >( o.derived(), derived() );
                return *this;
        }

        /// Copy constructor, the copy is linked right below the copied node.
        sh_base( sh_base& o ) noexcept
        {
                _sh_link_copy( o, *this );
        }

        /// Copy assignment operator, the node is detached and linked right below the copied node.
        // NOLINTNEXTLINE(misc-unconventional-assign-operator)
        sh_base& operator=( sh_base& o ) noexcept
        {
                if ( this == &o )
                        return *this;
                detach< Derived, access >( derived(), _comp );
                _sh_link_copy( o, *this );
                return *this;
        }

        ~sh_base() noexcept
        {
                _sh_detach< Derived, access >( *this, _comp );
        }

protected:
        Derived& derived() noexcept
        {
                return *static_cast< Derived* >( this );
        }

        Derived const& derived() const noexcept
        {
                return *static_cast< Derived const* >( this );
        }

private:
        [[no_unique_address]] Compare _comp;
};

/// Skew heap implementation. Provides API for linking and merging nodes, merging and popping the
/// heap, checking if the heap is empty and accessing the top node of the heap. The top node is the
/// node with the smallest value in the heap according to the comparison function `Compare`.
template < typename T, typename Acc, typename Compare >
struct sh_heap : private _raw_heap
{
        static constexpr bool noexcept_access  = _nothrow_access< Acc, T >;
        static constexpr bool noexcept_compare = _nothrow_access_compare< Acc, T, Compare >;

        sh_heap() noexcept                   = default;
        sh_heap( sh_heap const& )            = delete;
        sh_heap& operator=( sh_heap const& ) = delete;

        /// Constructs a heap with the given comparison function. The comparison function is used to
        /// maintain the heap property when linking and merging nodes. The top node of the heap is
        /// the node with the smallest value according to the comparison function.
        sh_heap( Compare comp )
          : _comp( std::move( comp ) )
        {
        }

        /// Move constructor, moved-from heap becomes empty. If top node is present in the
        /// moved-from heap, it is detached and attached to the new heap.
        sh_heap( sh_heap&& other ) noexcept
          : _comp( std::move( other._comp ) )
        {
                if ( other.root )
                        _sh_attach_top( *this, _sh_detach_top( other ) );
        }

        /// Move assignment operator, moved-from heap becomes empty. If top node is present in the
        /// moved-from heap, it is detached and attached to the new heap. If the current heap has
        /// a top node, it is detached before attaching the new top node.
        sh_heap& operator=( sh_heap&& other ) noexcept
        {
                if ( this == &other )
                        return *this;
                _comp = std::move( other._comp );
                if ( root )
                        _sh_detach_top( *this );
                if ( other.root )
                        _sh_attach_top( *this, _sh_detach_top( other ) );
                return *this;
        }

        /// Constructs a heap from an initializer list of nodes. All nodes in the initializer list
        /// must be detached. The nodes are linked together to form the heap using the comparison
        /// function `Compare`.
        sh_heap( std::initializer_list< T* > il ) noexcept( noexcept_compare )
        {
                // XXX: well, this could be more optimal
                for ( auto* n : il ) {
                        ZLL_ASSERT( n );
                        ZLL_ASSERT( ( detached< T, Acc >( *n ) ) );
                        link( *n );
                }
        }

        /// Destructor, detaches the top node if present.
        ~sh_heap() noexcept
        {
                if ( root )
                        _sh_detach_top( *this );
        }

        /// Links the node `node` into the heap. The node must be detached before calling this
        /// function. The heap property is maintained using the comparison function `Compare`.
        void link( T& node ) noexcept( noexcept_compare )
        {
                static_assert(
                    std::is_convertible_v<
                        decltype( Acc::get( node ) ),
                        sh_header< T, Acc, Compare >& >,
                    "the node's sh_header has to use the heap's Compare" );
                _sh_hdr* n = &Acc::get( node );
                if ( root )
                        n = &_sh_merge< T, Acc >( _sh_detach_top( *this ), *n, _comp );
                _sh_attach_top( *this, *n );
        }

        /// Merges the `other` heap into this heap. The `other` heap becomes empty after this
        /// operation. The heap property is maintained using the comparison function `Compare`.
        void merge( sh_heap&& other ) noexcept( noexcept_compare )
        {
                if ( this == &other || other.empty() )
                        return;
                _sh_hdr* n = &_sh_detach_top( other );
                if ( root )
                        n = &_sh_merge< T, Acc >( _sh_detach_top( *this ), *n, _comp );
                _sh_attach_top( *this, *n );
        }

        /// Returns true if the heap is empty, i.e. contains no nodes.
        bool empty() const noexcept
        {
                return !root;
        }

        /// Returns the top node of the heap, or null if the heap is empty.
        T* top() noexcept
        {
                return root ? &_node< T, Acc >( *root ) : nullptr;
        }

        /// Returns the top node of the heap, or null if the heap is empty.
        T const* top() const noexcept
        {
                return root ? &_node< T, Acc >( *root ) : nullptr;
        }

        /// Unlinks the top node from the heap. The new top node is determined by
        /// merging the left and right children of the detached top node. Undefined behavior if the
        /// heap is empty.
        void pop() noexcept( noexcept_compare )
        {
                ZLL_ASSERT( root );
                if ( _sh_hdr* n = _sh_pop< T, Acc >( _sh_detach_top( *this ), _comp ) )
                        _sh_attach_top( *this, *n );
        }

        /// Unlinks and returns the top node from the heap. The new top node is determined as if
        /// `pop` is used.
        T& take() noexcept( noexcept_compare )
        {
                ZLL_ASSERT( root );
                T& n = _node< T, Acc >( *root );
                pop();
                return n;
        }

private:
        [[no_unique_address]] Compare _comp{};
};

}  // namespace zll
