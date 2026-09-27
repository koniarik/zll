import gdb
import gdb.printing

# ---------------------------------------------------------------------------
# _vptr<A,B> helper
# ---------------------------------------------------------------------------


def _vptr_decode(vptr_val):
    """Return (kind, address) from a zll::_vptr value.

    kind is 'null', 'node', or 'sentinel'.
    address is the raw pointer value (int), meaningful when kind != 'null'.
    """
    ptr = int(vptr_val["ptr"])
    if ptr == 0:
        return ("null", 0)
    if ptr & 1:
        return ("sentinel", ptr & ~1)
    return ("node", ptr)


# ---------------------------------------------------------------------------
# header -> node
# ---------------------------------------------------------------------------


def _header_offset(node_type, acc_type, header):
    """Byte offset of the `header`<node_type, acc_type, ...> inside node_type, as a member or a base."""
    for field in node_type.strip_typedefs().fields():
        ftype = field.type.strip_typedefs()
        if str(ftype).startswith(header + "<") and str(ftype.template_argument(1)) == str(acc_type):
            return field.bitpos // 8
        if field.is_base_class:
            inner = _header_offset(ftype, acc_type, header)
            if inner is not None:
                return field.bitpos // 8 + inner
    return None


def _node_at(hdr_addr, node_type, acc_type, header):
    """The node whose `header` sits at hdr_addr."""
    addr = hdr_addr - _header_offset(node_type, acc_type, header)
    return gdb.Value(addr).cast(node_type.pointer()).dereference()


# ---------------------------------------------------------------------------
# ll_header<T,Acc>
# ---------------------------------------------------------------------------


def _ll_link_child(link_val, hdr_type):
    """The neighbour a link of an ll_header<T, Acc> points to: the node, the list, or 'null'."""
    kind, addr = _vptr_decode(link_val)
    if kind == "null":
        return "null"
    node_type = hdr_type.template_argument(0)
    acc_type = hdr_type.template_argument(1)
    if kind == "sentinel":
        list_type = gdb.lookup_type("zll::ll_list<{}, {}>".format(node_type, acc_type))
        return gdb.Value(addr).cast(list_type.pointer()).dereference()
    return _node_at(addr, node_type, acc_type, "zll::ll_header")


class LlHeaderPrinter(gdb.ValuePrinter):
    """Print a zll::ll_header as {prev, next}."""

    def __init__(self, val):
        self.__val = val

    def to_string(self):
        return None

    def children(self):
        hdr_type = self.__val.type.unqualified().strip_typedefs()
        yield ("prev", _ll_link_child(self.__val["_prev"], hdr_type))
        yield ("next", _ll_link_child(self.__val["_next"], hdr_type))


# ---------------------------------------------------------------------------
# _vptr<A,B>
# ---------------------------------------------------------------------------


class VptrPrinter(gdb.ValuePrinter):
    """Print a zll::_vptr."""

    def __init__(self, val):
        self.__val = val

    def to_string(self):
        kind, addr = _vptr_decode(self.__val)
        if kind == "null":
            return "null"
        if kind == "node":
            return "node @ 0x{:x}".format(addr)
        return "sentinel @ 0x{:x}".format(addr)


# ---------------------------------------------------------------------------
# ll_list<T,Acc>  — lazy iterator
# ---------------------------------------------------------------------------


class _LlListIterator:
    """Walks the headers from `first`, yielding the node of each."""

    def __init__(self, first_hdr, node_type, acc_type):
        self.__addr = int(first_hdr)
        self.__node_type = node_type
        self.__acc_type = acc_type
        self.__hdr_type = gdb.lookup_type("zll::_ll_hdr")
        self.__idx = 0

    def __iter__(self):
        return self

    def __next__(self):
        if self.__addr == 0:
            raise StopIteration
        hdr = gdb.Value(self.__addr).cast(self.__hdr_type.pointer()).dereference()
        node = _node_at(self.__addr, self.__node_type, self.__acc_type, "zll::ll_header")
        label = "[{}]".format(self.__idx)
        self.__idx += 1
        next_kind, next_addr = _vptr_decode(hdr["_next"])
        self.__addr = next_addr if next_kind == "node" else 0
        return (label, node)


class LlListPrinter(gdb.ValuePrinter):
    """Print a zll::ll_list."""

    def __init__(self, val):
        self.__val = val

    def display_hint(self):
        return "array"

    def to_string(self):
        if int(self.__val["first"]) == 0:
            return "{}"
        return None

    def children(self):
        first_hdr = self.__val["first"]
        if int(first_hdr) == 0:
            return
        list_type = self.__val.type.strip_typedefs()
        yield from _LlListIterator(
            first_hdr, list_type.template_argument(0), list_type.template_argument(1)
        )


# ---------------------------------------------------------------------------
# sh_header<T,Acc,Compare>
# ---------------------------------------------------------------------------


class ShHeaderPrinter(gdb.ValuePrinter):
    """Print a zll::sh_header."""

    def __init__(self, val):
        self.__val = val

    def to_string(self):
        return None

    def children(self):
        hdr_type = self.__val.type.unqualified().strip_typedefs()
        node_type = hdr_type.template_argument(0)
        acc_type = hdr_type.template_argument(1)
        parent_kind, parent_addr = _vptr_decode(self.__val["_parent"])
        if parent_kind == "sentinel":
            heap_type = gdb.lookup_type(
                "zll::sh_heap<{}, {}, {}>".format(node_type, acc_type, hdr_type.template_argument(2))
            )
            yield ("parent", gdb.Value(parent_addr).cast(heap_type.pointer()).dereference())
        elif parent_kind == "node":
            yield ("parent", _node_at(parent_addr, node_type, acc_type, "zll::sh_header"))
        else:
            yield ("parent", "null")
        for side in ("left", "right"):
            addr = int(self.__val["_" + side])
            if addr:
                yield (side, _node_at(addr, node_type, acc_type, "zll::sh_header"))
            else:
                yield (side, "null")


# ---------------------------------------------------------------------------
# sh_heap<T,Acc,Compare>  — in-order DFS, depth limited by print max-depth
# ---------------------------------------------------------------------------


class _ShHeapIterator:
    def __init__(self, root, node_type, acc_type):
        self.__node_type = node_type
        self.__acc_type = acc_type
        self.__hdr_type = gdb.lookup_type("zll::_sh_hdr")
        self.__idx = 0
        # stack holds (header address, state): state 0=push left, 1=yield self, 2=push right
        self.__stack = []
        if int(root) != 0:
            self.__stack.append((int(root), 0))

    def __iter__(self):
        return self

    def __next__(self):
        max_depth = gdb.parameter("print max-depth")
        if max_depth is not None and max_depth != -1 and self.__idx >= max_depth:
            raise StopIteration

        while self.__stack:
            addr, state = self.__stack[-1]
            hdr = gdb.Value(addr).cast(self.__hdr_type.pointer()).dereference()

            if state == 0:
                self.__stack[-1] = (addr, 1)
                left = int(hdr["_left"])
                if left != 0:
                    self.__stack.append((left, 0))
                continue
            elif state == 1:
                self.__stack[-1] = (addr, 2)
                label = "[{}]".format(self.__idx)
                self.__idx += 1
                return (label, _node_at(addr, self.__node_type, self.__acc_type, "zll::sh_header"))
            else:
                self.__stack.pop()
                right = int(hdr["_right"])
                if right != 0:
                    self.__stack.append((right, 0))
                continue

        raise StopIteration


class ShHeapPrinter(gdb.ValuePrinter):
    """Print a zll::sh_heap."""

    def __init__(self, val):
        self.__val = val

    def display_hint(self):
        return "array"

    def to_string(self):
        if int(self.__val["root"]) == 0:
            return "{}"
        return None

    def children(self):
        root = self.__val["root"]
        if int(root) == 0:
            return
        heap_type = self.__val.type.strip_typedefs()
        yield from _ShHeapIterator(
            root, heap_type.template_argument(0), heap_type.template_argument(1)
        )


# ---------------------------------------------------------------------------
# Registration
# ---------------------------------------------------------------------------


def build_pretty_printer():
    pp = gdb.printing.RegexpCollectionPrettyPrinter("zll")
    pp.add_printer("ll_list",   r"^zll::ll_list<",   LlListPrinter)
    pp.add_printer("ll_header", r"^zll::ll_header<",  LlHeaderPrinter)
    pp.add_printer("sh_heap",   r"^zll::sh_heap<",    ShHeapPrinter)
    pp.add_printer("sh_header", r"^zll::sh_header<",  ShHeaderPrinter)
    pp.add_printer("_vptr",     r"^zll::_vptr<",      VptrPrinter)
    return pp


gdb.printing.register_pretty_printer(
    gdb.current_objfile(),
    build_pretty_printer(),
    replace=True,
)
