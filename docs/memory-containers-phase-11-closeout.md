# Cierre de memoria y contenedores propios

Fecha de validación: 2026-09-02.

## Resultado

El ciclo queda cerrado sin APIs temporales de ownership. `arr` y `dyarr` sólo
aceptan un `AllocatorOwner` movible cuando deben poseer también el allocator;
un `Allocator*` siempre significa préstamo. Se retiraron `arr_reset`, los
overloads con allocator alternativo de `arr_clear`/`arr_shutdown`,
`_free_linear_allocator`, `_allocator_init`, `get_size_bytes` y
`get_allocation_count` porque ya no tenían consumidores.

Las variantes públicas Debug/Release conservan la sintaxis de macros para
capturar `__FILE__` y `__LINE__`, pero no contienen dos algoritmos: ambas
delegan en una única implementación interna que recibe `SourceLocation`. Los
overloads duplicados que sólo rechazaban ownership borrado fueron eliminados.

`nkpch.h` ya no importa `format`, `string`, `string_view`, `filesystem`,
`functional`, `variant` ni `mutex`. Los headers públicos que usan facilidades
estándar declaran directamente sus dependencias.

## API final por intención

### Allocators

El modo normal del runtime se inicializa con tracking en las configuraciones
que lo habilitan y se compila al mismo backend sin tracking en Release:

```cpp
nk::mem::MallocAllocator allocator;
if (allocator.allocator_init(
        nk::mem::MallocAllocator,
        "Renderer",
        nk::MemoryType::Renderer) == nullptr) {
    return false;
}

auto* object = allocator.construct_t(MyObject, constructor_argument);
allocator.deconstruct_t(MyObject, object);
```

`allocator_init_untracked` queda reservado para capas inferiores, metadata del
tracker, benchmarks y tests controlados. `LinearAllocator::reset()` conserva
su bloque y reinicia el cursor; la liberación del backing ocurre en su
destructor.

### Texto

El allocator ocupa siempre el primer argumento de un texto propietario:

```cpp
nk::str name{allocator, "Builtin.ObjectShader"};
nk::str copy_in_same_domain{name};
nk::str copy_in_other_domain{frame_allocator, name};

nk::strview borrowed{"literal sin ownership"};
nk::strbuf<256> message;
nk::format_to(message, "texture={} bytes={}", name, byte_count);
```

`str` usa 23 bytes inline antes de reservar; `strview` nunca posee ni garantiza
terminación nula; `strbuf<N>` nunca asigna y expone truncación. No existe un
allocator ambiental ni `NK_ALLOCATOR_SCOPE`.

### Contenedores

```cpp
nk::cl::arr<Vertex> vertices;
vertices.arr_init(&allocator, vertex_count);

nk::cl::dyarr<Entity> entities;
entities.dyarr_init(&allocator, 64);
entities.dyarr_push_copy(entity);

nk::cl::slice<const Vertex> view{vertices};

nk::cl::map<nk::u64, Texture> textures;
textures.map_init(&allocator, 128, nk::hash_seed::deterministic);
textures.insert_or_assign(texture_id, texture);
```

`arr` tiene longitud fija, `dyarr` conserva capacidad y permite crecimiento,
`slice` es una vista sin ownership y `map` es una tabla plana Robin Hood que
usa siempre el wrapper `hash64` sobre rapidhash V3. Un rehash invalida
referencias e iteradores. Cada contenedor debe finalizar antes que el allocator
prestado; las formas `*_init_own` reciben exclusivamente un `AllocatorOwner` y
destruyen el allocator después de su storage.

## Whitelist estándar y externa

Se conservan facilidades estándar que no introducen ownership dinámico del
runtime: traits, concepts, `initializer_list`, `optional`, `tuple`,
`numeric_limits`, `chrono`, `thread`, `charconv`, placement construction y las
operaciones de bytes revisadas por las reglas de lifetime.

La memoria devuelta por terceros mantiene obligatoriamente su deallocator:
`free` para replies/errores/eventos de XCB, las funciones `*_destroy` y
`*_unref` de Wayland/XKB, los pares `vkCreate*`/`vkDestroy*` y
`vkAllocate*`/`vkFree*` de Vulkan, y `fopen`/`fclose` para streams libc. El
`free` de `os.cpp` es el par del backend raw creado mediante
`posix_memalign`.

La auditoría final de `engine` y `editor`, excluyendo vendor, no encuentra
`std::string`, `std::string_view`, `std::vector`, `std::unordered_map`,
`std::map`, `std::function`, `std::format` ni `std::filesystem`.

## Frame estable

`MemorySystem` mantiene un contador monotónico de eventos de asignación
aceptados. Cuando `NK_SMOKE_TEST_FRAMES` solicita al menos dos frames, el
primero permite completar cualquier inicialización lazy y los siguientes se
comparan contra ese contador. La ejecución real:

```bash
NK_PLATFORM_BACKEND=wayland NK_SMOKE_TEST_FRAMES=3 nix run .#run -- Debug
```

se ejecutó en Wayland/Niri con Vulkan/llvmpipe, cerró con código 0 y reportó
`0 allocation events across 2 checked frame(s)`. Antes y después de esos
frames las estadísticas propias fueron idénticas: Native 216 B/3 activos,
EventSystem 256 B/4, App 99,76 KiB/3 y Renderer 1,30 KiB/21. El reporte final
encontró cero fugas y cero asignaciones activas en todos esos dominios.

## Comparación con la línea base

Medición Linux Release aislada, misma máquina, Nix, GCC 15.3 y metodología de
la línea base:

| Métrica | Inicial ns/op | Final ns/op | Cambio |
| --- | ---: | ---: | ---: |
| `malloc.allocate_free_64B` | 8,433 | 14,428 | +71,1 % |
| `dyarr.grow_push_u64` | 8,084 | 9,319 | +15,3 % |
| `dyarr.insert_middle_u64` | 587,276 | 587,692 | +0,1 % |
| `dyarr.remove_middle_u64` | 434,769 | 432,129 | -0,6 % |

La edición dentro de capacidad permanece esencialmente igual. La diferencia
de asignación aislada y crecimiento corresponde al contrato nuevo: el backend
raw usa `posix_memalign` para respetar cualquier alineación, y cada operación
actualiza lifecycle y estadísticas coherentes. Es un coste visible y queda
registrado para optimizarlo con una ruta rápida medida si se vuelve relevante;
no se oculta debilitando el contrato.

El layout Release cambió de 40 a 56 bytes para `Allocator`/`MallocAllocator` y
de 48 a 72 para `LinearAllocator`; `arr<u64>` continúa en 32 y `dyarr<u64>` en
40. Logging mejoró de 7/1/4 asignaciones en init/plain/formatted a 0/0/0; la
assertion formateada pasó de 4 a 0.

Los percentiles finales de `map` permanecen alineados con la validación
aislada anterior: insert 22,386 ns/op P50, lookup 17,187 y remove 18,659.
rapidhash V3 midió 8,959 ns/op frente a 25,475 de FNV-1a usado sólo por el
benchmark.

## Decisión sobre `pool<T>` y `handle<T>`

No se implementan todavía. La evidencia actual no muestra asignaciones en el
frame estable y los objetos Vulkan persistentes se crean en lotes durante
inicialización. Añadir ahora slots, free-list, generaciones y política de
agotamiento aumentaría superficie y metadata sin resolver un consumidor real.

La decisión se reabre cuando exista creación/destrucción frecuente de recursos
u objetos con referencias que deban sobrevivir a reutilización de slots. El
primer candidato razonable es el estado de objetos renderizables de
`ObjectShader`; antes de diseñar la API se medirá su churn, máximo simultáneo y
necesidad de detectar handles obsoletos.

## Validación final

- Linux Debug: 82/82 tests.
- Linux Debug con ASan y UBSan: 82/82, cero hallazgos.
- Linux Release: 74/74 tests.
- Paquete aislado: `nix build .#nk-engine --no-link` aprobado.
- Logging y assertions: cero asignaciones en los cuatro sitios medidos.
- Wayland/Niri: tres frames, cero eventos de asignación después del primer
  frame, cierre con código 0 y cero fugas propias.
