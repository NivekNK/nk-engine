# Evidencia de integración de `result` y contenedores

> Estado: aceptada
>
> Fecha: 2026-09-02
>
> Baseline del ciclo: `35ad6df`
>
> Último commit de producto validado: `a53ac02`

## Auditoría de contratos

La migración final conserva la clasificación semántica del plan:

- `File::exists`, `map::contains` y los predicados de sistemas continúan como
  `bool`;
- `map::find` continúa devolviendo un puntero anulable;
- `dyarr_pop` y `dyarr_remove` continúan usando `optional<T>` para ausencia
  normal;
- File, shader y renderer usan `result` sólo cuando existe un fallo
  recuperable cuya causa debe propagarse;
- frame y swapchain separan outcomes normales de `renderer_error`;
- `file_error`, `shader_error`, `renderer_error`, `map_error` y
  `storage_error` contienen únicamente enums y códigos triviales, nunca
  `str`, `Allocator` ni memoria propietaria.

No quedan adapters de la migración, APIs candidatas duplicadas, rutas SIMD,
flags experimentales ni layouts alternativos. `result` está marcado
`[[nodiscard]]`; GCC/Clang compilan con `-Werror=unused-result` y MSVC eleva
C4834 a error mediante `/we4834`.

## Matriz funcional

Las tres configuraciones se generaron en directorios independientes mediante
el flake y GCC 15.3.0:

```bash
NK_BUILD_DIR=/tmp/nk-engine-result-final-debug \
  .scripts/build.sh Debug --target tests --parallel 4
ctest --test-dir /tmp/nk-engine-result-final-debug --output-on-failure

NK_BUILD_DIR=/tmp/nk-engine-result-final-release \
  .scripts/build.sh Release --target tests --parallel 4
ctest --test-dir /tmp/nk-engine-result-final-release --output-on-failure

NK_BUILD_DIR=/tmp/nk-engine-result-final-sanitized \
NK_ENABLE_SANITIZERS=ON \
  .scripts/build.sh Debug --target tests --parallel 4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir /tmp/nk-engine-result-final-sanitized --output-on-failure

nix build .#nk-engine --no-link
```

Resultados:

| Configuración | Resultado |
| --- | ---: |
| Debug | 117/117 |
| Release | 109/109 |
| Debug + ASan/UBSan/LeakSanitizer | 117/117 |
| Derivación Nix aislada | construida |

No hubo hallazgos de ASan, UBSan ni LeakSanitizer. La diferencia de ocho tests
entre Debug y Release corresponde a casos de tracking compilados sólo cuando
ese subsistema está activo.

## Runtime Wayland/Niri

Comando final:

```bash
nix develop --command env \
  NK_PLATFORM_BACKEND=wayland NK_SMOKE_TEST_FRAMES=3 \
  .scripts/run.sh Debug
```

El editor seleccionó `PlatformWayland` con xdg-shell nativo y Vulkan/llvmpipe,
renderizó tres frames y cerró con código cero. El contador del `MemorySystem`
tomó el final del primer frame como baseline y reportó:

```text
Stable-frame allocation check: 0 allocation events across 2 checked frame(s).
```

El reporte de cierre encontró cero allocations vivas y cero fugas en Native,
EventSystem, App y Renderer.

Una ejecución diagnóstica sin el entorno Nix descubrió una fuga de 1,02 KiB
cuando faltaba `VK_LAYER_KHRONOS_validation`. `cb7a0ae` convirtió el buffer de
propiedades de capas a almacenamiento RAII allocator-aware. La misma ruta de
fallo se repitió y terminó con cero allocations vivas y cero fugas.

## Comparación de rendimiento

Los valores finales son medianas de siete procesos Release, cada uno con las
muestras internas de su harness. Se comparan contra la baseline ampliada del
mismo ciclo.

### Contratos y allocator

| Métrica | Baseline ns/op | Final ns/op | Cambio |
| --- | ---: | ---: | ---: |
| `bool + out<u64>` | 4,670 | 5,054 | referencia, no producto |
| `result<u64, E>` | — | 4,827 | -4,5 % frente al control final |
| allocate/free 64 B, align 16 | 15,585 | 12,009 | -22,9 % |
| allocate/free 64 B, align 32 | 17,217 | 15,891 | -7,7 % |
| allocate/free 64 B, align 64 | 17,430 | 15,873 | -8,9 % |
| allocate/free 64 B, align 256 | 16,402 | 15,886 | -3,1 % |

El retorno propietario `result<str, E>` permanece rechazado para hot paths;
File reutiliza un `str&` para lectura por líneas y sólo retorna ownership de
buffers donde el move resulta neutral.

### `map`

| Métrica | Baseline ns/op | Final ns/op | Cambio |
| --- | ---: | ---: | ---: |
| hit `u64`, 50 % | 11,297 | 11,214 | -0,7 % |
| miss `u64`, 50 % | 15,494 | 13,317 | -14,1 % |
| hit `u64`, 80 % | 21,714 | 18,664 | -14,0 % |
| miss `u64`, 80 % | 18,355 | 15,181 | -17,3 % |
| hit `AllocationKey`, 80 % | 27,123 | 22,953 | -15,4 % |
| miss `AllocationKey`, 80 % | 26,061 | 20,450 | -21,5 % |
| hit `strview`, 80 % | 30,925 | 28,510 | -7,8 % |
| miss `strview`, 80 % | 19,253 | 17,933 | -6,9 % |
| churn `u64`, 80 % | 114,836 | 112,796 | -1,8 % |

El backing storage conserva las reducciones ya aceptadas: 52,5 % para
`map<u64,u64>`, 23,9 % para el mapa real de allocations y 43,8 % para
`map<strview,u64>`. El filtro SIMD fue medido y rechazado; no quedó código de
producto ni flag para seleccionarlo.

### `dyarr` y `str`

| Métrica | Baseline/control ns/op | Final ns/op | Cambio |
| --- | ---: | ---: | ---: |
| push histórico `u64` | 7,700 | 7,887 | +2,4 %, ruido |
| loop por bloques de 64 | 0,945 | 0,945 | control final |
| append por bloques de 64 | 0,945 | 0,143 | -84,9 % |
| loop por bloques de 4096 | 0,753 | 0,753 | control final |
| append por bloques de 4096 | 0,753 | 0,131 | -82,6 % |

La auditoría del harness histórico detectó que el wrapper interno de
`result<void, storage_error>` se alcanzaba incluso cuando el `dyarr` ya tenía
capacidad. `a53ac02` restauró el retorno `bool` directo en esa ruta y reservó
el error tipado para el crecimiento real. El harness de ciclo permanece dentro
del ruido; inserción y borrado históricos quedaron en +0,5 % y -0,1 %.

El experimento de `str` reducía el objeto de 48 a 32 bytes, pero empeoraba sus
rutas frecuentes entre 5,9 % y 48,9 %. Se revirtió por completo; la API,
`str{allocator, texto}`, el SSO de 23 caracteres y el layout de 48 bytes
permanecen vigentes.

## Decisión final

Se aceptan `result`, los errores tipados de File/renderer, el fast path del
allocator, la API y metadata compacta de `map`, y las operaciones aditivas de
`dyarr`. Se rechazan el filtrado SIMD de `map`, el crecimiento 1,5x y el layout
compacto de `str`. No fue necesario revertir ningún cambio ya integrado; los
dos experimentos rechazados se retiraron antes de su cierre documental.
