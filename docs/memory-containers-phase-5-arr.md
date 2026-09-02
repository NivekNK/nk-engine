# `arr<T>`: construcción, ownership y movimientos

> Estado: completa
>
> Fecha: 2026-09-02

## Resultado

`arr<T>` vuelve a ser un array propietario de longitud fija, pero ahora el bloque reservado y los objetos vivos que contiene tienen contratos separados y explícitos. Toda posición de `[0, length)` se construye antes de poder observarse y se destruye exactamente una vez antes de liberar el bloque.

La implementación usa directamente las primitivas de duración de objetos de `memory/object_lifetime.h`; no duplica la lógica entre Debug y Release. La diferencia entre configuraciones se limita a adjuntar archivo y línea a las operaciones del allocator.

El estado sigue ocupando cuatro palabras de máquina en las plataformas de 64 bits soportadas:

| Campo | Propósito |
| --- | --- |
| `T* m_data` | Inicio del bloque poseído |
| `u64 m_length` | Número de objetos vivos |
| `Allocator* m_allocator` | Dominio prestado o allocator transferido |
| `AllocatorOwner::Destroy m_allocator_destroy` | Destructor tipado, nulo cuando el allocator es prestado |

El puntero de destrucción sustituye al antiguo booleano de ownership y aprovecha el espacio que antes terminaba ocupado por padding. Una comprobación de tamaño en los tests protege este layout contra crecimiento accidental.

## Inicialización y lifetime

La inicialización por longitud value-inicializa exactamente `length` objetos. Esto pone en cero escalares, punteros y enums, y ejecuta el constructor por defecto de clases. Sólo está disponible cuando `T` es default-constructible y destructible.

La inicialización por lista reserva el bloque exacto y copy-construye sus elementos, porque los elementos de `std::initializer_list` son constantes. Sólo requiere copy construction y destruction; no impone default construction ni move assignment.

Todas las variantes devuelven `bool` y son transaccionales:

- un allocator nulo, una reserva fallida o una reinicialización inválida devuelve `false`;
- el estado observable de `arr` no cambia ante el fallo;
- una variante con ownership no consume el token hasta que la reserva y la construcción están completas;
- longitud cero no reserva storage, pero conserva el vínculo explícito con el allocator.

Los nombres cómodos existentes se conservan como wrappers de transición: `arr_init`, `arr_init_list`, `arr_init_own`, `arr_init_list_own`, `arr_clear` y `arr_shutdown`. El caller puede comprobar sus nuevos resultados booleanos o ignorarlos temporalmente mientras se migran consumidores.

## `clear`, `shutdown` y destrucción

`clear` destruye el rango vivo en orden inverso, libera el bloque con su tamaño exacto y pone datos y longitud en cero. Conserva el allocator vinculado para que el mismo objeto pueda inicializarse de nuevo dentro del mismo dominio.

`shutdown` realiza primero el mismo cierre del storage y después desconecta el allocator. Cuando existe ownership explícito, ejecuta su destructor concreto y libera la memoria nativa usada por el propio objeto allocator. El destructor de `arr` usa esta misma ruta, de modo que no exige un cierre manual y mantiene el orden correcto: primero elementos y bloque; después allocator.

`arr_reset` permanece únicamente como compatibilidad y ahora equivale a un `shutdown` seguro. Ya no abandona silenciosamente el bloque. Su eliminación final se decidirá junto con los demás wrappers transitorios.

## Ownership tipado del allocator

El nuevo `mem::AllocatorOwner` es un token move-only que conserva tanto el `Allocator*` como la función capaz de destruir su tipo concreto y liberar el tamaño correcto. Sus factories reservan el objeto mediante la capa nativa y distinguen explícitamente creación instrumentada de creación `untracked`.

Ejemplo para un dominio interno no instrumentado:

```cpp
auto owner = mem::AllocatorOwner::make_native_untracked<mem::MallocAllocator>(
    mem::untracked);

cl::arr<u32> values;
if (!values.arr_init_own(std::move(owner), 64)) {
    // La operación falló y owner todavía conserva el allocator.
}
```

El token sólo acepta allocators concretos, no abstractos y con destructor `noexcept`. También puede adoptar una instancia obtenida previamente con la forma nativa correspondiente mediante `adopt_native` o `adopt_native_untracked`.

El overload histórico `arr_init_own(Allocator*, ...)` se conserva para que código antiguo siga compilando, pero diagnostica y devuelve `false`: un puntero base borrado no contiene el tipo dinámico ni el tamaño necesarios para una destrucción segura. No existen call sites del motor que dependan actualmente de ese comportamiento inseguro.

## Movimiento, conversión e iteración

Move construction roba el bloque, el vínculo y el token de ownership sin mover elementos individualmente. Move assignment detecta self-move, libera primero el estado anterior del destino y luego realiza la misma transferencia. El origen queda vacío y desconectado, por lo que ya no necesita `arr_reset`; se retiraron los resets redundantes de `Framebuffer`.

La conversión desde un `dyarr<T>&` realiza una copia legal. La conversión desde `dyarr<T>&&` roba directamente un bloque con capacidad exacta o relocaliza los objetos a un bloque exacto y deja el origen vacío. El ownership antiguo de allocator borrado en `dyarr` se rechaza deliberadamente hasta que ese contenedor adopte `AllocatorOwner` en su propia refactorización.

`begin`, `end`, `cbegin` y `cend` son accesos directos al puntero y a la longitud, sin storage ni asignaciones adicionales. El array vacío mantiene `begin() == end() == nullptr` y puede usarse con range-for.

## Verificación

Los tests de `arr` cubren:

- value-initialization de escalares e iteración mutable y constante;
- construcción y destrucción exactas de elementos no triviales;
- listas, tipos move-only y requisitos por operación;
- move construction, move assignment, destino previamente ocupado y self-move;
- fallo de reserva y reinicialización sin cambios parciales;
- retención del token cuando falla una inicialización propietaria;
- diferencias entre `clear`, `shutdown` y el wrapper `arr_reset`;
- ownership tipado, transferencia del owner y orden de destrucción;
- rechazo seguro del overload borrado `Allocator*`;
- copia y movimiento desde `dyarr` sin reset manual;
- tracking conjunto del objeto allocator y del bloque del array.

Resultados reproducidos dentro del flake en Linux:

```text
Debug build:    correcto
Debug tests:    48/48
Release build:  correcto
Release tests:  42/42
```

## Trabajo deliberadamente diferido

- Reemplazar el booleano de ownership borrado y las operaciones de lifetime restantes de `dyarr`; corresponde a su propia refactorización.
- Retirar definitivamente `arr_reset` y los overloads `arr_init_own(Allocator*)` después de migrar y auditar todos los consumidores.
- Ejecutar sanitizers y benchmarks comparativos en la fase integral de pruebas.
