# Migración global del runtime

Fecha de validación: 2026-09-02.

## Resultado

La memoria dinámica poseída por el runtime de NK Engine se obtiene ahora mediante un `Allocator` explícito. El logger, las assertions y los diagnósticos de memoria usan storage fijo; `MemorySystem` mantiene su metadata en estructuras propias; y el renderer Vulkan ya no posee storage mediante contenedores STL.

Los dominios iniciales continúan siendo `MallocAllocator` nombrados (`App`, `Renderer` y `EventSystem`). No se introdujo un allocator ambiental, un scope implícito ni un service locator para resolver dependencias.

## Cambios de ownership

- `ApplicationConfig::name` es una vista prestada. `Renderer` conserva el nombre mediante `str{allocator, application_name}`.
- `File` recibe el allocator en su constructor, posee su path mediante `str` y devuelve buffers asignados por el mismo dominio.
- `ObjectShader`, `ObjectShaderObjectState`, `Swapchain` y `Framebuffer` usan `arr`/`dyarr` para descriptor sets, generaciones, imágenes, views y attachments.
- `TextureData` se construye y destruye mediante el allocator del renderer, respetando la duración de sus miembros C++.
- `Engine`, `App`, `Platform`, `Renderer` y `EventSystem` comprueban fallos parciales y destruyen en orden inverso, dejando sus punteros en `nullptr`.
- `MemoryType` usa una interfaz `Provider` con métodos virtuales no propietarios en lugar de `std::function`.

## `MemorySystem`

El tracker usa un `MallocAllocator{untracked}` dedicado exclusivamente a metadata. Durante `Bootstrapping` reserva:

- `dyarr<AllocatorRecord>` para los dominios registrados;
- un único `map<AllocationKey, AllocationRecord>` Robin Hood;
- la clave `AllocationKey{allocator_id, address}`, evitando tablas separadas por allocator.

El reporte incluye el consumo agregado del allocator de metadata. Un reporte final detallado cambia primero a `ShuttingDown`, con lo que deja de aceptar eventos antes de recorrer y destruir las tablas. Overflow del journal, reentrada y fallos de metadata tienen contadores visibles sin intentar registrar los propios diagnósticos.

Los nombres de `AllocatorDescriptor` y los paths de `SourceLocation` son vistas prestadas de literales estáticos, que es el contrato de los call sites actuales. La metadata no copia texto ni genera dependencias circulares.

## Whitelist de ownership externo

Estas operaciones no representan memoria poseída por los contenedores de NK Engine:

- **libc:** `posix_memalign/free` implementan el backend raw de `MallocAllocator`; `FILE*` se obtiene con `fopen` y se libera con `fclose`.
- **XCB:** replies, errores y eventos entregados por XCB se liberan con `std::free`, como exige su ABI.
- **Wayland/XDG/XKB:** proxies, superficies, contexts, keymaps y states se destruyen con sus funciones `*_destroy`/`*_unref`; el display se cierra con `wl_display_disconnect`.
- **Vulkan:** handles y memoria de dispositivo se crean y destruyen con sus pares `vkCreate*`/`vkDestroy*`, `vkAllocate*`/`vkFree*` y `vkAllocateMemory`/`vkFreeMemory`.

Una búsqueda sobre `engine` y `editor`, excluyendo código vendor, no encontró objetos runtime `std::string`, `std::vector`, `std::unordered_map`, `std::map`, `std::function`, `std::format` ni `std::filesystem`. Sólo quedaron overloads transitorios de formatting y headers del PCH, que se retiran en la limpieza final.

## Validación

- Debug: 83/83 tests.
- Debug con ASan y UBSan: 83/83 tests, sin hallazgos.
- Release: 75/75 tests.
- Benchmark de asignaciones globales: `logging.init`, `logging.plain`, `logging.formatted` y `assertion.formatted` reportaron `0` allocations, `0` bytes y `0` frees.
- Wayland/Niri: `NK_PLATFORM_BACKEND=wayland NK_SMOKE_TEST_FRAMES=3 nix run .#run -- Debug` creó la ventana xdg-shell, inicializó Vulkan, renderizó tres frames y cerró con código 0.
- El reporte final del smoke test mostró 0 asignaciones activas y 0 leaks en Native, EventSystem, App y Renderer; la metadata no instrumentada se reportó por separado.

`NK_SMOKE_TEST_FRAMES=N` queda disponible para integraciones que necesiten ejecutar y cerrar el motor de forma determinista después de `N` frames.
