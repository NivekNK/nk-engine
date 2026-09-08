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

Render-pass and target descriptions follow the same boundary. `RenderPassConfig`
and `RenderTarget` contain only neutral formats, operations and borrowed
`Texture*` attachments. Dynamic rendering and the legacy `VkRenderPass`/
`VkFramebuffer` path consume the same config inside Vulkan. The full current
contract is documented in [render-targets.md](render-targets.md).

The Vulkan command boundary follows the scoped, explicit-resource design of
[NoGraphicsAPI](https://github.com/sebbbi/NoGraphicsAPI): `vk::GraphicsCommands`
borrows command buffers, buffer ranges, attachments and push-constant bytes;
it never takes ownership or allocates per draw. This is an NK compatibility
implementation, not a dependency on that prototype. It retains Vulkan 1.2
vertex/index buffers, descriptor sets and optimal image layouts. Dynamic
rendering and synchronization2 are selected independently from reported device
features, through core 1.3 or KHR entry points; `NK_VULKAN_LEGACY=1` exercises
the render-pass/legacy-barrier/submit fallback. Neither path requires mesh
shaders, descriptor heaps, device-address commands or Resizable BAR.

Image and sampler identities are now independent. `TextureBinding` is a borrowed
image pointer plus a generational sampler handle, copied into shader binding
storage. The compatibility backend combines them only when writing a descriptor.
This follows NoGraphicsAPI's separation of texture and sampler descriptions,
while retaining ordinary `VkSampler` objects instead of requiring descriptor
heaps. Sampler policy does not belong to shared image storage.

Image transitions describe producer/consumer uses; discarding an attachment
does not waive its acquire/fence dependency. Swapchain acquisition and its first
color transition share the color-output execution scope. Dynamic world and UI
passes have an explicit color-write/read-write barrier, and presentation follows
an explicit final layout transition. Future Vulkan features must preserve this
boundary and select capabilities without raising the mandatory GPU floor unless
that compatibility change is approved separately.

Frame submission is asynchronous. Two frame slots own their acquire semaphore,
completion fence, descriptor sets and disjoint aligned UBO ranges. The CPU waits
for a slot's fence before writing its mapped uniform storage. Static instance
uniforms are copied only when their content revision differs for that slot.

Swapchain images separately own depth attachments, graphics command buffers and
present-wait semaphores. Acquiring an image permits reuse of its present semaphore;
its previous graphics fence must complete before the command buffer is recorded
again. The normal frame loop never calls `vkDeviceWaitIdle`. Resize, shutdown and
the existing synchronous resource-upload/destruction operations still drain work.
Host-visible buffer mappings stay alive until resize or destruction; callers must
not overwrite an in-flight range. Device-local host-visible memory is preferred
when compatible, with ordinary host-visible memory as the upload/UBO fallback.

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
├── ShaderSystem ───→ Renderer, ResourceSystem, TextureSystem defaults
├── MaterialSystem ─→ ShaderSystem, TextureSystem, ResourceSystem
├── GeometrySystem ─→ Renderer, MaterialSystem
├── Mesh values ────→ GeometrySystem
└── frame-local RenderPacket views
```

Arrows show borrowed lifetime dependencies, not memory ownership. All systems
are allocated by the root allocator. `Renderer` additionally owns an internal
allocator used by its backend implementation.

`TextureSystem` is constructed before `ShaderSystem` so every generic backend
shader can use a live default texture. Material instances may also bind texture
slots owned by that system. Although the public `ShaderSystem` API does not take
`TextureSystem` directly, active backend descriptors make this an indirect
lifetime dependency.

Shutdown is the exact reverse:

```text
Mesh values
→ GeometrySystem
→ MaterialSystem
→ ShaderSystem
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
- `TextureSystem` outlives every material and shader that borrows a texture.
- `ShaderSystem` outlives every material that owns a shader instance ID.
- `MaterialSystem` outlives every geometry that borrows or retains a material.
- `GeometrySystem` outlives every `Mesh` that owns geometry acquisitions.
- A failed partial initialization uses the same reverse order and every
  `shutdown` remains safe to call more than once.

## Ownership matrix

| Object | Owner | Borrowed dependencies | Destruction authority |
|---|---|---|---|
| `ResourceSystem` | `Engine` through the root allocator | Registered custom loaders | `ResourceSystem::destroy` with the same allocator |
| `Resource` payload | Its selected `ResourceLoader`, allocated from the ResourceSystem allocator | Owning `ResourceSystem` and loader | `ResourceSystem::unload` delegates to the same loader |
| `ShaderResourceConfig` payload | Built-in `ShaderResourceLoader` as a loaded `Resource` | Inline strings and configuration slices contained in the same payload | `ResourceSystem::unload`; no view may survive it |
| `Renderer` | `Engine` through the root allocator | `Platform`, `ResourceSystem`, default texture | `Renderer::destroy` |
| Vulkan backend objects | Active `Renderer` | Platform surface and renderer resources | The Vulkan renderer during resource destruction or shutdown |
| `RenderTarget` snapshot | Vulkan renderer target arrays | Writable/external textures owned by the current swapchain generation | Vulkan renderer before replacing attachment views/images |
| Legacy `VkFramebuffer` | Vulkan renderer framebuffer arrays | Current target views and compatible `VkRenderPass` | Vulkan renderer before targets, views/images and render pass |
| `ShaderHandle` slot | Vulkan renderer shader registry | Compatible render pass, device, resources and live textures bound by descriptors | `ShaderSystem::destroy(shader)` through `Renderer::destroy_shader`, or renderer shutdown as a defensive fallback |
| `ShaderSystem` registry | `Engine` through the root allocator | `Renderer`, `ResourceSystem` and indirect texture lifetime | `ShaderSystem::destroy` after materials and before textures/renderer/resources |
| `ShaderSystem::ShaderRecord` | Its `ShaderSystem` fixed-capacity registry | Backend `ShaderHandle`; copied immutable metadata and owned lookup names | `ShaderSystem::destroy(shader)` or system shutdown |
| Shader instance ID | Its shader slot, borrowed by one `Material` | Per-frame-slot descriptor sets and disjoint instance UBO ranges | `Renderer::release_shader_instance` through material destruction |
| Vulkan object vertex/index buffers | Vulkan renderer | Device plus renderer allocator for range metadata | Vulkan renderer after all geometry ranges are released |
| `VulkanGeometryData` ranges | Vulkan geometry slot | Suballocators of the object vertex/index buffers | Vulkan renderer after graphics work using the ranges completes |
| `Texture` slot | `TextureSystem` | No high-level resource; opaque backend data belongs to `Renderer` | `TextureSystem`, through `Renderer::destroy_texture` |
| `TextureMap::texture` | Borrowed by its `Material` | `TextureSystem` slot or default texture | Never by `TextureMap`; its material releases the acquired texture reference |
| `TextureMap::sampler` | Its material owns one acquisition; native object lives in the renderer registry | Creating renderer/device | Material → `TextureSystem::release_map_resources` → `Renderer::release_sampler` |
| `TextureBinding` | Borrowed value copied into shader slots | Texture and sampler owners | Never releases either resource |
| Renderer fallback sampler | Vulkan renderer | Device, all shader slots using the implicit default | Renderer after shader shutdown and GPU drain |
| `Material` slot | `MaterialSystem` | `ShaderSystem` instance, `TextureSystem` and texture maps | `MaterialSystem`, through `ShaderSystem::release_instance` and texture release |
| `Geometry` slot | `GeometrySystem` | `MaterialSystem`, `Renderer`, material | `GeometrySystem`, through `Renderer::destroy_geometry` and material release |
| `Mesh` value | Its caller or containing `dyarr<Mesh>` | `GeometrySystem` and one acquired reference per subgeometry | `Mesh::reset`/destructor before `GeometrySystem` shutdown |
| `RenderPacket` arrays | Caller of `Renderer::draw_frame` | Meshes, geometries and materials referenced by the packet | Caller; renderer only reads them during the call |

Pointers returned by `acquire` are borrowed handles into fixed-capacity system
storage. The caller never deletes them and must not retain them after the
matching release makes the reference count zero with `auto_release` enabled, or
after system shutdown. A freed slot may be reused at the same address, so pointer
identity alone does not make a stale handle valid.

`Mesh::create` is the exception that converts those borrowed geometry pointers
into an unambiguous aggregate lifetime: it owns exactly one successful
`GeometrySystem::acquire` operation per entry in its private `dyarr`. The
pointers remain stable because `GeometrySystem` stores slots in a fixed `arr`,
and the mesh's reference prevents an auto-release slot from being reused while
the mesh is alive. `Mesh` is move-only, releases every acquisition before its
geometry system can shut down, and rolls back all completed acquisitions if a
later subgeometry fails. Its accessors still return non-owning views; callers
never delete or separately release them.

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

## Static mesh import and binary cache contract

- `StaticMeshResourceLoader` prefers a validated `.nkmesh` archive; otherwise it
  imports OBJ using the existing tinyobjloader boundary. Both routes publish the
  same owning `StaticMeshResource` with NK `dyarr` geometry and inline materials.
  Parsing uses a borrowed stream over bounded NK byte storage; tinyobj's STL
  objects never escape the loader. Its internal allocations remain the existing
  third-party exception to allocator tracking/recoverable OOM.
- Cache dependencies record content hashes of OBJ and attempted MTL files,
  including missing libraries, and presence of candidate PNG maps. When the OBJ
  exists, every dependency must match before the cache is used. With no OBJ, a
  structurally valid archive is a standalone asset; texture pixels are still
  separate resources. A corrupt standalone archive fails without publishing data.
- Decoding validates the entire wire representation before allocating its payload.
  The format and its limits are documented in [static-mesh-binary-format.md](static-mesh-binary-format.md).
  Serialization never includes native Vulkan resources, device addresses or C++
  object layouts. `Resource::data_size` includes the logical CPU configuration,
  material, vertex and index bytes, not disk bytes or GPU allocation sizes.
- Cache writes use an exclusive temporary next to the destination and atomic
  replacement. Failure to track, encode or save a cache is non-fatal to a valid
  OBJ import. `NK_MESH_CACHE=off` bypasses cache reads and writes for regression
  checks. It does not bypass bounded source reads or change triangulation.
- Imported/decoded CPU geometry is passed through `Mesh::create` and
  `GeometrySystem` to the existing renderer upload path. The Vulkan backend owns
  the resulting buffer ranges and synchronization; it never borrows archive bytes
  beyond upload. Unloading the CPU resource must not invalidate a live GPU mesh.
  NoGraphicsAPI-inspired command/resource scopes, optional dynamic rendering and
  the legacy fallback retain their previous contracts and GPU requirements.

## Shader resource and ShaderSystem contract

- Shader resources use the versioned `.shadercfg` format. Their fixed-capacity
  payload owns all strings and arrays inline; the returned `ShaderConfig` is a
  borrowed view valid only until the matching resource unload.
- The loader derives attribute layout, uniform offsets, descriptor bindings and
  push-constant ranges before validating the neutral `ShaderConfig`. Syntax and
  layout failures preserve a typed parser error through `ResourceSystem`.
- `ShaderSystem::create` receives an explicit allocator and borrows `Renderer`
  and `ResourceSystem`. `Engine` owns the active registry and loads both built-in
  shader resources after the default texture exists and before materials.
- Loading copies the resource's names and compact immutable uniform metadata
  into fixed-capacity system storage before unloading the temporary resource.
  No resource payload or borrowed configuration slice is retained.
- Shader and uniform name lookups accept `strview`; heterogeneous lookup does
  not construct temporary owning strings. The registry is single-threaded.
- A `ShaderUniformHandle` is local to the shader used to resolve it and becomes
  invalid when that shader is destroyed. The current shader must match before
  bind, apply or uniform operations reach the renderer.
- Limits for shaders, uniforms and global/instance samplers are established at
  system creation. Duplicate names and limit violations fail before publication.
- A failed resource load, metadata copy or backend creation leaves no registry
  entry. Temporary resources are unloaded and a newly-created backend shader is
  destroyed during rollback.
- The generic Vulkan shader owns uniform CPU storage, aligned global/instance
  UBO regions, descriptor layouts and per-instance descriptor state. Instance
  strides respect the device's minimum uniform-buffer offset alignment, and all
  byte ranges are checked before mapping or copying.
- Global and instance samplers, including array elements, route through the same
  metadata-driven path. Local uniforms route to validated push-constant ranges.
- `ShaderSystem` must not destroy or replace a shader while a live material owns
  one of its instance IDs. Material destruction is the synchronization point
  before shader destruction or reload.

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
- Vulkan uploads level zero and generates the complete mip chain in the same
  startup/upload command buffer when the format supports linear source/destination
  blits. Every subresource is transitioned before sampling; the image view
  exposes only initialized levels. Independent samplers use `VK_LOD_CLAMP_NONE`
  and sampling is constrained by those view levels. Unsupported formats and the diagnostic
  `NK_VULKAN_MIPMAPS=0` mode retain a single level. Depth attachments always use
  one level. The current RGBA8 UNORM color convention is unchanged; sRGB-aware
  import/filtering is a separate asset-pipeline change.
- `acquire_map_resources`/`release_map_resources` delegate independent sampler
  ownership to the renderer. They do not acquire or release the map's image.
  Empty sampler release is idempotent; failed release leaves its handle intact.
- Minification, magnification, mip filtering and U/V/W wrap are renderer-neutral
  enums mapped once in `vk::sampler_create_info`. Anisotropy 1 disables it;
  larger finite values are clamped to the enabled device limit. Missing support
  (or `NK_VULKAN_ANISOTROPY=0`) disables it rather than excluding that GPU.
  Border wrap uses opaque float black with normalized coordinates.
- `vk::Samplers` caps live native objects at `min(4096, maxSamplerAllocationCount)`
  including the fallback. Identical configs remain independent acquisitions, with
  no NK deduplication cache. Slot reuse increments generation; stale handles fail
  resolution instead of silently binding a different sampler.
- Sampler creation/replacement/release occurs between frames. The Vulkan frontend
  rejects mutation while graphics commands are recording or await submission.
  Release waits for submitted work before destroying the native sampler, matching
  the current conservative resource-destruction policy. No sampler allocation or
  device-idle wait is added to an unchanged draw. Deferred destruction is future work.

## MaterialSystem contract

- The system owns material slots and name-based reference counts. Material
  pointers returned by `acquire` are borrowed.
- A material retains one reference to each non-default texture it successfully
  acquires. Material destruction first returns its instance ID to `ShaderSystem`
  before releasing its per-map samplers and then those texture references.
- Default world and UI materials live for the entire MaterialSystem lifetime and
  borrow the TextureSystem default texture without incrementing its count. Their
  samplers and shader instances are acquired transactionally; failure to create
  the second default releases the first. World maps own three samplers, UI one.
- Material resources declare `shader=`. Omission remains backward-compatible by
  selecting the built-in world or UI shader. The current material contract only
  accepts the matching built-in shader for each `MaterialType`, because those
  are the two uniform layouts understood by `MaterialSystem`.
- `apply_global`, `apply_instance` and `apply_local` resolve uniforms by name once
  during initialization and route all runtime work through `ShaderSystem`.
- Per-material apply state caches frame number, material/texture generations,
  shader and instance ID. An unchanged material still binds its descriptor set
  for each draw, but uploads instance UBO data and rewrites descriptors at most
  once per frame. Any cached identity or generation change invalidates it.
- `set_diffuse_texture` acquires the replacement before publishing it, updates
  the material generation and only then releases the previous texture. Failure
  leaves the original binding intact.
- `set_sampler(material, use, config)` creates the replacement first, preserves
  image references, drains/releases the old sampler, and only then publishes the
  new handle and material generation. No-op changes allocate nothing. Failure
  leaves the previous configuration usable. Callers use this method, not direct
  edits of a live `TextureMap`'s sampling fields.
- Per-frame Vulkan descriptor keys include image identity/generation and sampler
  handle/generation. This catches sampler-only changes and distinguishes default
  images even though those textures share the invalid registry ID. Missing or
  invalid images use the fallback image with the selected sampler; an unset binding
  uses the renderer fallback sampler. Stale explicit sampler handles are errors.
- Omitted texture names borrow defaults. Failure to load an explicitly requested
  texture, invalid configuration and shader failures return typed errors and roll
  back the acquisitions already completed.

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
- Renderer-side geometry owns one aligned vertex range and, when indexed, one
  aligned index range. The containing Vulkan buffers own the native memory;
  geometry owns only its reservations within those independent address spaces.
- Geometry creation reserves all required ranges, uploads both payloads and only
  then publishes the slot. A failure releases every newly reserved range in
  reverse order. Replacement preserves the previous slot and ranges until the
  new upload succeeds.
- Geometry destruction waits for the graphics queue before returning its ranges
  to the suballocators. This is a conservative synchronization point: a future
  deferred-destruction queue may replace the wait, but a range must never be
  reused while submitted commands can still read it.

## Renderer and frame contract

- `Renderer::create` constructs the selected backend and returns it through the
  renderer-neutral base. `Renderer::destroy` must receive the allocator used to
  create that object.
- Texture, shader and geometry create operations are fallible and publish a
  valid handle only after backend creation succeeds. Their matching destroy
  operation is the sole path that interprets opaque backend state. Materials no
  longer have a renderer-specific create/destroy path.
- Shader creation is also transactional. The public `ShaderHandle` contains a
  compact slot index and generation; native shader, pipeline and descriptor
  objects remain private to the backend. Destroying a slot advances its
  generation so stale handles fail before any native object is accessed.
- A shader slot records the render pass against which its pipeline was created.
  `use_shader` rejects a handle when another pass is active, independently of
  the shader's name. Bind, apply and uniform operations only accept the shader
  most recently selected in that pass.
- Shader uniform handles are compact indices into immutable metadata created
  from a validated `ShaderConfig`. Offset and size conversions to 16 bits, and
  binding and array-length conversions to 8 bits, are checked before
  publication. Typed setters verify the value type and byte width; custom
  uniforms require an explicit byte count.
- Materials borrow shader instance IDs through `ShaderSystem`. Material creation
  publishes only after the descriptor sets exist, and material destruction
  returns the ID before the shader registry or device is destroyed.
- `RenderPacket` is a non-owning view. Its arrays and every referenced resource
  remain alive and unchanged for the duration of `draw_frame`.
- World `Mesh` entries are expanded directly during the render pass; this does
  not allocate a temporary flattened geometry array. A mesh supplies one model
  matrix to all of its subgeometries until the dedicated transform system is
  introduced.
- Material instance payload is uploaded at most once per material generation
  and frame. Consecutive subgeometries using the currently-bound material also
  skip a redundant descriptor bind; changing away and back still rebinds it.
- World geometry is recorded before UI geometry. Both passes belong to one frame
  and use the swapchain image acquired by `begin_frame`.
- Swapchain recreation can skip a frame without being an error. Callers inspect
  `frame_outcome` instead of treating every non-rendered frame as failure.
- Resize changes projection state and delegates backend resource recreation. A
  zero-sized/minimized surface may defer rendering until valid dimensions return.
- The stable-frame allocation smoke check measures rendered frames individually.
  Frames skipped for swapchain recreation are intentionally excluded because
  rebuilding swapchain-sized resources is not steady-state frame work.

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

Current resource APIs expose borrowed pointers and, for geometry and material
instances, numeric slot IDs. Slot reuse means these are not permanent identities.
Shader handles are the exception: they already pair an index with a generation
and validate both before access. Other callers must strictly stop using a
resource handle after its final release until those APIs gain the same property.

## Requirements for the next roadmap items

- Chapters 42–44 introduced the renderer-neutral `FreeList`, the CPU
  `FreeListAllocator` and the renderer-neutral `BufferSuballocator` described in
  [`memory-allocation-contracts.md`](memory-allocation-contracts.md). Vulkan
  object buffers now use the last of these directly for GPU offsets while
  preserving explicit metadata ownership and the shutdown order above.
- Chapter 46 introduced the backend-owned shader registry and renderer-neutral
  handles. Chapter 47 added `ShaderSystem`. Chapter 48 integrated it into
  `Engine`, made it outlive materials, transferred built-in shader ownership out
  of `Renderer`, migrated both material passes and removed the temporary
  `MaterialShader` bridge.
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
