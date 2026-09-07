# Static mesh binary format (`.nkmesh`)

## Scope and ownership

NK's CPU-side static mesh archive stores geometry, inline materials and import
dependencies. It is not a dump of C++ objects: no pointers, allocator state, GLM
padding, Vulkan handles, device addresses or descriptors are persisted.
`mesh_binary::decode(allocator, bytes)` borrows immutable bytes for the call and
returns owning `dyarr` containers through `result`. Failed decoding releases all
partially constructed containers. GPU upload remains a separate renderer task.

This adapts the binary-first/import-on-miss workflow of
[Kohi's mesh loader](https://raw.githubusercontent.com/travisvroman/kohi/920035fa5f8184f36a27107eed0ad81582efa23b/engine/src/resources/loaders/mesh_loader.c),
not its raw-struct `.ksm` representation. The resource-neutral archive preserves
the explicit resource ownership and small Vulkan command layer inspired by
[NoGraphicsAPI](https://github.com/sebbbi/NoGraphicsAPI). It adds no GPU features
or driver requirements.

## Version 1 header

All integers are unsigned, fixed-width, little-endian. Floats are IEEE-754 binary32
serialized by their bits, also little-endian. Header size is exactly 64 bytes.

| Offset | Bytes | Field |
| --- | --- | --- |
| 0 | 8 | Magic `NKMESH\r\n` |
| 8 | 4 | Format version: 1 |
| 12 | 4 | Endianness marker: `0x01020304` |
| 16 | 4 | Header bytes: 64 |
| 20 | 4 | Serialized vertex stride: 48 |
| 24 | 8 | Complete file length |
| 32 | 8 | Deterministic `hash64_bytes` of the entire body |
| 40 | 4 | Geometry count |
| 44 | 4 | Material count |
| 48 | 4 | Dependency count |
| 52 | 4 | Importer revision: 1 |
| 56 | 8 | Reserved, must be zero |

The checksum uses the engine's pinned rapidhash v3 and deterministic seed. It
detects accidental corruption, **not** authenticity. Any change to this hash
contract, serialized vertex/material layout, or importer semantics requires a
format or importer revision bump; unsupported revisions are never guessed.

## Sequential body

A string is `u32 byte_length` followed by exactly those bytes, without a NUL.
Embedded NULs and over-capacity strings are rejected. There are no implicit
alignment gaps or unchecked offsets. Records appear in the following order.

1. Dependencies: `u32 kind`, `u32 present`, `u64 size`, `u64 digest`, path string
   (511 bytes maximum). Kind 0 tracks source content, kind 1 only presence.
   Missing files and presence-only records have zero size/digest. Paths must be
   unique, relative to the asset root, forward-slash-separated and normalized:
   no empty, `.` or `..` components, drive letters, backslashes or control bytes.
2. Materials: name (255), shader name (127), diffuse/specular/normal map names
   (255 each); `u32 type`, `u32 auto_release`; four diffuse-color floats and one
   shininess float. Name is required; only world materials, boolean 0/1 and finite
   values with positive shininess are accepted. Missing maps remain empty.
3. Geometries: required name (255) and material name (255); `u32 vertex_count`,
   `u32 index_count`, `u32 vertex_stride` (48), `u32 index_stride` (4),
   `u64 vertex_and_index_bytes`; center, minimum and maximum (three floats each);
   all vertices, then all indices.
   A vertex consists of position XYZ, normal XYZ, UV and tangent XYZW (12 floats).
   Every index is a `u32` within the vertex count. Triangle counts must be divisible
   by three (indices, or vertices for non-indexed geometry). Positions and center
   must lie inside the finite extents. Trailing bytes are forbidden.

## Defensive limits

- 128 MiB per archive and aggregate recorded source content.
- 4096 geometries, 4096 materials, 512 dependencies.
- 2,097,152 vertices and 8,388,608 indices across the complete mesh.
- At least one nonempty geometry; materials and dependencies may be empty for
  standalone codec callers.

The reader first validates the full header, checksum, counts, sizes, bounded
strings, numeric fields, extents and indices **without allocating the runtime
payload**. A second pass constructs the owning mesh. Reading the file itself is
separately bounded before allocating its byte buffer. Limits are explicit policy,
not values silently inherited from the current C++ ABI.

Tests pin the wire header/field offsets independently of the decoder, round-trip
all current attributes, compare deterministic bytes and cover every truncation
of a triangle, hostile counts/strides/indices/floats with recomputed checksums,
unsupported versions, invalid paths and allocation rollback.
