# Renderer and resource lifetime contracts

Status: active architecture contract

This document describes the ownership and lifetime rules implemented by the
current NK Engine resource and rendering systems. It is intentionally limited
to behaviour that exists in the codebase. Later renderer chapters must update
this contract in the same commit that changes one of these relationships.

## Design boundary

The public systems expose renderer-neutral resources. Vulkan owns the native
objects behind those resources and must not leak Vulkan types into
`ResourceSystem`, `TextureSystem`, `MaterialSystem`, `GeometrySystem` or their
public configuration types.

`Renderer` is the boundary between those systems and the active backend. A
public resource can carry an opaque backend handle, but only its creating
renderer may interpret or destroy it.

Generated API documentation is not committed. Public comments should explain
behaviour that cannot be expressed by the type system; this document is the
authority for cross-system ownership and shutdown order.

## Construction and destruction order

`Engine` owns the root objects and constructs them in dependency order. The root
allocator provides their storage:

```text
Engine
├── MallocAllocator
├── App
├── ResourceSystem
├── Platform
├── Renderer ───────→ Platform, ResourceSystem
├── TextureSystem ──→ Renderer, ResourceSystem
├── MaterialSystem ─→ Renderer, TextureSystem, ResourceSystem
├── GeometrySystem ─→ Renderer, MaterialSystem
└── frame-local RenderPacket views
```

Arrows show borrowed lifetime dependencies, not memory ownership. All systems
are allocated by the root allocator. `Renderer` additionally owns an internal
allocator used by its backend implementation.

Shutdown is the exact reverse:

```text
GeometrySystem
→ MaterialSystem
→ TextureSystem
→ Renderer
→ ResourceSystem
→ Platform
→ App
→ MallocAllocator
```

These orderings are invariants:

- The root allocator outlives every object allocated from it.
- `ResourceSystem` outlives every system that may load an asset.
- `Platform` outlives `Renderer`, because the Vulkan surface depends on the
  platform window and display connection.
- `Renderer` outlives every system that asks it to destroy GPU resources.
- `TextureSystem` outlives every material that borrows or retains a texture.
- `MaterialSystem` outlives every geometry that borrows or retains a material.
- A failed partial initialization uses the same reverse order and every
  `shutdown` remains safe to call more than once.

## Ownership matrix

| Object | Owner | Borrowed dependencies | Destruction authority |
|---|---|---|---|
| `ResourceSystem` | `Engine` through the root allocator | Registered custom loaders | `ResourceSystem::destroy` with the same allocator |
| `Resource` payload | Its selected `ResourceLoader`, allocated from the ResourceSystem allocator | Owning `ResourceSystem` and loader | `ResourceSystem::unload` delegates to the same loader |
| `Renderer` | `Engine` through the root allocator | `Platform`, `ResourceSystem`, default texture | `Renderer::destroy` |
| Vulkan backend objects | Active `Renderer` | Platform surface and renderer resources | The Vulkan renderer during resource destruction or shutdown |
| `Texture` slot | `TextureSystem` | No high-level resource; opaque backend data belongs to `Renderer` | `TextureSystem`, through `Renderer::destroy_texture` |
| `TextureMap::texture` | Borrowed by its `Material` | `TextureSystem` slot or default texture | Never by `TextureMap`; its material releases the acquired texture reference |
| `Material` slot | `MaterialSystem` | `TextureSystem`, `Renderer`, texture maps | `MaterialSystem`, through `Renderer::destroy_material` and texture release |
| `Geometry` slot | `GeometrySystem` | `MaterialSystem`, `Renderer`, material | `GeometrySystem`, through `Renderer::destroy_geometry` and material release |
| `RenderPacket` arrays | Caller of `Renderer::draw_frame` | Geometries and materials referenced by the packet | Caller; renderer only reads them during the call |

Pointers returned by `acquire` are borrowed handles into fixed-capacity system
storage. The caller never deletes them and must not retain them after the
matching release makes the reference count zero with `auto_release` enabled, or
after system shutdown. A freed slot may be reused at the same address, so pointer
identity alone does not make a stale handle valid.

## ResourceSystem contract

- `Resource` is move-only and does not unload itself. Every successful `load` or
  `load_custom` must be paired with `unload` before `ResourceSystem` is destroyed.
- `Resource::data` belongs to the loader selected at load time. It is valid only
  while that `Resource` remains loaded and must not be retained after `unload`.
- `ResourceSystem::unload` rejects resources owned by another system. A repeated
  unload of an already-reset resource succeeds without work.
- A failed loader may leave a partial payload; `ResourceSystem` asks the same
  loader to clean it before returning the error.
- Built-in loaders are members of `ResourceSystem`. A custom registered loader is
  borrowed and must outlive the system because loader deregistration does not yet
  exist.
- `active_resource_count()` must return to zero before shutdown. A nonzero value
  is a lifecycle error even though shutdown continues defensively.

## TextureSystem contract

- The system owns a fixed array of texture slots and a name-to-slot reference
  map. A first `acquire` loads pixels temporarily through `ResourceSystem`, asks
  `Renderer` to create the GPU texture, unloads the decoded image and returns the
  system slot.
- Every successful non-default `acquire` contributes one reference and requires
  one matching `release(name)`.
- The first acquisition fixes the current `auto_release` policy for that loaded
  name. When its count reaches zero, an auto-released texture is destroyed and
  its slot becomes reusable. A retained texture stays loaded until shutdown or a
  later acquire/release cycle.
- The default texture is owned for the full lifetime of `TextureSystem`, does not
  participate in reference counting and ignores release requests.
- `Renderer::set_default_texture` borrows the pointer. `TextureSystem` clears that
  pointer before destroying the default texture.
- The opaque backend data in `Texture::m_internal_data` belongs exclusively to
  the renderer that created it.

## MaterialSystem contract

- The system owns material slots and name-based reference counts. Material
  pointers returned by `acquire` are borrowed.
- A material retains one reference to each non-default texture it successfully
  acquires. Material destruction releases those references after destroying the
  renderer-side material state.
- Default world and UI materials live for the entire MaterialSystem lifetime and
  borrow the TextureSystem default texture without incrementing its count.
- `set_diffuse_texture` acquires the replacement before publishing it, updates
  the material generation and only then releases the previous texture. Failure
  leaves the original binding intact.
- A missing diffuse texture during material loading currently degrades to the
  default texture and logs a warning. Invalid material configuration and renderer
  failures remain hard errors returned through `result`.

## GeometrySystem contract

- Geometry slots own the renderer-side vertex/index allocation represented by
  `Geometry::internal_id`. Returned `Geometry*` values are borrowed.
- Every successful `acquire` contributes one geometry reference. An auto-released
  geometry is destroyed when its count reaches zero; its slot may then be reused.
- A geometry retains one reference to a non-default material. Geometry
  destruction first destroys renderer data and then releases that material.
- Default world/UI geometries and their matching default materials live until
  system shutdown and do not participate in ordinary reference counts.
- World geometry accepts only world materials and UI geometry accepts only UI
  materials. A mismatch rolls back the renderer upload and returns an error.
- `GeometryConfig` and `Geometry2DConfig` own their temporary `dyarr` data. The
  renderer copies/uploads it during `acquire`; callers may destroy the configs
  after the call returns.

## Renderer and frame contract

- `Renderer::create` constructs the selected backend and returns it through the
  renderer-neutral base. `Renderer::destroy` must receive the allocator used to
  create that object.
- Texture, material and geometry create operations are fallible and publish a
  valid handle only after backend creation succeeds. Their matching destroy
  operation is the sole path that interprets opaque backend state.
- `RenderPacket` is a non-owning view. Its arrays and every referenced resource
  remain alive and unchanged for the duration of `draw_frame`.
- World geometry is recorded before UI geometry. Both passes belong to one frame
  and use the swapchain image acquired by `begin_frame`.
- Swapchain recreation can skip a frame without being an error. Callers inspect
  `frame_outcome` instead of treating every non-rendered frame as failure.
- Resize changes projection state and delegates backend resource recreation. A
  zero-sized/minimized surface may defer rendering until valid dimensions return.

## Error and rollback contract

- Recoverable creation, loading and GPU operations return `result<T, E>`.
- Predicates and best-effort release operations may remain `bool` or `void` when
  absence is part of normal control flow and no actionable error payload exists.
- On failure, no partially-created resource is inserted in a registry and no
  reference count is incremented.
- Rollback runs in reverse acquisition order using the allocator and system that
  originally created each resource.
- Default resources must also obey transactional initialization: failure to create
  a later default destroys every earlier default before returning.

## Threading and handle limitations

The current systems are single-threaded. Reference maps, slot arrays, resource
loaders, renderer commands and generation counters require external serialization.
Thread safety must not be inferred from const methods or fixed-capacity storage.

Current APIs expose borrowed pointers and, for geometry, numeric slot IDs. Slot
reuse means these are not permanent identities. A future stable handle must pair
an index with a generation and validate both before access or release. Until that
change is implemented, callers must strictly stop using a handle after its final
release.

## Requirements for the next roadmap items

- Chapters 42–44 may add CPU and GPU suballocators, but must inject metadata
  allocators explicitly and preserve the high-level shutdown order above.
- Chapters 45–48 must decide whether `ShaderSystem` is owned by `Renderer` or by
  `Engine` before exposing it. In either case it must outlive materials and die
  before `ResourceSystem` and the renderer resources on which it depends.
- Chapters 49–52 may expand vertex/material layouts without changing ownership.
- Chapters 53–56 must define whether `Mesh` owns geometry references or merely
  borrows them; that decision must be added to the ownership matrix.
- Chapter 57 moves sampler ownership from a texture to a texture map and must
  update both TextureSystem and MaterialSystem contracts.
- Chapter 58 must distinguish owned images from external swapchain images so a
  generic texture release can never destroy swapchain-owned memory.
- Chapter 59 makes render targets own attachment references, while image memory
  remains owned by its texture or swapchain source.

## Change rule

A change is incomplete if it alters any creation dependency, destruction order,
reference-count transition, default resource, opaque backend handle, borrowed
pointer lifetime or rollback path without updating this document and its tests in
the same semantic commit.
