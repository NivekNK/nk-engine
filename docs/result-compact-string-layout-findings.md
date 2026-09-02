# Hallazgos del layout compacto de `str`

Fecha: 2026-09-02

## Resultado

La candidata alcanzó `sizeof(str) == 32`, conservó 23 caracteres SSO, no añadió
allocations y pasó la cobertura funcional existente. Se rechaza porque
`assign`, `append`, comparación, move de strings heap e iteración de
contenedores superaron ampliamente la regresión máxima de 3 %. El árbol vuelve
a usar el layout estable de 48 bytes.

## Histograma observado

Se instrumentaron temporalmente las asignaciones propietarias y se ejecutó la
suite completa. El subconjunto File/Renderer cubre rutas de apertura, lectura de
líneas y nombres usados durante inicialización de renderer.

| Longitud | Suite completa | File/Renderer |
| ---: | ---: | ---: |
| 4 | 6 | 6 |
| 5 | 1 | 0 |
| 6 | 2 | 1 |
| 7 | 1 | 0 |
| 9 | 2 | 1 |
| 10 | 2 | 1 |
| 11 | 1 | 0 |
| 23 | 1 | 0 |
| 24 | 1 | 0 |
| 26 | 8 | 6 |
| 34 | 1 | 1 |
| 50 | 5 | 0 |
| 63 | 1 | 1 |

En la suite completa hubo 16 asignaciones dentro de SSO y 16 en heap; el
workload File/Renderer produjo 9 y 8 respectivamente. El intento de smoke test
del editor no llegó a cargar recursos porque el proceso de validación no tenía
acceso al socket Wayland de Niri, por lo que no se mezclaron muestras parciales
de ese arranque con el histograma reproducible.

## Representación evaluada

El objeto candidato contenía:

```text
[Allocator* 8 B][storage activo char[24] 24 B]
```

El storage fue siempre un array de `char`, no una unión. En modo inline, el
último byte codificaba la longitud 0-22 y permanecía como terminador nulo para
longitud 23. En modo heap contenía, mediante `memcpy`, puntero, longitud y
capacidad; el bit alto de capacidad distinguía el modo. Por ello:

- no se etiquetó el puntero;
- no se leyó un miembro inactivo de unión;
- se preservaron bytes nulos embebidos gracias a la longitud explícita;
- el límite de capacidad heap fue `2^63 - 1`;
- la última iteración exigía little-endian, condición cierta en x86-64,
  AArch64 Linux y Windows x64, pero menos general que el layout actual.

La sintaxis `str{allocator, texto}`, el terminador nulo, las transiciones
23/24, copy/move entre allocators, reserve, clear, aliasing y OOM permanecieron
funcionalmente correctos en las pruebas ejecutadas.

## Densidad

| Caso | Layout actual | Candidata | Cambio |
| --- | ---: | ---: | ---: |
| `sizeof(str)` | 48 B | 32 B | -33.3 % |
| storage de 4096 `str` en `arr/dyarr` | 196608 B | 131072 B | -33.3 % |
| `sizeof(result<str, enum_u8>)` | 56 B | 40 B | -28.6 % |

La densidad sí cumple la puerta de tamaño y mantiene el mismo umbral SSO.

## Rendimiento A/B

Entorno: Linux x86-64, GCC 15.2.0, Release. Cada valor es la mediana de cinco
procesos; cada proceso calcula la mediana interna de 15 muestras de 100 000
operaciones. El baseline se recompiló después de retirar la candidata usando el
mismo binario de benchmark y la misma sesión.

| Operación | Longitud | Actual ns/op | Candidata ns/op | Cambio |
| --- | ---: | ---: | ---: | ---: |
| assign | 0 | 5.996 | 7.496 | +25.0 % |
| assign | 8 | 8.940 | 9.470 | +5.9 % |
| assign | 23 | 10.468 | 9.476 | -9.5 % |
| assign | 24 | 7.699 | 11.441 | +48.6 % |
| assign | 64 | 8.588 | 11.170 | +30.1 % |
| assign | 256 | 9.128 | 11.911 | +30.5 % |
| append | 8 | 8.959 | 10.343 | +15.4 % |
| append | 24 | 7.701 | 11.470 | +48.9 % |
| append | 64 | 8.375 | 11.506 | +37.4 % |
| compare | 8 | 6.021 | 6.481 | +7.6 % |
| compare | 24 | 6.226 | 6.468 | +3.9 % |
| move | 8 | 7.545 | 4.984 | -33.9 % |
| move | 24 | 3.777 | 4.864 | +28.8 % |
| move | 64 | 3.875 | 4.858 | +25.4 % |
| iteración `dyarr`, longitud 8 | — | 0.882 | 0.958 | +8.6 % |
| iteración `arr`, longitud 8 | — | 0.792 | 0.952 | +20.2 % |

La candidata favorece moves inline y la frontera exacta de 23 caracteres, pero
penaliza tanto strings heap —la mitad del workload observado— como recorridos de
colecciones. El costo proviene de decodificar el modo/longitud/capacidad que el
layout actual mantiene en campos directos.

## Decisión de puerta y reversión

Se rechaza el layout. Reducir tamaño no basta porque varias cargas frecuentes
exceden 3 %, algunas por más de 40 %. Se retiraron todos los cambios de
representación y no quedó flag, adapter ni implementación alternativa. Los
workloads de comparación, append e iteración sí se conservan en
`optimization_baseline` para evaluar una hipótesis futura distinta.
