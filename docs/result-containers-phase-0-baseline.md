# Baseline de `result` y contenedores

> Estado: aceptada
>
> Fecha: 2026-09-02
>
> Commit de producto medido: `e66199c`

## Entorno reproducible

- Sistema: Linux 7.2.2 x86-64.
- CPU: AMD Ryzen 5 5500U, 6 núcleos/12 hilos, AVX2 disponible.
- Toolchain: GCC 15.3.0 provisto por el flake.
- Configuración de rendimiento: `Release`, `-O3 -DNDEBUG`, C++20,
  `-fno-exceptions`, memory tracking desactivado por la configuración Release.
- Backend de memoria: `nk::mem::MallocAllocator` sobre `nk::os::allocate_raw`.
- Cada métrica nueva contiene 15 muestras internas. La tabla principal registra
  la mediana de siete procesos independientes después de un proceso de
  calentamiento.

Comandos usados:

```bash
nix run .#build -- Release --target optimization_baseline phase0_baseline collections_benchmark
./bin/Linux-Release/benchmarks/optimization_baseline
./bin/Linux-Release/benchmarks/phase0_baseline
./bin/Linux-Release/benchmarks/collections_benchmark

nix run .#build -- Debug --target tests
nix develop -c ctest --test-dir out/build/Linux-Debug --output-on-failure
nix run .#build -- Release --target tests
nix develop -c ctest --test-dir out/build/Linux-Release --output-on-failure
NK_ENABLE_SANITIZERS=ON nix run .#build -- Debug --target tests
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
nix develop -c ctest --test-dir out/build/Linux-Debug-Sanitized --output-on-failure

NK_PLATFORM_BACKEND=wayland NK_SMOKE_TEST_FRAMES=3 nix run .#run -- Debug
```

## Auditoría de contratos existentes

La regla de migración es semántica, no mecánica. El inventario de headers y
callers del engine queda clasificado así:

| Familia actual | Decisión | Motivo |
| --- | --- | --- |
| Predicados de `InputSystem`, `strview`, `map::contains` y `File::exists` | conservar `bool` | Sólo responden sí/no; no existe causa de error útil. |
| `map::find` y factories cuya ausencia es normal | conservar puntero anulable | La ausencia no es un error recuperable por sí misma. |
| `dyarr::pop/remove` | conservar `optional<T>` | Vacío/índice ausente es un resultado normal sin diagnóstico adicional. |
| `EventSystem::fire/register/unregister` | conservar inicialmente `bool` | Expresan handled/presencia; una revisión posterior sólo procede si aparecen causas recuperables distintas. |
| `Allocator::allocate_*` y `free_*` | conservar puntero/`bool` | OOM es el único fallo operativo; argumentos, estado y tamaños inválidos son contratos o diagnósticos. |
| `str::assign/append/reserve` y crecimiento interno de contenedores | diferir | El único error actual es OOM; se migran sólo en las APIs aditivas previstas para no contaminar todo el hot path. |
| Apertura, lectura, escritura y carga completa de `File` | migrar a `result` | Hay causas distintas (`not_found`, EOF, seek, read, write, modo y OOM) que el caller puede resolver o registrar. |
| Creación de módulos shader | migrar a `result` | El `bool + out` actual pierde el código/capa que falló. |
| Creación e inicialización de renderer/Vulkan | migrar por fronteras | El código nativo `VkResult` es accionable y debe traducirse al cruzar capas. |
| Acquire/present/begin/draw | usar outcome más error | `out_of_date`/`suboptimal` son resultados normales recuperables, no el mismo caso que un fallo Vulkan. |
| Assertions, índices inválidos, objetos sin inicializar y punteros de salida nulos | mantener fatal/assertion | Son violaciones de precondición, no errores recuperables. |

`App::create` y `Platform::create` todavía confluyen fallos en `nullptr`, pero no
se migrarán en el primer corte: sus errores dependen de los dominios File,
renderer y plataforma que deben estabilizarse antes.

## Baseline de rendimiento

Los valores son P50 de nanosegundos por operación entre siete procesos. Se
conservan además P50/P95/P99 por proceso en la salida de
`optimization_baseline`.

### Contratos y allocator

| Métrica | ns/op |
| --- | ---: |
| `bool + out<u64>` | 4.670 |
| puntero anulable a `u64` | 4.645 |
| status enum + `out<u64>` | 4.869 |
| allocate/free 16 B, align 16 | 15.619 |
| allocate/free 16 B, align 32 | 16.785 |
| allocate/free 16 B, align 64 | 16.929 |
| allocate/free 16 B, align 256 | 16.937 |
| allocate/free 64 B, align 16 | 15.585 |
| allocate/free 64 B, align 32 | 17.217 |
| allocate/free 64 B, align 64 | 17.430 |
| allocate/free 64 B, align 256 | 16.402 |
| allocate/free 256 B, align 16 | 15.616 |
| allocate/free 256 B, align 32 | 16.843 |
| allocate/free 256 B, align 64 | 16.702 |
| allocate/free 256 B, align 256 | 16.286 |
| allocate/free 4096 B, align 16 | 46.048 |
| allocate/free 4096 B, align 32 | 59.109 |
| allocate/free 4096 B, align 64 | 59.222 |
| allocate/free 4096 B, align 256 | 59.897 |

La matriz adicional de payloads ejecuta exactamente el mismo trabajo bajo las
tres convenciones. Una ejecución Release representativa dio:

| Payload | `bool + out` | nullable | status + out |
| --- | ---: | ---: | ---: |
| `u32` | 4.640 | 4.554 | 4.621 |
| `u64` | 4.648 | 4.671 | 4.646 |
| puntero | 1.253 | 1.251 | 1.503 |
| `Texture` | 2.496 | 1.510 | 2.497 |
| `str` dentro de SSO | 24.422 | 24.411 | 24.377 |
| `dyarr<u8>` de un elemento | 17.695 | 17.251 | 17.539 |

Estas cifras separan las diferencias de ABI para payloads triviales y el coste
real de ownership para los dos payloads allocator-aware. La aceptación de
`result` repetirá esta matriz dentro del mismo binario candidato.

El coste con tracking activo se valida funcionalmente en Debug y en el smoke
test, no se mezcla con las cifras Release: activar tracking cambia el layout,
la configuración y el trabajo realizado, por lo que no sería una comparación
del fast path Release en igualdad de condiciones. Cualquier optimización de
tracking tendrá su benchmark Debug/RelWithDebInfo separado.

### `map`

| Métrica | ns/op |
| --- | ---: |
| hit `u64`, carga 50 % | 11.297 |
| miss `u64`, carga 50 % | 15.494 |
| hit `u64`, carga 80 % | 21.714 |
| miss `u64`, carga 80 % | 18.355 |
| hit `AllocationKey`, carga 80 % | 27.123 |
| miss `AllocationKey`, carga 80 % | 26.061 |
| hit `strview`, carga 80 % | 30.925 |
| miss `strview`, carga 80 % | 19.253 |
| churn insert/remove `u64`, carga 80 % | 114.836 |
| insert `u64` histórico | 22.981 |
| remove `u64` histórico | 18.986 |

Layout observado:

| Especialización/carga | Capacidad | Bytes | Bytes/elemento vivo |
| --- | ---: | ---: | ---: |
| `map<u64, u64>`, 4096/8192 | 8192 | 327680 | 80.000 |
| `map<u64, u64>`, 6553/8192 | 8192 | 327680 | 50.005 |
| `map<AllocationKey, AllocationRecord>`, 6553/8192 | 8192 | 720896 | 110.010 |
| `map<strview, u64>`, 1638/2048 | 2048 | 98304 | 60.015 |

También se ejecutan claves `u32` a cargas de 50 % y 80 %. Las colisiones y
backward-shift deletion quedan cubiertas por tests
deterministas. Para evitar que una distribución artificial domine la línea
base, el benchmark de rendimiento usa rapidhash con las tres clases de clave
reales; cada cambio de probing deberá añadir su workload adversarial propio.

### `dyarr` y `str`

| Métrica | ns/op |
| --- | ---: |
| `dyarr<u64>` push con crecimiento | 7.700 |
| `dyarr<u64>` inserción media histórica | 587.962 |
| `dyarr<u64>` borrado medio histórico | 430.731 |
| `str::assign`, longitud 0/8/23 | 8.091 / 8.740 / 9.720 |
| `str::assign`, longitud 24/64/256 | 7.999 / 8.298 / 9.210 |
| move alternado `str`, longitud 0/8/23 | 7.962 / 7.240 / 7.312 |
| move alternado `str`, longitud 24/64/256 | 3.262 / 3.254 / 3.323 |

Tamaños ABI actuales: `Texture` 32 B, `str` 48 B (alineación 8, SSO 23),
`dyarr<u8>` 40 B y el objeto `map<u64, u64>` 40 B. Los tipos move-only y no
triviales de `dyarr` se cubren con pruebas exactas de lifetime; sus benchmarks
se incorporarán junto con las operaciones aditivas para medir trabajo real en
lugar de destructores sintéticos.

## Resultado funcional

- Debug: 82/82 tests.
- Release: 74/74 tests.
- ASan/UBSan Debug: 82/82 tests, sin hallazgos ni fugas.
- Wayland/Niri con llvmpipe: tres frames y cierre correcto.
- Frames estables: 0 eventos de allocation en los dos frames comprobados.
- Shutdown: 0 allocations vivas y 0 bytes usados.

No se cambió comportamiento del runtime. La nueva pieza es un target
`optimization_baseline` excluido del build normal, de modo que sólo se ejecuta
cuando se solicita expresamente.
