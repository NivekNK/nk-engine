# Evidencia de `nk::result`

> Estado: aceptada
>
> Fecha: 2026-09-02
>
> Baseline: `35ad6df`

## Contrato implementado

`engine/include/core/result.h` incorpora `result<T, E>` y
`result<void, E>` como unions discriminadas inline. El wrapper:

- no tiene constructor por defecto ni estado vacío observable;
- se construye sólo mediante tags distintos `ok(...)` y `err(...)`, incluso
  cuando `T == E`;
- no depende de allocators, logging, `str`, contenedores ni `MemorySystem`;
- habilita copy/move según las alternativas y soporta tipos move-only;
- preserva copy/destruction triviales cuando ambas alternativas lo permiten;
- aborta también en Release al acceder a la alternativa incorrecta;
- propaga `constexpr` y `noexcept` en las operaciones aplicables.

Los operadores de asignación no requieren que la alternativa sea asignable:
destruyen la alternativa activa y construyen la nueva. Esto permite usar tipos
move-constructible con asignación eliminada y mantiene un único modelo de
lifetime.

## Layout

Medido en Linux x86-64 con GCC 15.3:

| Instanciación | Tamaño |
| --- | ---: |
| `result<void, enum_u8>` | 2 B |
| `result<u64, enum_u8>` | 16 B |
| `result<Texture, enum_u8>` | 40 B |
| `result<str, enum_u8>` | 56 B |
| `result<dyarr<u8>, enum_u8>` | 48 B |

`result<void, enum_u8>` y `result<u64, enum_u8>` son trivially copyable y
trivially destructible. Los dos límites de tamaño del plan se cumplen sin
pointer tagging ni supuestos de ABI no portables.

## Rendimiento

P50 de nanosegundos por operación entre siete procesos Release, con 15
muestras internas por proceso:

| Payload | `bool + out` | nullable | status + out | `result` | Decisión |
| --- | ---: | ---: | ---: | ---: | --- |
| `u32` | 4.607 | 4.525 | 4.611 | 4.642 | neutral |
| `u64` | 4.629 | 4.560 | 4.642 | 4.430 | aceptada |
| puntero | 1.755 | 1.750 | 1.497 | 1.251 | aceptada |
| `Texture` | 2.493 | 1.752 | 2.490 | 2.422 | neutral frente a `bool + out` |
| `dyarr<u8>` | 18.285 | 17.232 | 18.177 | 17.542 | neutral |
| `str` SSO | 23.440 | 23.677 | 23.415 | 29.622 | rechazar retorno propietario en hot path |

La puerta primaria queda satisfecha por los payloads escalares que usarán
File/renderer: `u64` no presenta regresión y su código generado retorna valor y
discriminador en registros (`RAX`/`DL`) sin hidden out-pointer ni allocation.

El caso `result<str, E>` sí queda descartado para rutas calientes: el objeto de
56 B y su lifetime por retorno empeoran alrededor de 26 % frente a reutilizar
un `str&`. Esto confirma la decisión del plan para `read_line`: outcome/error
tipado con salida reutilizable, no texto propietario dentro del resultado.
`result<dyarr<u8>, E>` permanece viable para carga completa porque su diferencia
está dentro del ruido y transfiere ownership sin copiar bytes.

## Validación

- Debug: 91/91 tests.
- Release: 83/83 tests.
- ASan/UBSan: 91/91 tests, sin hallazgos ni fugas.
- Nueve tests específicos cubren `T == E`, tipos move-only, self-assignment,
  cambio de alternativa, contadores de lifetime, `value_or`, void, ausencia de
  allocations y tres accesos inválidos mediante death tests.
- El benchmark permanece en el target explícito `optimization_baseline`; el
  runtime todavía no consume `result`.

La implementación base se acepta. La forma propietaria con `str` queda
registrada como decisión rechazada y no se llevará a las migraciones iniciales.
