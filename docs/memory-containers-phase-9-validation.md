# Validación aislada de memoria y contenedores

Fecha: 2026-09-02.

## Configuraciones coherentes

Las macros que alteran el layout público de `Allocator` se definen ahora una
sola vez para todo el árbol CMake:

- `Debug` y `RelWithDebInfo`: tracking activo.
- `Release`: tracking inactivo.

De este modo engine, editor, tests y benchmarks ya no pueden compilar headers
públicos con layouts distintos dentro de un mismo build. No existe ningún
`#undef NK_ACTIVE_MEMORY_SYSTEM` local en la suite.

CTest se habilita desde la raíz. La variante instrumentada se reproduce sin
pisar el directorio de build normal:

```bash
NK_ENABLE_SANITIZERS=ON .scripts/build.sh Debug --target tests
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir out/build/Linux-Debug-Sanitized --output-on-failure
```

## Resultados de tests

| Configuración | Tracking | Resultado |
| --- | --- | --- |
| Linux Debug | activo | 82/82 |
| Linux Debug + ASan + UBSan | activo | 82/82, cero hallazgos |
| Linux Release | inactivo | 74/74 |

La diferencia de cantidad corresponde a casos que ejercitan exclusivamente la
interfaz de tracking. La cobertura incluye lifecycle frío/listo/detenido de
`MemorySystem`, tracker C++ falso, attach/detach/destrucción, journal temprano,
alineación, OOM transaccional, tipos no triviales y las APIs públicas de los
contenedores y textos propios.

El journal cancela pares allocate/free completados antes del bootstrap sin
reordenar los registros supervivientes; su capacidad fija y overflow también
tienen prueba de límite. Un allocator `untracked` que asigna metadata dentro de
un callback del tracker no vuelve a entrar al tracker.

## Línea de asignaciones previa a la migración del logger

El benchmark `phase0_baseline` intercepta `new/delete` únicamente dentro de su
binario. Resultado Linux Release:

| Sitio | Asignaciones | Bytes | Frees durante la operación |
| --- | ---: | ---: | ---: |
| `logging.init` | 7 | 222 | 0 |
| `logging.plain` | 1 | 129 | 1 |
| `logging.formatted` | 4 | 342 | 4 |

Assertions están desactivadas en Release. La referencia Debug del assertion
formateado fue 4 asignaciones, 401 bytes y 4 frees. Estos valores se volverán a
medir después de migrar logging y assertions.

## Percentiles de contenedores y hash

`collections_benchmark` usa 31 muestras de 20.000 operaciones y reporta tiempo
del lote. Resultado Linux Release de esta máquina:

| Métrica | P50 | P95 | P99 | P50/op |
| --- | ---: | ---: | ---: | ---: |
| `map.insert` | 449.636 ns | 899.765 ns | 1.145.918 ns | 22,482 ns |
| `map.lookup` | 339.560 ns | 360.219 ns | 362.383 ns | 16,978 ns |
| `map.remove` | 378.463 ns | 403.711 ns | 407.948 ns | 18,923 ns |
| rapidhash V3, claves del engine | 178.777 ns | 192.332 ns | 196.270 ns | 8,939 ns |
| FNV-1a, sólo línea comparativa | 510.641 ns | 518.637 ns | 519.879 ns | 25,532 ns |

FNV-1a sólo vive en el binario de benchmark; no forma parte del producto ni es
seleccionable por `map`. En esta muestra rapidhash V3 tarda aproximadamente un
35 % del tiempo de esa referencia sobre paths y nombres representativos del
motor.

Estos números no son umbrales universales: sirven como evidencia local y como
punto de comparación para detectar regresiones posteriores.
