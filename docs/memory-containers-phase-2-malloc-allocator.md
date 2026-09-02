# Fase 2 — `MallocAllocator`

> Estado: completa
>
> Fecha: 2026-09-01

## Resultado

`MallocAllocator` ya respeta toda alineación potencia de dos representable por la plataforma, incluida la memoria sobrealineada. La ruta de asignación es memoria cruda: no inicializa implícitamente el bloque. Cuando un consumidor necesita ceros debe pedirlo mediante los helpers `allocate_zeroed_*`.

El backend concreto ya no invoca `calloc` ni `free` directamente. Delega en la capa OS sin tracking, conserva las macros y helpers públicos actuales y sólo modifica estadísticas después de confirmar el éxito de la operación nativa.

```text
Allocator::_allocate_impl
  -> valida estado, tamaño y alineación
  -> MallocAllocator::_do_allocate
      -> os::allocate_raw
          Linux:  posix_memalign
          Windows: _aligned_malloc
  -> actualiza estadísticas
  -> emite el evento instrumentado
```

## Backend y emparejamiento

Cada plataforma utiliza un único par compatible para todos los tamaños y alineaciones. De esta forma `free_raw` no necesita reconstruir qué variante creó una dirección.

| Plataforma | Asignación | Liberación | Normalización mínima |
| --- | --- | --- | --- |
| Linux | `posix_memalign` | `free` | `sizeof(void*)` |
| Windows | `_aligned_malloc` | `_aligned_free` | `sizeof(void*)` |

En Linux se usa `posix_memalign` en vez de `aligned_alloc`: el tamaño solicitado no necesita ser múltiplo de la alineación. La prueba usa deliberadamente 37 bytes con alineaciones entre 1 y 4096.

`os::allocate_raw/free_raw` permanece sin tracking. Los wrappers `native_*` siguen siendo compatibles y emiten eventos solamente después de una operación nativa exitosa.

## Memoria cruda y cero explícito

La API normal conserva su semántica de memoria cruda:

```cpp
void* data = allocator.allocate_raw(size_bytes, alignment);
```

La inicialización a cero queda visible en el call site:

```cpp
void* bytes = allocator.allocate_zeroed_raw(size_bytes, alignment);
u32* values = allocator.allocate_zeroed_lot_t(u32, count);
```

También están disponibles `allocate_zeroed_t(Type)` y sus métodos internos equivalentes. Estas variantes pasan por la misma asignación, estadísticas y tracking; el `memset` sólo ocurre cuando el backend devolvió una dirección válida.

## Estadísticas y errores

Antes de reservar se comprueba que sumar el nuevo bloque no desborde `reserved_bytes`, `used_bytes` ni `active_allocations`. Tras una asignación correcta se incrementan los tres campos y se actualiza el pico. Tras una liberación correcta se decrementan. Un fallo de plataforma o una operación inválida deja el snapshot intacto.

Los contratos de la base continúan rechazando:

- tamaño cero;
- alineación cero o que no sea potencia de dos;
- tamaño o alineación no representable por `size_t`;
- liberación de `nullptr` o con tamaño cero.

## Validación instrumentada de liberaciones

`AllocationTracker` incorpora `validate_free`. `Allocator` la consulta antes de ejecutar el backend cuando el tracking está activo. `MemorySystem` valida la pareja `{allocator_id, address}` y distingue:

- allocator desconocido;
- dirección desconocida o perteneciente a otro allocator;
- tamaño diferente al registrado;
- dirección ya liberada;
- tracker fuera de su ciclo de vida válido.

El mismo control funciona antes de inicializar `MemorySystem`: durante `Cold` y `Bootstrapping` se consulta el journal inline, sin introducir asignaciones ni una dependencia circular. Una liberación rechazada nunca llega a `free`, no genera un evento y no cambia contadores. En Release esta consulta y sus miembros asociados quedan eliminados por compilación; las liberaciones incorrectas siguen siendo una violación del contrato del caller.

## Verificación

Se añadieron pruebas para:

- alineaciones 1, 2, 4, 8, 16, 32, 64, 256 y 4096;
- tamaños que no son múltiplos de la alineación;
- lectura y escritura del bloque resultante;
- helpers raw y tipados con inicialización explícita a cero;
- `nullptr`, tamaño cero y alineaciones inválidas sin cambios de estadísticas;
- mismatch de tamaño, dirección desconocida y double free con tracking;
- conservación de los wrappers nativos y tipados existentes.

Resultados reproducidos dentro del flake en Linux:

```text
Debug build:    correcto
Debug tests:    19/19
Release build:  correcto
Release tests:  14/14
```

La implementación Windows quedó aislada tras `NK_PLATFORM_WINDOWS` con el par `_aligned_malloc/_aligned_free`. El entorno actual no contiene un toolchain Windows, por lo que su compilación y ejecución deberán volver a comprobarse en Windows o CI multiplataforma. La implementación Linux sí fue compilada y ejecutada con alineaciones normales y sobrealineadas.

La inspección de símbolos de `engine/libengine.a` en Release no encuentra referencias a `MemorySystem`, `AllocationTracker` ni `validate_free`.

## Trabajo deliberadamente diferido

- Alineación del cursor, padding y ownership del bloque de `LinearAllocator`.
- Sustitución del almacenamiento STL transitorio de `MemorySystem`.
- Migración global de consumidores y retiro de macros de compatibilidad.
- Ejecución de ASan y UBSan junto con la batería completa de contenedores.
