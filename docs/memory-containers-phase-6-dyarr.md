# `dyarr<T>`: capacidad, lifetime y mutaciones

> Estado: completa
>
> Fecha: 2026-09-02

## Resultado

`dyarr<T>` separa ahora el bloque reservado de los objetos vivos que contiene. `capacity` describe slots de storage raw y sólo `[0, length)` contiene objetos construidos. Todas las operaciones mantienen `length <= capacity` y `data == nullptr` si y sólo si `capacity == 0`.

El estado conserva el layout de cinco palabras de máquina y 40 bytes en las plataformas de 64 bits soportadas:

| Campo | Propósito |
| --- | --- |
| `T* m_data` | Inicio del bloque reservado |
| `u64 m_length` | Número de objetos vivos |
| `u64 m_capacity` | Número total de slots |
| `Allocator* m_allocator` | Dominio prestado o transferido |
| `AllocatorOwner::Destroy m_allocator_destroy` | Destructor tipado del allocator poseído |

El puntero de destrucción sustituye al antiguo booleano y ocupa el espacio que antes completaba su padding. Los tests contienen una comprobación estática del tamaño.

## Reserva y crecimiento

La inicialización por capacidad reserva storage sin construir elementos. Se conserva la capacidad mínima histórica de cuatro slots, incluido `dyarr_init(allocator, 0)`, para no cambiar el comportamiento de crecimiento de los consumidores y del benchmark existente.

La inicialización por longitud reserva al menos `max(capacity, length, 4)` y value-inicializa exactamente `length` elementos. La inicialización por lista copy-construye únicamente los elementos recibidos; el resto del bloque permanece como storage raw.

Cuando una mutación necesita crecer, la capacidad nueva es el máximo entre:

- la capacidad mínima requerida por la operación;
- el doble de la capacidad actual;
- cuatro slots.

El cálculo evita overflow antes de duplicar. El allocator tipado conserva además la comprobación de `sizeof(T) * capacity`. Un índice lejano usa `index + 1` como requisito real, no la longitud anterior, por lo que nunca escribe fuera del bloque.

La reserva es transaccional: primero se obtiene un bloque nuevo, después se relocaliza el rango vivo y sólo entonces se publica el puntero y la capacidad nuevos. Si la allocation falla, datos, longitud, capacidad, valores y token de ownership no cambian.

## Construcción y mutaciones

`push` construye por movimiento en el primer slot libre; `push_copy` construye una copia. `push_ptr` conserva el nombre de compatibilidad y pasa por la misma ruta de copy construction para punteros.

El camino frecuente de `push`, cuando todavía existe capacidad, consiste únicamente en comprobar el límite, construir en el extremo e incrementar la longitud. Las comprobaciones de overflow, referencias internas y realloc quedan en el camino de crecimiento.

Las referencias a elementos del propio `dyarr` se detectan antes de una realloc y se reconstruyen mediante su índice en el bloque nuevo. Esto permite operaciones como `values.push_copy(values[1])` incluso cuando el crecimiento invalida la dirección original.

La inserción tiene dos rutas:

- dentro del rango vivo desplaza el sufijo mediante `relocate_range`, deja el slot de inserción como storage raw y construye allí el valor;
- más allá del extremo value-inicializa `[old_length, index)` y construye el valor en `index`.

Para tipos trivialmente relocables la primitiva compartida usa `memmove`. Para tipos no triviales construye por movimiento y destruye cada origen; no asigna sobre objetos vivos. Esto permite insertar y borrar tipos con move-assignment eliminado, además de los tipos reales `Framebuffer`, `CommandBuffer` y `Fence` del renderer.

`resize` construye el tramo nuevo al crecer y destruye el tramo retirado al reducir. Alcanzar exactamente la capacidad no provoca una realloc. `dyarr_at` reutiliza esta ruta para crear todos los objetos hasta el índice pedido; si no puede cumplir una operación que debe devolver referencia, emite un diagnóstico directo a OS y termina sin depender del logger.

`pop` mueve el último objeto a un `std::optional<T>` inline y destruye su slot. `remove` hace lo mismo con el valor elegido y relocaliza el sufijo hacia la izquierda, sin move-assignment ni allocations del contenedor.

## Reset, liberación y movimiento

Las operaciones de cierre tienen diferencias deliberadas:

| Operación | Destruye elementos | Libera bloque | Conserva allocator | Destruye allocator poseído |
| --- | --- | --- | --- | --- |
| `reset` | sí | no | sí | no |
| `clear` | sí | sí | sí | no |
| `shutdown` | sí | sí | no | sí |

El destructor usa `shutdown`, por lo que un contenedor propietario no necesita cierre manual. Move construction roba el estado completo. Move assignment contempla self-move, libera primero los elementos, bloque y posible allocator del destino, y después transfiere el estado del origen.

Las operaciones dentro de capacidad no reservan memoria. Los tests verifican que `data`, `capacity` y el número de allocations activas permanecen constantes durante secuencias de push, insert, resize y `dyarr_at` que caben en el bloque.

## Ownership tipado e integración con `arr`

`dyarr_init_own`, `dyarr_init_own_len` y `dyarr_init_list_own` reciben `mem::AllocatorOwner&&`. El token sólo se consume después de reservar y construir correctamente. Los overloads transitorios con `Allocator*` diagnostican y devuelven `false` porque el puntero borrado no contiene el destructor ni el tamaño concretos.

El ownership tipado se transfiere también al construir un `arr<T>` desde `dyarr<T>&&`. Si la capacidad ya coincide con la longitud, `arr` roba el bloque; en caso contrario crea un bloque exacto, relocaliza el rango y libera la reserva anterior. En ambos casos el allocator concreto se destruye una sola vez al finalizar el nuevo owner.

## Verificación

Los tests cubren:

- reserva sin construcción y capacidad inicial mínima;
- inicialización por longitud y listas no triviales;
- tipos move-only, sobrealineados y sin move-assignment;
- crecimiento, resize y cierre exacto de lifetimes;
- fast path sin allocations dentro de capacidad;
- inserción central y lejana con huecos value-initialized;
- aliasing durante crecimiento e inserción;
- pop, remove y reset con destrucción correcta;
- fallos transaccionales de inicialización y crecimiento;
- move construction, move assignment y self-move;
- reemplazo y transferencia de allocators poseídos;
- transferencia de ownership desde `dyarr` hacia `arr`;
- tracking del objeto allocator y del storage del contenedor;
- wrappers Debug y Release, incluida la variante `dyarr_init_own_len`.

Resultados reproducidos dentro del flake en Linux:

```text
Debug build:    correcto
Debug tests:    60/60
Release build:  correcto
Release tests:  54/54
```

El ejecutable `phase0_baseline` también compila y termina correctamente, y confirma que `sizeof(dyarr<u64>)` continúa en 40 bytes. Sus tiempos se ejecutaron como smoke benchmark, pero la comparación estadística y cualquier ajuste de política de crecimiento permanecen deliberadamente en la fase integral de benchmarks.

## Trabajo deliberadamente diferido

- Añadir sanitizers y una comparación estadística controlada contra la línea base.
- Decidir mediante benchmarks si la capacidad mínima y el factor de crecimiento requieren ajustes.
- Retirar los overloads propietarios borrados y wrappers sin consumidores al cerrar la migración global.
