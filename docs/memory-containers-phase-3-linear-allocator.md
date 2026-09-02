# Fase 3 — `LinearAllocator`

> Estado: completa
>
> Fecha: 2026-09-01

## Resultado

`LinearAllocator` usa ahora un cursor monotónico alineado sobre la dirección absoluta del backing block. `used_bytes` incluye tanto payload como padding, por lo que describe exactamente el siguiente offset disponible y nunca presenta como libre un espacio consumido por alineación.

El allocator distingue dos formas explícitas de backing:

```cpp
// El allocator padre posee el bloque y debe sobrevivir al linear allocator.
LinearAllocator frame{mem::untracked, parent_allocator, KiB(64)};

// El caller conserva el ownership del bloque externo.
LinearAllocator scratch{mem::untracked, sizeof(storage), storage};
```

Pasar `nullptr` ya no selecciona implícitamente una asignación con `calloc`. Un backing poseído se obtiene mediante el `Allocator&` padre y vuelve al mismo allocator durante destrucción o al reemplazar un destino vacío mediante movimiento.

## Cursor, alineación y capacidad

Para cada reserva se calcula:

```text
current_address = base_address + used_bytes
padding         = distancia hasta la siguiente dirección alineada
aligned_offset  = used_bytes + padding
next_used       = aligned_offset + size_bytes
```

La operación sólo modifica estado después de verificar:

- overflow de `base_address + used_bytes`;
- overflow de la dirección al sumar padding y payload;
- overflow de `used_bytes + padding + size_bytes`;
- `next_used <= reserved_bytes`;
- disponibilidad del contador de allocations activas.

El puntero retornado es `base + aligned_offset`. Después del éxito, `used_bytes` pasa a `next_used`, se incrementa `active_allocations` y se actualiza el pico. Ante cualquier fallo, cursor, pico y contadores permanecen intactos.

Esto funciona aunque la dirección inicial de un bloque externo no esté alineada: el primer allocation consume el padding necesario dentro de la capacidad declarada.

## Ownership del backing

La inicialización propietaria recibe primero el allocator padre:

```cpp
LinearAllocator arena;
arena.allocator_init(
    LinearAllocator,
    "Frame arena",
    MemoryType::Renderer,
    parent_allocator,
    KiB(64));
```

El padre debe:

- estar inicializado antes de crear el linear allocator;
- soportar liberación individual del bloque solicitado;
- sobrevivir al linear allocator y a todos sus movimientos.

El bloque se solicita como memoria raw con alineación de `max_align_t`; las subasignaciones sobrealineadas ajustan después el cursor absoluto. La capacidad visible corresponde a los bytes realmente solicitados al padre.

La forma externa exige dirección no nula y capacidad positiva. Nunca libera ni adopta esa dirección.

Para representar correctamente fallos de backing, la base `Allocator` acepta ahora backends cuyo `init(...)` devuelve `bool`, además de los inicializadores `void` existentes. Un retorno `false` deja el lifecycle en `InitializationFailed` y la llamada `allocator_init*` devuelve `nullptr`.

## Reset

`reset()` invalida todas las subasignaciones, establece `used_bytes` y `active_allocations` en cero, conserva capacidad, backing y pico histórico, y emite un único evento de reset cuando existe tracking.

La política de contenido es explícita mediante `LinearResetMode`:

| Modo | Efecto |
| --- | --- |
| `RetainContents` | Sólo reinicia cursor y contadores. |
| `ZeroMemory` | Inicializa a cero todo el backing antes de reiniciar. |

El modo predeterminado es `ZeroMemory` en builds instrumentadas para conservar la ayuda de diagnóstico existente y `RetainContents` en Release para evitar un recorrido completo del bloque. Release sólo limpia cuando el caller solicita `ZeroMemory`.

`_free_linear_allocator()` se conserva como wrapper de compatibilidad y aplica la misma política predeterminada. No se añadió liberación individual: `_free_raw` continúa devolviendo `false` sin alterar el cursor.

El allocator administra bytes, no lifetimes de tipos. Los propietarios deben destruir todos los objetos vivos antes de `reset()` o antes de destruir el allocator.

## Movimiento

Move construction transfiere backing, allocator padre, ownership, cursor, estadísticas y tracker; el origen queda en `MovedFrom` sin capacidad para liberar el bloque.

Move assignment:

- acepta self-move como no-op;
- rechaza destinos con subasignaciones activas;
- devuelve primero el backing poseído de un destino vacío a su padre;
- no intenta liberar un backing externo;
- transfiere después todo el estado del origen.

De este modo un destino inicializado pero vacío no pierde su reserva anterior y el backing transferido se libera exactamente una vez.

## Verificación

Se añadieron pruebas para:

- secuencias de tamaños y alineaciones mezcladas sobre un backing deliberadamente desalineado;
- padding incluido exactamente en `used_bytes`;
- ausencia de solapamiento entre rangos;
- fallos por capacidad y overflow sin cambios de estadísticas;
- rechazo de liberación individual;
- reset predeterminado distinto en Debug y Release;
- zero-fill explícito en ambas configuraciones;
- backing externo y backing poseído mediante `MallocAllocator`;
- rechazo de dirección externa nula, capacidad cero y padre no inicializado;
- move construction, self-move, move assignment sobre destino vacío y rechazo de destino activo;
- inicialización y `_free_linear_allocator()` mediante los wrappers compatibles con tracking.

Resultados reproducidos dentro del flake en Linux:

```text
Debug build:    correcto
Debug tests:    27/27
Release build:  correcto
Release tests:  22/22
```

## Trabajo deliberadamente diferido

- Destrucción y relocalización de objetos almacenados; corresponde a las primitivas de lifetime y a los contenedores.
- Pool, frame, stack y free-list allocators adicionales.
- Migración global de consumidores y retiro de wrappers de compatibilidad.
- ASan, UBSan y benchmarks finales de toda la familia de contenedores.
