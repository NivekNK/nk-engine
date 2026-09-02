# Evidencia del fast path de alineación fundamental

> Estado: aceptada
>
> Fecha: 2026-09-02
>
> Baseline: `35ad6df`

## Cambio

En Linux, `os::allocate_raw` usa `std::malloc` cuando la alineación efectiva es
menor o igual a `alignof(std::max_align_t)`. Las alineaciones mayores conservan
`posix_memalign`. Ambas rutas se liberan con `std::free`, por lo que no se añade
metadata ni una bifurcación al liberar.

Windows permanece intacto con la pareja `_aligned_malloc/_aligned_free`; no se
mezclan familias de allocation. La validación de cero, tamaños no
representables y alineaciones que no son potencia de dos sigue ocurriendo antes
de seleccionar el backend.

## Medición

Entorno idéntico a la baseline: Release, GCC 15.3.0, glibc del dev shell Nix,
100 000 pares allocate/free por muestra. Se ejecutaron siete procesos y se tomó
la mediana de sus `p50_ns_per_op`.

| 64 bytes | Baseline ns/op | Candidato ns/op | Cambio |
| --- | ---: | ---: | ---: |
| align 16 | 15.585 | 12.061 | -22.6 % |
| align 32 | 17.217 | 15.861 | -7.9 % |
| align 64 | 17.430 | 15.760 | -9.6 % |
| align 256 | 16.402 | 15.829 | -3.5 % |

La ruta objetivo supera la puerta de 15 % y las rutas sobrealineadas no
regresan; su pequeña mejora corresponde a variación favorable del entorno, no
a un cambio de implementación.

## Contratos

Los tests recorren 1, 16, 64, 256 y 4096 bytes con alineaciones fundamental,
32, 64 y 256. Cada puntero satisface el módulo de alineación, permite escritura
y se libera por el mismo API. Los tests existentes mantienen:

- rechazo de tamaño cero y alineaciones inválidas;
- estadísticas actualizadas sólo después de operaciones exitosas;
- detección de tamaño incorrecto, dirección desconocida y double free;
- eventos de tracking y cero allocations activas al terminar.

No cambió la API de `Allocator`, el formato de eventos ni el lifecycle.
