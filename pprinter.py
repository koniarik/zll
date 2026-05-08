import re

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
# ll_header<T,Acc>
# ---------------------------------------------------------------------------


def _ll_vptr_child(vptr_val):
    """Return (label_suffix, child_value) for one side of an ll_header _vptr.

    child_value is either a gdb.Value (the dereferenced node) or a string
    ('list' / 'null').
    """
    kind, addr = _vptr_decode(vptr_val)
    if kind == "null":
        return "null"
    if kind == "sentinel":
        list_type = vptr_val.type.template_argument(1)
        return gdb.Value(addr).cast(list_type.pointer()).dereference()
    # node — dereference through the stored address
    node_type = vptr_val.type.template_argument(0)
    return gdb.Value(addr).cast(node_type.pointer()).dereference()


class LlHeaderPrinter(gdb.ValuePrinter):
    """Print a zll::ll_header as {prev, next}."""

    def __init__(self, val):
        self.__val = val

    def to_string(self):
        return None

    def children(self):
        prev_child = _ll_vptr_child(self.__val["prev"])
        next_child = _ll_vptr_child(self.__val["next"])
        yield ("prev", prev_child)
        yield ("next", next_child)


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
    def __init__(self, first_ptr, node_type):
        self.__cur = first_ptr
        self.__node_type = node_type
        self.__idx = 0

    def __iter__(self):
        return self

    def __next__(self):
        if int(self.__cur) == 0:
            raise StopIteration
        node = self.__cur.dereference()
        hdr = _find_ll_header(node)
        label = "[{}]".format(self.__idx)
        self.__idx += 1
        if hdr is not None:
            next_kind, next_addr = _vptr_decode(hdr["next"])
            if next_kind == "node":
                self.__cur = gdb.Value(next_addr).cast(self.__node_type.pointer())
            else:
                self.__cur = gdb.Value(0).cast(self.__node_type.pointer())
        else:
            self.__cur = gdb.Value(0).cast(self.__node_type.pointer())
        return (label, node)


def _find_ll_header(node_val):
    """Walk a node's fields looking for the first ll_header member."""
    t = node_val.type.strip_typedefs()
    for field in t.fields():
        ftype = field.type.strip_typedefs()
        if re.match(r"^zll::ll_header<", str(ftype)):
            return node_val[field.name]
    return None


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
        first_ptr = self.__val["first"]
        if int(first_ptr) == 0:
            return
        node_type = self.__val.type.strip_typedefs().template_argument(0)
        yield from _LlListIterator(first_ptr, node_type)


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
        parent_kind, parent_addr = _vptr_decode(self.__val["parent"])
        if parent_kind == "sentinel":
            heap_type = self.__val["parent"].type.template_argument(1)
            yield ("parent", gdb.Value(parent_addr).cast(heap_type.pointer()).dereference())
        elif parent_kind == "node":
            node_type = self.__val.type.template_argument(0)
            yield ("parent", gdb.Value(parent_addr).cast(node_type.pointer()).dereference())
        else:
            yield ("parent", "null")
        left_ptr = int(self.__val["left"])
        if left_ptr:
            yield ("left", self.__val["left"].dereference())
        else:
            yield ("left", "null")
        right_ptr = int(self.__val["right"])
        if right_ptr:
            yield ("right", self.__val["right"].dereference())
        else:
            yield ("right", "null")


# ---------------------------------------------------------------------------
# sh_heap<T,Acc,Compare>  — in-order DFS, depth limited by print max-depth
# ---------------------------------------------------------------------------


class _ShHeapIterator:
    def __init__(self, top_ptr, node_type):
        self.__node_type = node_type
        self.__idx = 0
        # stack holds (node_val, state): state 0=push left, 1=yield self, 2=push right
        self.__stack = []
        if int(top_ptr) != 0:
            self.__stack.append((top_ptr.dereference(), 0))

    def __iter__(self):
        return self

    def __next__(self):
        max_depth = gdb.parameter("print max-depth")
        if max_depth is not None and max_depth != -1 and self.__idx >= max_depth:
            raise StopIteration

        while self.__stack:
            node, state = self.__stack[-1]
            hdr = _find_sh_header(node)
            if hdr is None:
                self.__stack.pop()
                continue

            if state == 0:
                self.__stack[-1] = (node, 1)
                left_ptr = hdr["left"]
                if int(left_ptr) != 0:
                    self.__stack.append((left_ptr.dereference(), 0))
                continue
            elif state == 1:
                self.__stack[-1] = (node, 2)
                label = "[{}]".format(self.__idx)
                self.__idx += 1
                return (label, node)
            else:
                self.__stack.pop()
                right_ptr = hdr["right"]
                if int(right_ptr) != 0:
                    self.__stack.append((right_ptr.dereference(), 0))
                continue

        raise StopIteration


def _find_sh_header(node_val):
    """Walk a node's fields looking for the first sh_header member."""
    t = node_val.type.strip_typedefs()
    for field in t.fields():
        ftype = field.type.strip_typedefs()
        if re.match(r"^zll::sh_header<", str(ftype)):
            return node_val[field.name]
    return None


class ShHeapPrinter(gdb.ValuePrinter):
    """Print a zll::sh_heap."""

    def __init__(self, val):
        self.__val = val

    def display_hint(self):
        return "array"

    def to_string(self):
        if int(self.__val["top"]) == 0:
            return "{}"
        return None

    def children(self):
        top_ptr = self.__val["top"]
        if int(top_ptr) == 0:
            return
        node_type = self.__val.type.strip_typedefs().template_argument(0)
        yield from _ShHeapIterator(top_ptr, node_type)


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
