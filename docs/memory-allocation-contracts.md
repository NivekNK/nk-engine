# Memory allocation contracts

Status: active architecture contract

This document records the ownership and failure rules for NK Engine memory
primitives. It complements the renderer/resource lifetime contract and must be
updated when an allocator changes how it owns storage, tracks allocations or
depends on another memory component.

## Layering and bootstrap

`Allocator` is the common storage interface. An allocator may be tracked by
`MemorySystem` or explicitly untracked, but clients depend on the injected
allocator instance rather than locating `MemorySystem` globally. This keeps
low-level memory primitives usable during bootstrap and prevents a circular
dependency between allocator metadata and the tracker.

The owner of an injected allocator must outlive every allocation made through
it. Tracking observes the allocator operations; it does not acquire ownership
of either the allocator or its storage.

## FreeList ownership

`nk::mem::FreeList` manages offsets in a logical contiguous address space. It
does not allocate, own or expose the backing bytes represented by those offsets.
The caller owns that backing store and is responsible for pairing each reserved
range with the memory operation that uses it.

Initialization receives both a metadata allocator and an explicit number of
range entries. It performs one fixed `arr<MemoryRange>` allocation and the
metadata allocator must outlive the list. `reserve`, `release`, `resize` and
`clear` never allocate. This explicit capacity replaces Kohi's size-derived
metadata formula and avoids a hidden minimum or a magic entry count for small
address spaces.

`shutdown` releases only the metadata array. It never releases the caller's
backing store.

## FreeList invariants

- The managed size is nonzero while initialized. Metadata capacity is at least
  one range and cannot change before shutdown.
- Free ranges are ordered by offset, disjoint and coalesced. Two adjacent free
  ranges never remain as separate entries after a successful release.
- `free_space() + used_space() == total_size()` after every successful or failed
  public operation.
- A successful reservation uses the lowest-offset free range that can both fit
  the aligned request and be represented by the available metadata. Alignment
  must be a nonzero power of two. The optional alignment bias aligns
  `bias + offset`, which lets a client align an absolute address even when its
  backing store is not naturally aligned. Any prefix padding remains free.
- A release rejects zero-sized, overflowing, out-of-bounds and already-free
  ranges. It may merge with the previous range, the next range or both.
- Every operation is transactional. Capacity exhaustion, fragmentation,
  invalid input and blocked shrinking leave all ranges and counters unchanged.
- Growth extends a free tail when one exists; otherwise it needs a spare metadata
  entry. Shrinking succeeds only when the entire removed suffix is free.
- `clear` declares the complete logical address space free. The caller must have
  invalidated every outstanding reservation before calling it.

`free_range(index)` returns a borrowed diagnostic view into the metadata array.
The pointer is invalid after any mutating operation or shutdown and must not be
used as an allocation handle.

## Range identity limitation

`MemoryRange` is a value describing an occupied span, not a stable allocation
identity. The free list can prove that a released span is currently occupied,
but it cannot prove that it exactly matches the reservation that produced it.
It therefore permits partial release of an occupied span. A stale range can
also exhibit an ABA problem if its offsets are later reserved again.

Allocators layered on top of `FreeList` must validate their own allocation
headers or generation metadata before release when exact block identity is a
requirement. They must not weaken the overlap, bounds or transactional checks
provided by the free list.

## FreeListAllocator ownership and lifetime

`nk::mem::FreeListAllocator` layers reusable CPU allocations over a fixed
contiguous backing store. It supports two explicit ownership modes:

- The owning form obtains the backing block from an injected backing allocator
  and returns it during destruction.
- The borrowing form receives an external block and never releases it.

In both forms, a separately injected metadata allocator owns the fixed range
array used by `FreeList`. The backing and metadata allocators, or the borrowed
block, must outlive the `FreeListAllocator`. Self-dependencies are rejected at
initialization. No parent allocation occurs after a successful initialization.

The allocator is movable but not copyable. Move construction transfers backing
ownership, metadata and live allocation state. Move assignment is accepted only
when the destination has no live allocations; its previous empty backing and
metadata are released before the source state is transferred.

## FreeListAllocator allocation contract

- Every allocation consumes the requested payload plus one 8-byte guard header.
  Alignment applies to the returned payload address, including when a borrowed
  backing address is misaligned. Alignment prefixes remain reusable.
- The caller still supplies the exact payload size to `free`, as required by the
  base `Allocator` API. The guard binds backing address, range offset and size,
  so wrong-size, interior, foreign and duplicate frees are rejected even when
  `MemorySystem` tracking is disabled.
- The guard detects API misuse; it is not a security boundary. A stale pointer
  has the same ABA limitation after the identical range and size are allocated
  again.
- Allocation and release are transactional. Overflow, out-of-space,
  fragmentation, metadata exhaustion or invalid release do not change the
  allocator statistics or free ranges.
- `reserved_bytes` is the complete pool size. `used_bytes`, `peak_used_bytes`
  and active allocations describe payloads only. `occupied_space()` includes
  live payload headers, while `free_space()` reports all reusable pool bytes.

This allocator is intended for explicit fixed-lifetime domains and reusable
heaps. It does not replace `MallocAllocator` globally: the current
`MemorySystem` bootstrap and injected allocator topology avoid a circular
dependency, and measurements show that neither implementation wins for every
allocation size. Vulkan buffer suballocation continues to use `FreeList`
directly because those offsets describe GPU buffers rather than CPU pointers.

## Concurrency and complexity

The current implementation is single-threaded and requires external
serialization. It stores fixed metadata and uses linear first-fit scans plus
linear insertion/removal, so operations are `O(number_of_free_ranges)` with no
hidden allocation. This tradeoff is intentional for the initial CPU and Vulkan
suballocation work; a different indexing policy requires measurement against
real engine workloads before replacing it.
