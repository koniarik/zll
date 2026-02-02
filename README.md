<div align="center">
ZLL - Z Linked List

---

[Documentation](https://koniarik.github.io/zll/)

</div>

Single-file header-only implementation of intrusive double-linked list and skew heap.
The container does not own the data, but rather provides secondary data structure for example
for registration of callbacks.

The linked list pointers are embed into the nodes of the list, easier is using CRTP baseclass:

```cpp
struct node : zll::ll_base< node >{};
```

This gives the node api to link itself to another node:
```cpp
node a, b;

a.link_next(b);
```

Any linked node is movable and unlinks on destruction:
```cpp
node a, b;
a.link_next(b);

{
    // `b` will unlink itself and `c` will get linked after `a`
    node c{std::move(b)}; 
}

// here `a` has no successor as `c` unlinked itself in destructor
```

## Example

Assume we are implementing R2D2 and want to break the system into components:

```cpp
struct R2D2{
    voice_synth voice;

    struct {
        leg left_leg;
        leg right_leg;
        wheel bot_wheel;
        
        arm arm1;
        arm arm2;
    } chassis;

    struct {
        motor spinner;
    } head;
};
```

Let's assume that we want to monitor each component for status, this happens sporadically
and we don't want to polute the main structure too much.

```cpp
enum class status { NOMINAL, FAULTY };
struct comp_iface : zll::ll_base< comp_iface > {
    virtual status get_status() = 0;
};
```

For any component of R2D2, we just have to inherit:

```cpp
struct motor : comp_iface { ... };
struct voice_synth : comp_iface { ... };
struct leg : comp_iface { ... };
```

The key thing: we just made the components something that can be part of linked list, but
is still movable, or can unregister by itself:

```cpp

voice_synth build_voice(zll::ll_list< comp_iface >&);

R2D2 build_R2D2(zll::ll_list< comp_iface >& l){
    return R2D2{
        .voice = build_voice(l),
        ...
    };
}

```

Hence once for example `voice_synth` is built, it can be just
moved out of the build_voice function, the R2D2 can be built 
by itself in similar way, and during any move or destruction
the components will stay registered in the linkd list itself.

##  How it works

Any member of the linked list has to contain `ll_header` which
looks similar to this:

```cpp
template< typename T, typename Acc >
struct ll_header {
    _ll_ptr< T, Acc > next;
    _ll_ptr< T, Acc > prev;

    ~ll_header();
};
```

where:
 - `T` is the base type of the nodes that is being used
-  `Acc` is utility type used to access the header within a `T`
 - `_ll_ptr` is pointer that points either to `T` or `ll_list<T,A>`
 - `~ll_header()` automatically unlinks next/prev pointers if their link to something.

To use the header:
```cpp
struct node {
    struct access {
        static auto& get(node& n) {
            return n.hdr;
        }
    };

    ll_header< node, access > hdr;
};
```

For the library to work, given `node` it needs a way of accessing the header stored inside.
The `accessor` is customization point to provide ability to extract the header, anytime
the library wants to access the header of node it uses `auto& node_header = Acc::get(my_node);`.

This give the user ultimate flexibility to store the header anywhere in the type or to 
have multiple headers at once (each usable for different linked list).

`ll_base` is jut convenience base class that contains the `ll_header` and provides
accessor for it. 

`ll_header` needs capability to point to the list structure itself in case
last or first item of the list is being operated on - these are pointed-to
by the list, so the node needs the pointer to list to unlink itself.

## 
