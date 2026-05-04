# Delta Spec: render

## MODIFIED Requirements

### Requirement: Arena allocator

The module SHALL provide a `Handle`-backed arena allocator in `render/arena.h`:

```c
typedef struct Arena Arena;
int    arena_init(Arena**, size_t initial_size, const MacSyscalls*);
void   arena_destroy(Arena*);
int    arena_ensure(Arena*, size_t bytes_needed);
void*  arena_alloc(Arena*, size_t n_bytes);
void   arena_reset(Arena*);
size_t arena_high_water(const Arena*);
size_t arena_capacity(const Arena*);
```

The arena SHALL:

1. Back itself with a single `Handle` obtained via `MacSyscalls.new_handle`.
2. `HLock` the handle once at init and keep it locked for the arena's entire lifetime.
3. Return 4-byte-aligned pointers from `arena_alloc`.
4. Use `arena_ensure` to grow the handle via `SetHandleSize` BEFORE any allocations in a cycle — growth SHALL NOT happen during `arena_alloc`.
5. Double its size up to 64 KB, then grow by 32 KB increments, capped at 512 KB (`kArenaHardCap`).
6. Preserve state on grow failure: high_water and existing allocations remain valid.
7. Zero its high_water but keep its backing memory on `arena_reset`.
8. Call `DisposeHandle` exactly once on `arena_destroy`.
9. **Own its `MacSyscalls` by value.** `arena_init` SHALL accept `const MacSyscalls* sys` as a parameter and copy `*sys` into a `MacSyscalls` field at init. After `arena_init` returns, the Arena MUST NOT depend on the caller's `MacSyscalls` storage lifetime. The 80-byte vtable copy is acceptable overhead in exchange for eliminating the dangling-pointer bug class CodeQL flagged on the previous `const MacSyscalls*` field design.

#### Scenario: Allocation returns aligned pointer
- GIVEN an arena with capacity > 8 bytes and high_water == 0
- WHEN `arena_alloc(a, 5)` is called
- THEN the returned pointer's address is divisible by 4
- AND `arena_high_water(a)` returns 8 (5 rounded up to 4-byte alignment)

#### Scenario: Grow failure preserves state
- GIVEN `MacSyscalls.set_handle_size` returns failure
- WHEN `arena_ensure(a, 10000)` is called on an arena with capacity 1000 and high_water 500
- THEN `arena_ensure` returns non-zero
- AND `arena_capacity(a) == 1000` and `arena_high_water(a) == 500`

#### Scenario: Reset preserves capacity
- GIVEN an arena with capacity 4096 and high_water 2000
- WHEN `arena_reset(a)` is called
- THEN `arena_high_water(a) == 0` and `arena_capacity(a) == 4096`

#### Scenario: Caller's MacSyscalls storage may be released after init
- GIVEN a stack-local `FakeSyscalls f` and an arena initialized via `arena_init(&a, 4096, (const MacSyscalls*)&f)`
- WHEN the caller's stack frame holding `f` is later released (or `f` is mutated, or the storage is otherwise reused)
- THEN `arena_alloc`, `arena_ensure`, `arena_reset`, and `arena_destroy` SHALL continue to function correctly using the by-value `MacSyscalls` snapshot taken at init
