<div align="center">
ZLL - Z Linked List

---

[Documentation](https://koniarik.github.io/zll/namespacezll.html)

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

voice_synth build_voice(zll::ll_list< comp_iface >& l){
    voice_synth v;
    l.link_back(v);
    return v;
}

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

```cpp
zll::ll_list< comp_iface > status_list;
R2D2 r2d2 = build_R2D2(status_list);

// list is iterable
for(auto& comp : status_list){
    std::cout << comp.get_status() << "\n";
}

// or we can iterate just by having one node
for_each_node(r2d2.chassis.left_leg, [](comp_iface& comp){
    std::cout << comp.get_status() << "\n";
});

// if we move r2d2 it still works
struct millennium_falcon {
    R2D2 r2d2;
}
millennium_falcon falcon{ .r2d2 = std::move(r2d2) };

for(auto& comp : status_list){
    // note that same status list as above is used, the components are still registered in it
    std::cout << comp.get_status() << "\n";
}

{
    // re-link first and last node to new list
    zll::ll_list< comp_iface > another_list = std::move(status_list);
}
// here, first and last node unlinked from the another list - the nodes are list-less.

```

## Usage

Library is provided as single header file `zll.hpp`, just include it
where needed, or use this as any other cmake library. Bare minimum is C++20.
No dependency except for the standard library.

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

## Skew heap

For the sake of all purposes the skew heap is implemented in similar way as the linked list above, the nodes are intrusive, non-owning, movable, and unlink during destruction.
Skep heap provides partial sort of nodes and is usabel for priority queue or similar structures. More about the data structure here: (https://en.wikipedia.org/wiki/Skew_heap)[https://en.wikipedia.org/wiki/Skew_heap].

They key value of the skew heap of binary heap is that skew heap does not require efficient
access to last element to insert new item, at the cost of less optimal structure.

### Timer Example

```cpp
#include "zll.hpp"
#include <iostream>
#include <cstdint>

struct timer_event : zll::sh_base<timer_event> {
    uint64_t deadline;
    const char* name;

    bool operator<(const timer_event& other) const {
        return deadline < other.deadline;  // earliest deadline first
    }
};

int main() {
    zll::sh_heap<timer_event> timers;
    uint64_t now = 0;

    timer_event t1(100, "timeout_1");
    timer_event t2(50, "timeout_2");
    timer_event t3(150, "timeout_3");

    timers.link(t1);
    timers.link(t2);
    timers.link(t3);

    // Process timers
    while (!timers.empty()) {
        auto& next = *timers.top;
        now = next.deadline;
        std::cout << "Fire: " << next.name << " at " << now << "\n";
        timers.take();
    }

    // Demonstrate auto-cancel: timer removes itself when destroyed
    {
        timer_event short_lived(200, "canceled");
        timers.link(short_lived);
        // short_lived auto-unlinks here when going out of scope
    }

    std::cout << "Heap empty: " << timers.empty() << "\n";  // true
    return 0;
}
```

Key benefits:
- Timer objects can live anywhere (stack, member variable, container)
- Automatic cancellation on destruction - no manual cleanup
- Move timer object freely - it stays registered in heap
- No dynamic allocation required for heap structure

## Assert

Library asserts by using custom `ZLL_ASSERT` macro, by default it maps to standard `assert`,
but user can override it before including the header to use custom assert mechanism.
