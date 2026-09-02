# Fase 1 — Base `Allocator`

> Estado: completa
>
> Fecha: 2026-09-01

## Resultado

La base de memoria ya no depende de `MemorySystem`. `Allocator` conoce solamente la interfaz abstracta `AllocationTracker`, recibe el tracker de forma explícita y usa el tag `untracked` para los casos internos que deliberadamente no deben instrumentarse.

Las rutas con archivo/línea y sin instrumentación convergen en los mismos métodos internos. Esto evita que Debug y Release ejecuten backends, validaciones o estadísticas distintas.

```text
macro heredado / llamada explícita
              |
              v
Allocator::_allocate_impl
  -> valida estado, tamaño y alineación
  -> backend concreto::_do_allocate
  -> emite evento sólo después del éxito
```

## API y compatibilidad

La selección explícita queda disponible de estas formas:

```cpp
MallocAllocator metadata_allocator{mem::untracked};

MallocAllocator runtime_allocator;
runtime_allocator.allocator_init_tracked(
    MallocAllocator,
    tracker,
    "Runtime",
    MemoryType::System);
```

También se conserva temporalmente la llamada existente:

```cpp
runtime_allocator.allocator_init(
    MallocAllocator,
    "Runtime",
    MemoryType::System);
```

En una build instrumentada, la macro heredada obtiene el tracker predeterminado de `MemorySystem` mediante un puente de compatibilidad. El allocator no conoce ese sistema concreto. En Release, las macros se resuelven directamente a inicialización `untracked`; no existen miembros de tracker, IDs, callbacks ni overloads instrumentados en el objeto o en el binario del engine.

`attach_tracker` y `detach_tracker` existen solamente en builds instrumentadas. Attach exige un allocator inicializado, no instrumentado y sin allocations activas. Detach se rechaza mientras existan allocations activas.

## Contratos implementados

### Inicialización y operaciones

- Un allocator comienza en `Uninitialized` y rechaza allocate/free hasta completar una inicialización explícita.
- Una inicialización repetida se rechaza.
- Si el tracker explícito rechaza el registro, la inicialización termina en `InitializationFailed`; el allocator no cae silenciosamente a modo `untracked` y rechaza allocations.
- `size_bytes == 0` devuelve `nullptr` sin entrar al backend.
- La alineación debe ser mayor que cero, potencia de dos y representable por la plataforma.
- Los helpers de arrays detectan overflow antes de calcular `sizeof(T) * count`.
- Los helpers de construcción no ejecutan placement new si la reserva falla.
- Los helpers de destrucción destruyen el tipo solicitado `T`, incluso cuando reciben un puntero a una base.
- Liberar `nullptr` o un tamaño cero devuelve `false` y no emite evento.

### Estadísticas

Cada allocator expone un snapshot uniforme:

| Campo | Significado |
| --- | --- |
| `reserved_bytes` | Memoria actualmente reservada por el backend o capacidad del bloque. |
| `used_bytes` | Bytes de payload actualmente en uso. |
| `peak_used_bytes` | Máximo histórico de `used_bytes` durante la vida del allocator. |
| `active_allocations` | Número de allocations que continúan activas. |

Los backends actualizan estos valores sólo después de una operación exitosa. `get_size_bytes()` y `get_allocation_count()` permanecen como aliases de compatibilidad.

### Movimiento

El movimiento transfiere estado, tracker e ID y deja el origen en `MovedFrom`. Move assignment se rechaza cuando el destino todavía tiene memoria usada o allocations activas; de esta forma ya no pierde silenciosamente el estado previo. Los allocators con backing propio deben liberar un destino vacío antes de delegar al movimiento base.

## Tracking y bootstrap

`AllocationTracker`, `AllocatorDescriptor`, `AllocationEvent` y `AllocatorResetEvent` no contienen contenedores ni memoria propietaria. Todos los callbacks son virtuales `noexcept` y los eventos transportan el snapshot de estadísticas posterior a la operación.

`MemorySystem` implementa esta interfaz y conserva por ahora `std::vector`, `std::unordered_map` y `std::string` internamente. Esa metadata será migrada a los contenedores propios en una fase posterior.

Los IDs son monotónicos durante cada ciclo del sistema. El ID cero está reservado para las allocations nativas instrumentadas.

`EarlyAllocationJournal` posee 1024 registros inline y no asigna memoria. Sus registros son trivially-copyable y pueden representar alta, baja, allocate, free y reset. Si se supera la capacidad:

- aumenta un contador de eventos perdidos;
- se escribe un diagnóstico directamente mediante OS;
- el bootstrap no entra en `Ready` y termina en `Stopped`.

El ciclo implementado es:

```text
Cold -> Bootstrapping -> Ready -> ShuttingDown -> Stopped
```

Los eventos recibidos en `Cold` o `Bootstrapping` entran al journal y se reproducen en orden. Un guard `thread_local` impide una llamada reentrante al tracker. Los eventos en `ShuttingDown` o `Stopped` se ignoran porque en esos estados sólo debe permanecer operativa la capa OS raw.

## Capa OS

`os::allocate_raw/free_raw` es la primitiva sin tracking. Los wrappers `_native_allocate/_native_free`, conservados detrás de las macros `native_*`, llaman primero a la primitiva y emiten el evento sólo después de que la operación tenga éxito.

La corrección completa de memoria sobrealineada y el emparejamiento de APIs de plataforma pertenecen a la fase 2. La alineación real del cursor de `LinearAllocator` y su padding pertenecen a la fase 3.

## Verificación

Se añadieron pruebas para:

- rechazo antes de `init`;
- tamaño cero, alineación inválida y overflow tipado;
- fallo de construcción sin ejecutar el constructor;
- tracker explícito, attach y detach;
- rechazo de un registro de tracker sin fallback implícito a `untracked`;
- movimiento hacia destino vacío y rechazo de destino ocupado;
- capacidad y overflow del journal;
- separación entre memoria OS raw y wrappers instrumentados;
- replay real de tres eventos tempranos desde `Cold`;
- transiciones `Ready` y `Stopped`;
- ejecución de `arr` con un allocator `untracked`, eliminando el segfault de la línea base.

Resultados reproducidos dentro del flake:

```text
Debug build:    correcto
Debug tests:    15/15
Release build:  correcto
Release tests:  11/11
```

La librería Release no enlaza `MemorySystem`, `default_allocation_tracker` ni callbacks virtuales de `AllocationTracker`. El tipo base tampoco almacena `AllocationTracker*` ni `AllocatorId` en esa configuración.

## Trabajo deliberadamente diferido

- `MallocAllocator` todavía usa `calloc` y no garantiza alineaciones superiores a `max_align_t`: fase 2.
- La detección completa de mismatch, double free y ownership por dirección en `MallocAllocator`: fase 2.
- `LinearAllocator` todavía no alinea el cursor ni contabiliza padding: fase 3.
- Assertions y logging completamente allocation-free: fase 7 y migración global.
- Metadata propia de `MemorySystem` sin STL: después de completar los contenedores.
- Retiro de las macros globales heredadas y del puente de tracker predeterminado: migración global.
