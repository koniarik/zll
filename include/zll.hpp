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
#include <iterator>

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
concept _nothrow_access = noexcept( Acc::get( (T*) nullptr ) );

template < typename T, typename Acc = typename T::access >
concept _provides_ll_header = requires( T& t ) {
        {
                Acc::get( t )
        } -> std::convertible_to< ll_header< std::remove_const_t< T >, Acc > const& >;
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
                return ptr && is_a() ? std::bit_cast< A* >( ptr ) : nullptr;
        }

        B* b() const noexcept
        {
                static_assert( alignof( B ) > 2 );
                return ptr && !is_a() ? std::bit_cast< B* >( ptr & ~mask ) : nullptr;
        }

        friend auto operator<=>( _vptr const& lh, _vptr const& rh ) noexcept = default;
};

/// Variadic ptr wrapper pointer either to ll_list or node with ll_header.
template < typename T, typename Acc = typename T::access >
using _ll_ptr = _vptr< T, ll_list< T, Acc > >;

template < typename T, typename Acc = typename T::access >
constexpr auto* _node( _ll_ptr< T, Acc > p ) noexcept
{
        return p.a();
}

template < typename T, typename Acc = typename T::access >
constexpr auto* _list( _ll_ptr< T, Acc > p ) noexcept
{
        return p.b();
}

template < typename T, typename Acc = typename T::access >
void _prev_or_last_set( _ll_ptr< T, Acc > p, _ll_ptr< T, Acc > n )
{
        if ( T* x = _node( p ) )
                Acc::get( *x ).prev = n;
        else if ( auto* h = _list( p ) )
                h->_last = _node( n );
}

template < typename T, typename Acc = typename T::access >
void _next_or_first_set( _ll_ptr< T, Acc > p, _ll_ptr< T, Acc > n )
{
        if ( T* x = _node( p ) )
                Acc::get( *x ).next = n;
        else if ( auto* h = _list( p ) )
                h->_first = _node( n );
}

/// Linked-list header containing pointers to the next and previous elements or the list itself.
/// Will detach itself from the linked list on destruction.
///
/// Type `T` is the type of the node that contains this header.
/// Type `Acc` is the access type that provides access to the header of the node.
template < typename T, typename Acc >
struct ll_header
{
        _ll_ptr< T, Acc > next = nullptr;
        _ll_ptr< T, Acc > prev = nullptr;

        ll_header() noexcept                               = default;
        ll_header( ll_header&& other ) noexcept            = delete;
        ll_header( ll_header const& other )                = delete;
        ll_header& operator=( ll_header&& other ) noexcept = delete;
        ll_header& operator=( ll_header const& other )     = delete;

        ~ll_header() noexcept( _nothrow_access< Acc, T > )
        {
                _prev_or_last_set( next, prev );
                _next_or_first_set( prev, next );
        }
};

/// Unlink a node from existing list. Previous or following node are linked together instead.
/// Node itself does not keep any connections.
template < typename T, typename Acc = typename T::access >
requires( _provides_ll_header< T, Acc > )
void detach( T& node ) noexcept( _nothrow_access< Acc, T > )
{
        auto& n_hdr = Acc::get( node );

        _prev_or_last_set( n_hdr.next, n_hdr.prev );
        _next_or_first_set( n_hdr.prev, n_hdr.next );

        n_hdr.next = nullptr;
        n_hdr.prev = nullptr;
}

/// Returns true if the node is detached from list.
template < typename T, typename Acc = typename T::access >
requires( _provides_ll_header< T, Acc > )
bool detached( T& node ) noexcept( _nothrow_access< Acc, T > )
{
        auto& n_hdr = Acc::get( node );
        return !n_hdr.next && !n_hdr.prev;
}

/// Detaches subrange [first, last] from the list. The range is not linked to any other node after
/// detachment. Successor of `last` and predecessor of `first` are linked together.
template < typename T, typename Acc = typename T::access >
requires( _provides_ll_header< T, Acc > )
void detach_range( T& first, T& last ) noexcept( _nothrow_access< Acc, T > )
{
        _prev_or_last_set( Acc::get( last ).next, Acc::get( first ).prev );
        _next_or_first_set( Acc::get( first ).prev, Acc::get( last ).next );

        Acc::get( first ).prev = nullptr;
        Acc::get( last ).next  = nullptr;
}

/// Returns true if the range is detached from list.
template < typename T, typename Acc = typename T::access >
requires( _provides_ll_header< T, Acc > )
bool detached_range( T& first, T& last ) noexcept( _nothrow_access< Acc, T > )
{
        return !Acc::get( first ).prev && !Acc::get( last ).next;
}

/// Links predecessor and successor of `from` node as predecessor and successor of `to` node.
/// Can be used to implement move semantics of custom nodes. Undefined behavior if `to` is not
/// detached.
template < typename T, typename Acc = typename T::access >
requires( _provides_ll_header< T, Acc > )
void move_from_to( T& from, T& to ) noexcept( _nothrow_access< Acc, T > )
{
        ZLL_ASSERT( detached( to ) );
        auto& from_hdr = Acc::get( from );
        auto& to_hdr   = Acc::get( to );

        to_hdr.next = from_hdr.next;
        to_hdr.prev = from_hdr.prev;

        _prev_or_last_set< T, Acc >( to_hdr.next, to );
        _next_or_first_set< T, Acc >( to_hdr.prev, to );

        from_hdr.next = nullptr;
        from_hdr.prev = nullptr;
}

/// Link detached node `d` after node `n`, any successor of `n` will be successor of `d`.
/// Undefined behavior if `d` is not detached.
template < typename T, typename Acc = typename T::access >
requires( _provides_ll_header< T, Acc > )
void link_detached_as_next( T& n, T& d ) noexcept( _nothrow_access< Acc, T > )
{
        ZLL_ASSERT( detached( d ) );
        auto& e_hdr = Acc::get( d );
        auto& n_hdr = Acc::get( n );

        e_hdr.next = n_hdr.next;
        _prev_or_last_set< T, Acc >( e_hdr.next, d );

        n_hdr.next = d;
        e_hdr.prev = n;
}

/// Link detached node `d` before node `n`, any predecessor of `n` will be predecessor of `d`.
/// Undefined behavior if `d` is not detached.
template < typename T, typename Acc = typename T::access >
requires( _provides_ll_header< T, Acc > )
void link_detached_as_prev( T& n, T& d ) noexcept( _nothrow_access< Acc, T > )
{
        ZLL_ASSERT( detached( d ) );
        auto& e_hdr = Acc::get( d );
        auto& n_hdr = Acc::get( n );

        e_hdr.prev = n_hdr.prev;
        _next_or_first_set< T, Acc >( e_hdr.prev, d );

        n_hdr.prev = d;
        e_hdr.next = n;
}

/// Iterate over predecessors of node `n` and return the first node in the list.
template < typename T, typename Acc = typename T::access >
requires( _provides_ll_header< T, Acc > )
T& first_node_of( T& n ) noexcept( _nothrow_access< Acc, T > )
{
        auto* p = &n;
        while ( p ) {
                auto* pp = Acc::get( p ).prev.node();
                if ( !pp )
                        break;
                p = pp;
        }
        return *p;
}

/// Iterate over successors of node `n` and return the last node in the list.
template < typename T, typename Acc = typename T::access >
requires( _provides_ll_header< T, Acc > )
T& last_node_of( T& n ) noexcept( _nothrow_access< Acc, T > )
{
        auto* p = &n;
        while ( p ) {
                auto* pp = _node( Acc::get( *p ).next );
                if ( !pp )
                        break;
                p = pp;
        }
        return *p;
}

/// Link detached node `d` as last element of the list accessed by node `n`.
/// Undefined behavior if `d` is not detached.
template < typename T, typename Acc = typename T::access >
requires( _provides_ll_header< T, Acc > )
void link_detached_as_last( T& n, T& d ) noexcept( _nothrow_access< Acc, T > )
{
        ZLL_ASSERT( detached( d ) );
        T& last = last_node_of( n );
        link_detached_as_next< T, Acc >( last, d );
}

/// Link detached node `d` as first element of the list accessed by node `n`.
/// Undefined behavior if `d` is not detached.
template < typename T, typename Acc = typename T::access >
requires( _provides_ll_header< T, Acc > )
void link_detached_as_first( T& n, T& d ) noexcept( _nothrow_access< Acc, T > )
{
        ZLL_ASSERT( detached( d ) );
        T& first = first_node_of( n );
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
        ZLL_ASSERT( detached_range( first, last ) );
        Acc::get( last ).next = Acc::get( n ).next;
        _prev_or_last_set< T, Acc >( Acc::get( n ).next, last );

        Acc::get( first ).prev = n;
        Acc::get( n ).next     = first;
}

/// Link detached range [first, last] as predecessor of node `n`.
/// Undefined behavior if sublist [first, last] is not detached.
/// The range is linked as predecessor of `n` and the first element of the range is linked as
/// successor of previous `n` predecessor.
template < typename T, typename Acc = typename T::access >
requires( _provides_ll_header< T, Acc > )
void link_range_as_prev( T& n, T& first, T& last ) noexcept( _nothrow_access< Acc, T > )
{
        ZLL_ASSERT( detached_range( first, last ) );
        Acc::get( first ).prev = Acc::get( n ).prev;
        _next_or_first_set< T, Acc >( Acc::get( n ).prev, first );

        Acc::get( last ).next = n;
        Acc::get( n ).prev    = last;
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
                ZLL_ASSERT( detached( **b ) );
                link_detached_as_next( *n, **b );
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
        detach_range( rhf, rhl );
        T*      lh    = &lhf;
        T*      rh    = &rhf;
        T*      first = nullptr;
        T*      last  = nullptr;
        _ll_ptr pred  = Acc::get( lhf ).prev;
        _ll_ptr succ  = Acc::get( lhl ).next;
        while ( lh && rh ) {
                T* tmp = nullptr;
                if ( comp( *rh, *lh ) ) {
                        tmp = rh;
                        rh  = _node( Acc::get( *rh ).next );
                } else {
                        tmp = lh;
                        if ( lh == &lhl )
                                lh = nullptr;
                        else
                                lh = _node( Acc::get( *lh ).next );
                }
                detach( *tmp );
                if ( !first )
                        first = tmp;
                if ( last )
                        link_detached_as_next( *last, *tmp );
                last = tmp;
        }
        ZLL_ASSERT( first );
        ZLL_ASSERT( last );
        _next_or_first_set< T, Acc >( pred, *first );
        Acc::get( *first ).prev = pred;

        if ( lh ) {
                Acc::get( *last ).next = *lh;
                Acc::get( *lh ).prev   = *last;
                last                   = &lhl;
        } else if ( rh ) {
                Acc::get( *last ).next = *rh;
                Acc::get( *rh ).prev   = *last;
                last                   = &rhl;

                Acc::get( *last ).next = succ;
                _prev_or_last_set< T, Acc >( succ, *last );
        }

        return { first, last };
}

/// Remove all nodes in the range [first, last] for which `p` returns true. Returns the number of
/// removed nodes.
template < typename T, typename Acc, typename Pred >
requires( _provides_ll_header< T, Acc > )
std::size_t range_remove( T& first, T& last, Pred&& p ) noexcept(
    _nothrow_access< Acc, T > && noexcept( p( first ) ) )
{
        T*          n     = &first;
        std::size_t count = 0;

        for ( ;; ) {
                T* tmp = _node( Acc::get( *n ).next );
                if ( p( *n ) ) {
                        detach( *n );
                        ++count;
                }
                if ( n == &last )
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
        T* n = &last;
        while ( n != &first ) {
                T* p = _node( Acc::get( last ).prev );
                detach( *p );
                link_detached_as_next( *n, *p );
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
        std::size_t count = 0;

        for ( T* m = &first; m != &last; ) {
                T* n = _node( Acc::get( *m ).next );
                if ( !n )
                        break;
                if ( p( *m, *n ) ) {
                        detach( *n );
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
        T& pivot     = first;
        T* n         = _node( Acc::get( first ).next );
        T* new_first = nullptr;
        for ( ;; ) {
                T* next = _node( Acc::get( *n ).next );
                if ( cmp( *n, pivot ) ) {
                        detach( *n );
                        link_detached_as_prev( pivot, *n );
                        if ( !new_first )
                                new_first = n;
                }
                if ( n == &last )
                        break;
                n = next;
        }
        if ( _node( Acc::get( pivot ).prev ) != &last )
                range_qsort< T, Acc >( *_node( Acc::get( pivot ).next ), last, cmp );
        if ( new_first )
                range_qsort< T, Acc >( *new_first, *_node( Acc::get( pivot ).prev ), cmp );
}

/// Standard linked list iterator, needs just pointer to node
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
          : _n( n )
        {
        }

        reference operator*() const noexcept
        {
                ZLL_ASSERT( _n );
                return *_n;
        }

        pointer operator->() const noexcept
        {
                ZLL_ASSERT( _n );
                return _n;
        }

        ll_iterator& operator++() noexcept
        {
                _n = _n ? _node( Acc::get( *_n ).next ) : nullptr;
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
                return _n == other._n;
        }

        T* get() const noexcept
        {
                return _n;
        }

private:
        T* _n = nullptr;
};

/// Standard linked list const-iterator, needs just pointer to node
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
          : _n( n )
        {
        }

        ll_const_iterator( ll_iterator< T, Acc > const& it ) noexcept
          : _n( it.get() )
        {
        }

        reference operator*() const noexcept
        {
                ZLL_ASSERT( _n );
                return *_n;
        }

        pointer operator->() const noexcept
        {
                ZLL_ASSERT( _n );
                return _n;
        }

        ll_const_iterator& operator++() noexcept
        {
                _n = _n ? _node( Acc::get( *_n ).next ) : nullptr;
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
                return _n == other._n;
        }

private:
        T const* _n = nullptr;
};

/// Non-owning linked list container, expects nodes to contain ll_header as member.
/// The nodes are linked together in a doubly linked list, with the first and last nodes
/// accessible through the `front()` and `back()` methods.
///
/// Type `T` is the type of the node that contains the header.
/// Type `Acc` specifies how to access the node's header.
template < typename T, typename Acc >
struct ll_list
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
                        ZLL_ASSERT( detached( *n ) );
                        link_back( *n );
                }
        }

        /// Copy is not allowed
        ll_list( ll_list const& )            = delete;
        ll_list& operator=( ll_list const& ) = delete;

        /// Move constructor. Moved-from list is empty after move.
        ll_list( ll_list&& other ) noexcept( noexcept_access )
        {
                *this = std::move( other );
        }

        /// Move assignment operator. Moved-from list is empty after move.
        ll_list& operator=( ll_list&& other ) noexcept( noexcept_access )
        {
                if ( this == &other )
                        return *this;

                detach_nodes();
                _first = other._first;
                if ( _first ) {
                        other._first             = nullptr;
                        Acc::get( *_first ).prev = *this;
                }
                _last = other._last;
                if ( _last ) {
                        other._last             = nullptr;
                        Acc::get( *_last ).next = *this;
                }

                return *this;
        }

        T& front() noexcept
        {
                return *_first;
        }

        T& back() noexcept
        {
                return *_last;
        }

        T const& front() const noexcept
        {
                return *_first;
        }

        T const& back() const noexcept
        {
                return *_last;
        }

        iterator begin() noexcept
        {
                return iterator{ _first };
        }

        const_iterator begin() const noexcept
        {
                return const_iterator{ _first };
        }

        const_iterator cbegin() const noexcept
        {
                return const_iterator{ _first };
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
            noexcept_access && noexcept( comp( *_first, *_last ) ) )
        {
                if ( this == &other || other.empty() )
                        return;
                if ( empty() ) {
                        *this = std::move( other );
                        return;
                }

                std::tie( _first, _last ) =
                    merge_ranges< T, Acc >( *_first, *_last, *other._first, *other._last, comp );

                Acc::get( *_first ).prev = *this;
                Acc::get( *_last ).next  = *this;
                other._first             = nullptr;
                other._last              = nullptr;
        }

        /// Removes all nodes compared equal to value `value` from the list. Returns the number of
        /// removed nodes.
        std::size_t
        remove( T const& value ) noexcept( noexcept_access && noexcept( *_first == *_last ) )
        {
                if ( empty() )
                        return 0;
                return range_remove< T, Acc >( *_first, *_last, [&value]( T& n ) noexcept {
                        return n == value;
                } );
        }

        /// Removes all nodes for which `p` returns true from the list. Returns the number of
        /// removed nodes.
        template < typename Pred >
        std::size_t remove_if( Pred&& p ) noexcept( noexcept_access && noexcept( p( *_first ) ) )
        {
                if ( empty() )
                        return 0;
                return range_remove< T, Acc >( *_first, *_last, std::forward< Pred >( p ) );
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
                        auto* f = other._first;
                        auto* l = other._last;
                        other.detach_nodes();
                        link_range_as_next( *_last, *f, *l );
                } else {
                        auto* f = other._first;
                        auto* l = other._last;
                        other.detach_nodes();
                        link_range_as_prev( *pos, *f, *l );
                }
        }

        /// Reverses the order of nodes in the list. The first node becomes the last and the last
        /// node becomes the first.
        void reverse() noexcept( noexcept_access )
        {
                if ( empty() )
                        return;
                range_reverse< T, Acc >( *_first, *_last );
        }

        /// Removes all consecutive nodes in the list for which `p` returns true. Only first
        /// element / in each group of equal elements is left. Returns the number of removed nodes.
        template < typename BinPred >
        std::size_t
        unique( BinPred p ) noexcept( noexcept_access && noexcept( p( *_first, *_last ) ) )
        {
                if ( empty() )
                        return 0;
                return range_unique< T, Acc >( *_first, *_last, std::move( p ) );
        }

        /// Removes all consecutive nodes in the list for which `std::equal_to<>` returns true.
        /// Only first element in each group of equal elements is left. Returns the number of
        /// removed nodes.
        std::size_t
        unique() noexcept( noexcept_access && noexcept( std::equal_to<>{}( *_first, *_last ) ) )
        {
                return unique( std::equal_to<>{} );
        }

        /// Sorts the nodes in the list. The `cmp` is used to compare two nodes.
        template < typename Compare >
        void sort( Compare&& cmp ) noexcept( noexcept_access && noexcept( cmp( *_first, *_last ) ) )
        {
                if ( empty() )
                        return;
                range_qsort< T, Acc >( *_first, *_last, std::forward< Compare >( cmp ) );
        }

        /// Sorts the nodes in the list. Uses `std::less<>` for comparison.
        void sort() noexcept( noexcept_access && noexcept( std::less<>{}( *_first, *_last ) ) )
        {
                sort( std::less<>{} );
        }

        /// Links the node `node` as the first element of the list. The previous first element
        /// becomes the second element. Detaches `node` from any other list it might be
        /// attached to.
        void link_front( T& node ) noexcept( noexcept_access )
        {
                detach< T, Acc >( node );
                if ( _first )
                        link_detached_as_prev< T, Acc >( *_first, node );
                else
                        link_first( node );
        }

        /// Detaches the first element of the list. The second element becomes the first element.
        /// Undefined behavior if the list is empty.
        void detach_front() noexcept( noexcept_access )
        {
                detach< T, Acc >( *_first );
        }

        /// Returns true if the list is empty, i.e. contains no elements.
        bool empty() const noexcept
        {
                return !_first;
        }

        /// Links the node `node` as the last element of the list. The previous last element
        /// becomes the second last element. Detaches `node` from any other list it might
        /// be attached to.
        void link_back( T& node ) noexcept( noexcept_access )
        {
                detach< T, Acc >( node );
                if ( _last )
                        link_detached_as_next< T, Acc >( *_last, node );
                else
                        link_first( node );
        }

        /// Detaches the last element of the list. The second last element becomes the last
        /// element. Undefined behavior if the list is empty.
        void detach_back() noexcept( noexcept_access )
        {
                detach< T, Acc >( *_last );
        }

        /// Detaches all nodes in the list. The list becomes empty after this operation.
        /// The nodes themselves are still linked together, but not to this list.
        ~ll_list() noexcept( noexcept_access )
        {
                detach_nodes();
        }

private:
        template < typename U, typename Bcc >
        friend void _prev_or_last_set( _ll_ptr< U, Bcc > p, _ll_ptr< U, Bcc > n );
        template < typename U, typename Bcc >
        friend void _next_or_first_set( _ll_ptr< U, Bcc > p, _ll_ptr< U, Bcc > n );

        void detach_nodes() noexcept( noexcept_access )
        {
                if ( _first )
                        Acc::get( *_first ).prev = nullptr;
                if ( _last )
                        Acc::get( *_last ).next = nullptr;
                _first = nullptr;
                _last  = nullptr;
        }

        void link_first( T& node ) noexcept( noexcept_access )
        {
                _first = &node;
                _last  = &node;

                Acc::get( node ).next = *this;
                Acc::get( node ).prev = *this;
        }

        T* _first = nullptr;
        T* _last  = nullptr;
};

/// CRTP base class for linked list nodes containing `ll_header`. Provides access type to the header
/// of the node and implements move and copy semantics for the node. Provides basic API for the
/// node.
///
/// Note that for copy construction to work it has to use non-const reference to the node. This is
/// so we can re-link the copied node into the list.
template < typename Derived >
struct ll_base
{

        /// Access type to the header of ll_base.
        struct access
        {
                static auto& get( Derived& d ) noexcept
                {
                        return static_cast< ll_base* >( &d )->_hdr;
                }

                static auto& get( Derived const& d ) noexcept
                {
                        return static_cast< ll_base const* >( &d )->_hdr;
                }
        };

        /// Default constructor node is detached
        ll_base() noexcept = default;

        /// Move constructor, moved-from node is detached. The new node is linked to the
        /// list of the moved-from node instead of it.
        ll_base( ll_base&& o ) noexcept
        {
                move_from_to< Derived, access >( o.derived(), derived() );
        }

        /// Copy constructor, copied node is linked to the list of the copied node after it.
        ll_base( ll_base& o ) noexcept
        {
                link_detached_as_next< Derived, access >( o.derived(), derived() );
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
                return _node( _hdr.next );
        }

        Derived const* next() const
        {
                return _node( _hdr.next );
        }

        Derived* prev()
        {
                return _node( _hdr.prev );
        }

        Derived const* prev() const
        {
                return _node( _hdr.prev );
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
        ll_header< Derived, access > _hdr;
};

/// Iterate over all nodes in the list starting from `n` and call `f` for each node.
/// The order of the nodes is: predecessors, `n`, successors.
template < typename T, typename Acc = typename T::access >
requires( _provides_ll_header< T, Acc > )
void for_each_node( T& n, std::invocable< T& > auto&& f ) noexcept(
    _nothrow_access< Acc, T > && noexcept( f( n ) ) )
{
        auto& h = Acc::get( n );
        for ( T* m = _node( h.prev ); m; m = _node( Acc::get( *m ).prev ) )
                f( *m );
        f( n );
        for ( T* m = _node( h.next ); m; m = _node( Acc::get( *m ).next ) )
                f( *m );
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
        for ( T* m = _node( h.prev ); m; m = _node( Acc::get( *m ).prev ) )
                if ( f( *m ) )
                        return m;
        if ( f( n ) )
                return &n;
        for ( T* m = _node( h.next ); m; m = _node( Acc::get( *m ).next ) )
                if ( f( *m ) )
                        return m;
        return nullptr;
}

template < typename T, typename Acc = typename T::access >
struct sh_header;

template < typename T, typename Acc = typename T::access >
struct sh_heap;

template < typename T, typename Acc = typename T::access >
concept _provides_sh_header = requires( T t ) {
        {
                Acc::get( t )
        } -> std::convertible_to< sh_header< std::remove_const_t< T >, Acc > const& >;
};

template < typename T, typename Acc = typename T::access >
using _sh_ptr = _vptr< T, sh_heap< T, Acc > >;

template < typename T, typename Acc = typename T::access >
auto* _node( _sh_ptr< T, Acc > p )
{
        return p.a();
}

template < typename T, typename Acc = typename T::access >
auto* _heap( _sh_ptr< T, Acc > p )
{
        return p.b();
}

template < typename T, typename Acc = typename T::access >
T& _detach_right( T& node ) noexcept
{
        T& tmp                 = *Acc::get( node ).right;
        Acc::get( tmp ).parent = nullptr;
        Acc::get( node ).right = nullptr;
        return tmp;
}

template < typename T, typename Acc = typename T::access >
T& _detach_left( T& node ) noexcept
{
        T& tmp                 = *Acc::get( node ).left;
        Acc::get( tmp ).parent = nullptr;
        Acc::get( node ).left  = nullptr;
        return tmp;
}

template < typename T, typename Acc = typename T::access >
T& _detach_first( sh_heap< T >& parent ) noexcept
{
        T& tmp                 = *parent.first;
        Acc::get( tmp ).parent = nullptr;
        parent.first           = nullptr;
        return tmp;
}

template < typename T, typename Acc = typename T::access >
_sh_ptr< T, Acc > _detach_parent( T& node ) noexcept
{
        _sh_ptr< T, Acc > tmp = Acc::get( node ).parent;

        if ( auto* n = _node( tmp ) ) {
                if ( Acc::get( *n ).left == &node )
                        Acc::get( *n ).left = nullptr;
                else if ( Acc::get( *n ).right == &node )
                        Acc::get( *n ).right = nullptr;
        } else if ( auto* h = _heap( tmp ) ) {
                h->first = nullptr;
        }
        Acc::get( node ).parent = nullptr;
        return tmp;
}

/// Returns true if the node is detached from heap.
template < typename T, typename Acc = typename T::access >
requires( _provides_sh_header< T, Acc > )
bool detached( T& node ) noexcept( _nothrow_access< Acc, T > )
{
        auto& n_hdr = Acc::get( node );
        return !n_hdr.left && !n_hdr.right && !n_hdr.parent;
}

template < typename T, typename Acc = typename T::access >
void _attach_right( T& parent, T& node ) noexcept
{
        Acc::get( parent ).right = &node;
        Acc::get( node ).parent  = parent;
}

template < typename T, typename Acc = typename T::access >
void _attach_left( T& parent, T& node ) noexcept
{
        Acc::get( parent ).left = &node;
        Acc::get( node ).parent = parent;
}

template < typename T, typename Acc = typename T::access >
void _attach_first( sh_heap< T, Acc >& parent, T& node ) noexcept
{
        parent.first            = &node;
        Acc::get( node ).parent = parent;
}

template < typename T, typename Acc = typename T::access >
void _attach_parent( T& node, _sh_ptr< T, Acc > p ) noexcept
{
        Acc::get( node ).parent = p;
        if ( auto* n = _node( p ) ) {
                if ( !Acc::get( *n ).left )
                        Acc::get( *n ).left = &node;
                else if ( !Acc::get( *n ).right )
                        Acc::get( *n ).right = &node;
        } else if ( auto* h = _heap( p ) ) {
                h->first = &node;
        }
}

template < typename T, typename Acc, typename Compare >
T& _sh_merge( T& left, T& right, Compare&& comp ) noexcept;

template < typename T, typename Acc, typename Compare >
T& _sh_merge_impl( T& left, T& right, Compare&& comp ) noexcept
{
        T* left_left = nullptr;
        if ( Acc::get( left ).left )
                left_left = &_detach_left( left );
        if ( Acc::get( left ).right ) {
                auto& left_right = _detach_right( left );
                auto& new_left   = _sh_merge< T, Acc >( left_right, right, comp );
                _attach_left( left, new_left );
        } else {
                _attach_left( left, right );
        }
        if ( left_left )
                _attach_right( left, *left_left );

        return left;
}

template < typename T, typename Acc, typename Compare >
T& _sh_merge( T& left, T& right, Compare&& comp ) noexcept
{
        ZLL_ASSERT( !Acc::get( left ).parent );
        ZLL_ASSERT( !Acc::get( right ).parent );

        if ( comp( left, right ) )
                return _sh_merge_impl< T, Acc >( right, left, comp );
        else
                return _sh_merge_impl< T, Acc >( left, right, comp );
}

template < typename T, typename Acc = typename T::access >
void _replace_in_parent( T& node, T& new_node ) noexcept
{
        _sh_ptr< T, Acc >& parent = Acc::get( node ).parent;

        if ( auto* n = _node( parent ) ) {
                if ( Acc::get( *n ).left == &node )
                        _attach_left( *n, new_node );
                else if ( Acc::get( *n ).right == &node )
                        _attach_right( *n, new_node );
        } else if ( auto* h = _heap( parent ) ) {
                _attach_first( *h, new_node );
        }

        parent = nullptr;
}

template < typename T, typename Acc = typename T::access >
requires( _provides_sh_header< T, Acc > )
void move_from_to( T& from, T& to ) noexcept
{
        ZLL_ASSERT( detached( to ) );

        if ( Acc::get( from ).left ) {
                auto& l = _detach_left( from );
                _attach_left( to, l );
        }
        if ( Acc::get( from ).right ) {
                auto& r = _detach_right( from );
                _attach_right( to, r );
        }
        if ( Acc::get( from ).parent )
                _replace_in_parent( from, to );
}

template < typename T, typename Acc = typename T::access >
requires( _provides_sh_header< T, Acc > )
void link_detached_copy_of( T& node, T& cpy ) noexcept
{
        ZLL_ASSERT( detached( cpy ) );

        T* n = nullptr;
        if ( Acc::get( node ).right ) {
                auto& r = _detach_right( node );
                n       = &_sh_merge< T, Acc >( r, cpy, std::less<>{} );
        } else {
                n = &cpy;
        }
        ZLL_ASSERT( n );
        _attach_right( node, *n );
}

template < typename T, typename Acc = typename T::access >
requires( _provides_sh_header< T, Acc > )
void detach( T& node ) noexcept( _nothrow_access< Acc, T > )
{
        T* n = nullptr;
        if ( Acc::get( node ).left && Acc::get( node ).right ) {
                auto& l = _detach_left( node );
                auto& r = _detach_right( node );
                n       = &_sh_merge< T, Acc >( l, r, std::less<>{} );
        } else if ( Acc::get( node ).left ) {
                n = &_detach_left( node );
        } else if ( Acc::get( node ).right ) {
                n = &_detach_right( node );
        }
        if ( n )
                _replace_in_parent( node, *n );
}

template < typename T, typename Acc = typename T::access >
requires( _provides_sh_header< T, Acc > )
void for_each_node( T& n, std::invocable< T& > auto&& f )
{
        auto& h = Acc::get( n );
        if ( h.left )
                for_each_node< T, Acc >( *h.left, f );
        f( n );
        if ( h.right )
                for_each_node< T, Acc >( *h.right, f );
}

template < typename T, typename Acc = typename T::access >
requires( _provides_sh_header< T, Acc > )
void link_detached( T& n1, T& n2 ) noexcept
{
        ZLL_ASSERT( detached( n2 ) );

        auto p = _detach_parent( n1 );

        auto& n = _sh_merge< T, Acc >( n1, n2, std::less<>{} );
        _attach_parent( n, p );
}

template < typename T, typename Acc >
struct sh_header
{
        T*                left   = nullptr;
        T*                right  = nullptr;
        _sh_ptr< T, Acc > parent = nullptr;

        sh_header() noexcept                         = default;
        sh_header( sh_header const& )                = delete;
        sh_header( sh_header&& ) noexcept            = delete;
        sh_header& operator=( sh_header const& )     = delete;
        sh_header& operator=( sh_header&& ) noexcept = delete;
};

template < typename Derived >
struct sh_base
{
        struct access
        {
                static auto& get( Derived& d ) noexcept
                {
                        return static_cast< sh_base* >( &d )->_hdr;
                }

                static auto& get( Derived const& d ) noexcept
                {
                        return static_cast< sh_base const* >( &d )->_hdr;
                }
        };

        sh_base() noexcept = default;

        sh_base( sh_base&& o ) noexcept
        {
                move_from_to< Derived, access >( o.derived(), derived() );
        }

        ~sh_base()
        {
                detach< Derived, access >( derived() );
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
        sh_header< Derived, access > _hdr;
};

template < typename T, typename Acc >
struct sh_heap
{
        T* first = nullptr;

        sh_heap() noexcept                       = default;
        sh_heap( sh_heap const& )                = delete;
        sh_heap( sh_heap&& ) noexcept            = delete;
        sh_heap& operator=( sh_heap const& )     = delete;
        sh_heap& operator=( sh_heap&& ) noexcept = delete;

        static constexpr bool noexcept_access = _nothrow_access< Acc, T >;

        ~sh_heap() noexcept( noexcept_access )
        {
                if ( first )
                        _detach_first( *this );
        }

        void link( T& node ) noexcept( noexcept_access )
        {
                T* n = nullptr;
                if ( first ) {
                        auto& f = _detach_first( *this );
                        n       = &_sh_merge< T, Acc >( f, node, std::less<>{} );
                } else {
                        n = &node;
                }
                _attach_first( *this, *n );
        }

        bool empty() const noexcept
        {
                return !first;
        }

        T& top() noexcept
        {
                ZLL_ASSERT( first );
                return *first;
        }

        void pop() noexcept( noexcept_access )
        {
                ZLL_ASSERT( first );
                _detach_first( *this );
        }

        T& take() noexcept( noexcept_access )
        {
                ZLL_ASSERT( first );
                T* n = _detach_first( *this );
                return *n;
        }
};

}  // namespace zll