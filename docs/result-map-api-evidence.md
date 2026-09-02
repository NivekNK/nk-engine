# Evidencia de la API tipada de `map`

Fecha: 2026-09-02

## Contratos implementados

- `map_init` y `reserve` devuelven `result<void, map_error>`.
- `insert`, `insert_or_assign` y `try_emplace` devuelven
  `result<insert_outcome, map_error>`.
- `insert_outcome` distingue `inserted`, `already_present` y `assigned` sin
  presentar un duplicado como fallo.
- `map_error` conserva `out_of_memory` y `capacity_overflow`.
- Operar sobre un mapa no inicializado es una violación de contrato y termina
  el proceso; no se disfraza como OOM.
- `find`, `contains`, `at` y `remove` aceptan claves compatibles. Esto permite
  guardar `str` y consultar o borrar con `strview` usando el mismo hash de
  contenido.
- `find` sigue expresando ausencia mediante puntero anulable, `contains`
  conserva `bool` y `remove` conserva presencia/ausencia normal.

No se modificaron todavía el bucket, Robin Hood probing ni backward-shift
deletion. El layout y las mediciones de memoria siguen siendo directamente
comparables con la baseline.

## Construcción directa

La prueba `Map.TryEmplaceConstructsValuesOnlyForSuccessfulInsertions` usa un
valor instrumentado y verifica:

| Operación | Construcciones nuevas de `V` |
| --- | ---: |
| inserción nueva | 1 |
| clave duplicada | 0 |
| crecimiento rechazado por OOM | 0 |

La prueba heterogénea toma el contador de allocations después de insertar un
`str`, ejecuta `contains`, `find` y `at` con `strview`, y comprueba que el
contador no cambia.

## Validación

Comandos ejecutados:

```bash
nix run .#build -- Debug --target tests optimization_baseline collections_benchmark
nix develop -c ctest --test-dir out/build/Linux-Debug --output-on-failure
nix run .#build -- Release --target tests optimization_baseline collections_benchmark
nix develop -c ctest --test-dir out/build/Linux-Release --output-on-failure
nix develop -c env NK_ENABLE_SANITIZERS=ON .scripts/build.sh Debug --target tests
nix develop -c ctest --test-dir out/build/Linux-Debug-Sanitized --output-on-failure
```

Resultados:

- Debug: 108/108 pruebas.
- Release: 100/100 pruebas.
- ASan + UBSan: 108/108 pruebas, sin diagnósticos.
- Los targets `optimization_baseline` y `collections_benchmark` compilan con
  comprobación explícita de `insert_outcome::inserted`.

## Decisión

Se acepta la API. Resuelve la ambigüedad entre duplicado y fallo recuperable,
permite consultas textuales sin ownership temporal y demuestra que
`try_emplace` evita trabajo de construcción. El cambio queda separado de la
optimización posterior del layout para que ambos puedan evaluarse y revertirse
de manera independiente.
