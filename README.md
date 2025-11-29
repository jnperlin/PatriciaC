# PatriciaC
*A compact, efficient, CC0-licensed Patricia (compressed radix-2 trie) implementation in C*

PatriciaC provides a minimal, pointer-based, **mutable** compressed radix-2 trie (Patricia tree) implementation suitable for embedded systems, real-time environments, and any place you need predictable memory access patterns and prefix-based queries.

This implementation uses **dual-use nodes** (internal/external merged), a synthetic root sentinel, and supports **pre-, in-, and post-order iteration** in **both directions**, driven by a compact FSM with bit-indexed traversal.

Designed originally for fast key lookup with piggy-packed keys, it aims to be simple, portable, deterministic, and easy to audit.

The entire library is published under **CC0**, effectively placing it into the public domain.

---

## Features

- **Mutable Patricia (radix-2) tree**  
  Compressed internal nodes, no separate “internal” vs “leaf” structs.

- **Bitwise big-endian key representation**  
  MSB of the first byte = bit 1 (Pascal-style indexing).

- **Dual-use nodes (internal/external unified)**  
  Same struct layout regardless of branching role.

- **Piggy-packed keys**  
  Flexible storage, variable-length byte arrays.

- **Sentinel root**  
  Simplifies traversal and deletion logic.

- **Robust iteration**  
  Pre-order, in-order, post-order; forward and backward; protected by a small parent-FIFO with verified parent recovery.

- **Predictable performance**  
  No recursion; no dynamic allocations during traversal.

- **C99-compatible**, GCC/Clang/MSVC-tested.

- **CC0 license**  
  Free for any purpose, including embedded or proprietary use.

---

## Building

A CMake-based build system is provided primarily for unit testing and CI.  
To build with GCC/Clang/MSVC:

```sh
cmake -B build
cmake --build build
ctest --test-dir build
```

### Compiler Support

The repository tests against:

- GCC ≥ 7  
- Clang ≥ 12  
- MSVC ≥ 2017  

Sanitizers (address/undefined) are automatically enabled when supported by the compiler.

---

## Using PatriciaC in your project

The library is a single logical module. Typical usage is to include the header and compile the C source into your build directly:

```c
#include "patricia.h"

PatriciaMapT map;
ptinit(&map, NULL, NULL);

ptinsert(&map, "test", 4, my_payload_ptr);
void *p = ptlookup(&map, "test", 4);

ptdelete(&map, "test", 4);
ptclear(&map);
```

### Node Layout

```c
typedef struct pt_map_node_ {
    struct pt_map_node_ *_m_child[2];
    uintptr_t            payload;
    uint16_t             _m_bpos;
    uint16_t             nbit;
    char                 data[1];
} PTMapNodeT;
```

Keys are stored in `data[]` with `nbit` bits.  
Bit numbering: MSB of data[0] = bit 1.

---

## Iteration Example

The iterator supports in-order, pre-order, and post-order traversal, forward or backward:

```c
PTMapIterT it;
ptiter_init(&it, &map, map._m_root, true, ePTMode_inOrder);

const PTMapNodeT *n;
while ((n = ptiter_next(&it)) != NULL) {
    printf("key = %.*s\n", (n->nbit + 7) / 8, n->data);
}
```

For reverse iteration:

```c
ptiter_init(&it, &map, map._m_root, false, ePTMode_inOrder);

const PTMapNodeT *n;
while ((n = ptiter_next(&it)) != NULL) {
    ...
}
```

For stepping back:

```c
ptiter_init(&it, &map, map._m_root, true, ePTMode_inOrder);
// ... do some steps forward here
const PTMapNodeT *n;
while ((n = ptiter_prev(&it)) != NULL) {
    ...
}
```
One has to make at least one step forward before stepping back becomes possible: There is no node before the
first one!  But you *can* step back from the end position.

---

## Running the Tests

Tests are built automatically. Use:

```sh
ctest --output-on-failure -V --test-dir build
```

The tests use the **Unity** test framework, automatically fetched at configure time.

Unit tests also serve as **usage examples**, covering:

- Construction & cleanup  
- Insert/lookup/delete  
- Key prefix behavior  
- Iteration (all six traversal variants)  
- Edge cases (empty tree, single node, long keys, etc.)

---

## Directory Layout

```
PatriciaC/
  src/        Implementation -- source and header
  tests/      Unity-based unit tests
  CMakeLists.txt
  README.md
  LICENSE     CC0
```

---

## Status / Roadmap

The core implementation is stable. Planned additions:

- More benchmark cases (random keys, worst-case bit patterns)
- Optional memory-arena usage examples
- Better graphviz/DOT visualization helpers
- Extended documentation on invariants and FSM iteration logic

---

## Contributing

Contributions are welcome under CC0.

Please follow:

- Line width: ~89 columns  
- Prefer explicit logic over “clever” tricks where readability suffers  
- Warnings as errors (`-Wall -Wextra -Wpedantic`, MSVC `/W4`)  
- Keep iteration invariants and FSM logic correctly documented  
- All new code must include tests

---

## License

This project is released under **CC0 1.0 Universal**.  
You may use it freely for any purpose, commercial or private.
