# Evidencia de crecimiento directo de `dyarr`

Fecha: 2026-09-02

## API aceptada

Se añadieron tres operaciones allocator-aware, conservando la convención de
source location de `dyarr`:

- `dyarr_reserve(capacity)`;
- `dyarr_emplace_back(args...)`;
- `dyarr_append(slice<const T>)`.

Las tres devuelven `result<void, storage_error>`. `storage_error` sólo distingue
`out_of_memory` y `capacity_overflow`; no contiene texto, allocator ni estado
propietario. Operar antes de inicializar el contenedor continúa siendo una
violación de contrato fail-fast. `dyarr_pop` y `dyarr_remove` conservan
`optional<T>` porque la ausencia es un resultado normal.

No se añadió inserción por rango: la auditoría de consumidores no encontró un
caso real y esa operación ampliaría el núcleo y su superficie de aliasing sin
beneficio medible actual.

## Construcción y aliasing

Con capacidad disponible, `emplace_back` construye `T` directamente en el slot
final. Cuando debe crecer:

1. asigna el backing storage nuevo sin modificar el anterior;
2. construye el elemento final mientras sus argumentos del bloque anterior aún
   son válidos;
3. relocaliza el rango vivo y libera el bloque anterior.

Así no introduce un temporal ni un move adicional, admite un elemento propio
como argumento y deja el contenedor y el argumento intactos si la allocation
falla. La prueba no trivial confirma cero copy/move con capacidad y, al crecer
desde cuatro elementos usando un elemento propio, una copia solicitada y sólo
los cuatro moves inevitables de la relocalización.

`append` calcula el tamaño total con overflow check, conserva el offset cuando
el slice pertenece al mismo `dyarr`, hace como máximo una reserva y construye el
rango completo. Para tipos triviales la ruta termina en una copia bytewise.

## Rendimiento

Entorno: Linux x86-64, GCC 15.2.0, Release. Cada cifra es la mediana interna de
15 muestras de `optimization_baseline`; ambos candidatos procesan 1 048 576
elementos y comienzan con la misma capacidad.

| Bloque `u64` | Loop de `push_copy` | `append(slice)` | Cambio |
| --- | ---: | ---: | ---: |
| 64 elementos | 0.976 ns/elemento | 0.138 ns/elemento | -85.9 % |
| 4096 elementos | 0.750 ns/elemento | 0.127 ns/elemento | -83.1 % |

La prueba con crecimiento verifica además que un append de 60 elementos sobre
cuatro existentes solicita exactamente una allocation nueva. La operación
histórica `dyarr.push_copy_u64` midió 7.737 ns/op frente a 7.700 ns/op del
baseline ampliado: +0.5 %, dentro de ruido y sin regresión material.

## Factor de crecimiento

Se simuló la política actual y el candidato 1,5× para alcanzar 100 000
elementos desde capacidad cuatro:

| Factor | Allocations | Capacidad final | Elementos relocalizados | Slack final |
| --- | ---: | ---: | ---: | ---: |
| 2× | 16 | 131072 | 131068 | 31072 |
| 1,5× | 26 | 106709 | 213400 | 6709 |

El factor 1,5× reduce 24363 slots de slack al final, pero añade diez allocations
y 62.8 % más relocalizaciones. Sin un workload real que demuestre que ese ahorro
compensa el costo, se conserva 2× y no se expone una policy pública.

## Regresión funcional

La cobertura nueva comprueba:

- reserva sin allocation cuando la capacidad ya basta;
- construcción directa con y sin crecimiento;
- argumento propio durante crecimiento;
- append externo y auto-append que cruza una relocalización;
- una sola allocation para append por rango;
- `capacity_overflow` antes de llamar al allocator;
- OOM transaccional en reserve, emplace y append;
- contrato fatal antes de inicialización.

La suite Debug completa pasó 117/117. La matriz Release y ASan/UBSan se vuelve
a ejecutar en el cierre integral.

## Decisión de puerta

Se acepta la entrega: `emplace_back` elimina el temporal objetivo, `append`
supera ampliamente el mínimo de 10 %, las operaciones anteriores permanecen
estables y no hay razón medida para cambiar el crecimiento 2×.
