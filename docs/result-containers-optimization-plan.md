# Plan de `result` y optimización de contenedores

> Estado: en progreso
>
> Versión del plan: 1
>
> Última actualización: 2026-09-02
>
> Baseline funcional anterior a este ciclo: `258b090`

## Objetivo

Incorporar un tipo propio `nk::result<T, E>` para representar errores
recuperables sin excepciones ni asignaciones dinámicas, y optimizar las
estructuras existentes únicamente cuando las mediciones demuestren una mejora
real.

Este ciclo abarca:

1. El contrato y la implementación de `result<T, E>` y `result<void, E>`.
2. La migración inicial de I/O y renderer/Vulkan a errores tipados.
3. Una ruta rápida de asignación para alineaciones normales.
4. Mejoras de API y layout interno de `map` sin crear otro mapa público.
5. Operaciones de capacidad y construcción directa en `dyarr`.
6. La evaluación de un layout más compacto para `str`.
7. Pruebas de regresión, benchmarks comparables y un procedimiento de
   reversión independiente para cada cambio.

El propósito de `result` es mejorar los contratos. No se asumirá que envolver
un valor en `result` lo hace más rápido que `bool`, un puntero anulable o un
`enum`. Cada API conservará la representación más pequeña que exprese
correctamente su semántica.

## Principios no negociables

- Se mantiene C++20 y el build sin excepciones.
- `result` no conoce `Allocator`, `MemorySystem`, logging ni contenedores.
- `result` almacena inline exactamente un valor o un error y nunca posee un
  estado vacío.
- Los errores no contienen `str` ni realizan allocations para poder
  propagarse.
- Una violación de precondición se resuelve mediante assertion/fatal; no se
  transforma en un error recuperable.
- Una ausencia normal no se transforma automáticamente en error.
- `map` continúa siendo el único mapa público y conserva rapidhash V3.
- La primera optimización de `map` conserva Robin Hood y backward-shift
  deletion. Cambiar el algoritmo de probing exige otro ciclo y evidencia
  independiente.
- `arr` y `slice` no se modificarán sin un consumidor o medición que lo
  justifique.
- No se ampliará `is_trivially_relocatable_v` más allá de los tipos para los
  cuales la relocalización por bytes sea legal en C++20.
- No se usará pointer tagging, bits libres de punteros ni representaciones que
  dependan de una ABI particular.
- Cada etapa termina en un commit semántico que describe el cambio, sin
  mencionar números de fase.
- Un experimento rechazado deja evidencia documental, no código muerto ni un
  feature flag permanente.

## Clasificación de resultados

Antes de migrar una función se aplicará esta tabla:

| Situación | Representación |
| --- | --- |
| Predicado puro | `bool` |
| Ausencia normal de un objeto | puntero anulable u `optional<T>` |
| Varios resultados normales sin payload | `enum class ..._status` |
| Valor más fallo recuperable | `result<T, E>` |
| Operación sin valor más fallo recuperable | `result<void, E>` |
| Resultado normal múltiple más fallo | `result<outcome, E>` |
| Precondición o invariante rota | assertion/fatal |
| Fallo que el proceso ha decidido no recuperar | fatal en el límite definido |

`result<bool, E>` se evitará salvo que `true` y `false` sean realmente parte
del valor producido. Un `bool` usado sólo como éxito/fallo se representa con
`result<void, E>`.

## Contrato propuesto de `result`

### Ubicación y dependencia

- Header público: `engine/include/core/result.h`.
- Namespace: `nk`.
- Dependencias permitidas: tipos fundamentales, traits, utilities y primitivas
  estándar de construcción/destrucción.
- Dependencias prohibidas: `Allocator`, `MemorySystem`, `str`, logging,
  assertions formateadas y cualquier contenedor del engine.

El tipo contenido sí puede ser `str`, `dyarr<T>` u otro objeto allocator-aware;
el allocator pertenece a ese valor y nunca al wrapper `result`.

### Superficie inicial

```cpp
namespace nk {
    template <typename T, typename E>
    class [[nodiscard]] result;

    template <typename E>
    class [[nodiscard]] result<void, E>;

    template <typename T>
    constexpr auto ok(T&& value);

    constexpr auto ok();

    template <typename E>
    constexpr auto err(E&& error);
}
```

La API inicial incluirá:

- construcción explícita mediante `ok(...)` y `err(...)`;
- `has_value()` y conversión explícita a `bool`;
- `value()` y `error()` con overloads `&`, `const&` y `&&`;
- `operator*` y `operator->` cuando `T` lo permita;
- `value_or` únicamente si no fuerza copies innecesarias;
- copy/move/destructor habilitados condicionalmente según `T` y `E`;
- soporte para valores move-only;
- especialización sin storage de valor para `result<void, E>`;
- `constexpr` y `noexcept` propagados desde las operaciones contenidas.

No se permitirá `result<T&, E>` en este ciclo. Tampoco habrá constructor por
defecto, conversiones implícitas ambiguas, estado `valueless`, heap interno ni
tracking de "error observado" dentro del destructor. `[[nodiscard]]` y los
warnings del compilador serán el mecanismo para detectar resultados ignorados.

Acceder a la alternativa incorrecta debe producir un fallo de contrato visible
también en Release; nunca continuará con comportamiento indefinido y nunca
intentará lanzar una excepción.

### Errores

Cada dominio tendrá enums o estructuras pequeñas:

```cpp
enum class file_error : u8 {
    invalid_mode,
    already_open,
    not_found,
    end_of_file,
    seek_failed,
    read_failed,
    write_failed,
    out_of_memory,
};

struct renderer_error {
    renderer_error_code code;
    i32 native_code;
};
```

El texto se genera en logging/UI al consumir el error. El código nativo se
conserva cuando entrega información accionable, por ejemplo `errno` o
`VkResult`. Un subsistema traduce el error inferior a su propio dominio cuando
cruza una frontera de abstracción.

## Dependencias entre etapas

```text
baseline
├── result base ──> File/shaders ──> renderer/Vulkan
├── allocator raw fast path
├── map API ──> metadata escalar compacta ──> SIMD opcional
├── dyarr API y operaciones por rango
└── experimento de layout de str
                         └──────────> integración y cierre
```

Los carriles se mantienen en commits separados. `result` no depende de las
optimizaciones, y el cambio de metadata de `map` no depende de SIMD.

## Método de medición y puertas globales

### Entorno

- Usar el flake y el mismo compiler/configuración para baseline y candidato.
- Ejecutar benchmarks en Release y tests en Debug, Release y ASan/UBSan.
- Registrar CPU, compiler, flags, commit, backend de memoria y estado de
  tracking.
- Hacer al menos una ejecución de calentamiento y siete procesos independientes
  por benchmark aceptado.
- Cuando esté disponible, fijar afinidad de CPU y evitar tareas concurrentes.
- Conservar muestras individuales, P50/P95/P99 y allocations/bytes, no sólo el
  promedio final.

La banda de ruido de una métrica será:

```text
ruido = max(3 %, 2 * MAD / mediana)
```

donde MAD es la desviación absoluta mediana de las ejecuciones baseline.

### Requisitos funcionales absolutos

Un cambio se rechaza inmediatamente si aparece cualquiera de estos casos:

- fallo nuevo de tests;
- hallazgo de ASan o UBSan;
- lifetime incorrecto, fuga, double-free o contador de destructores erróneo;
- cambio de resultado observable no documentado;
- allocation nueva en una ruta declarada allocation-free;
- fallo de compilación en una plataforma soportada;
- pérdida de alineación o corrupción ante colisiones, rehash o fallos
  inyectados;
- allocation durante los frames estables ya validados.

### Criterio general de rendimiento

- Una regresión es material si empeora más que la banda de ruido y al menos un
  3 % en la mediana.
- Una regresión superior al 5 % en una métrica primaria obliga a corregir o
  revertir, aunque otra métrica mejore.
- Una optimización orientada a velocidad debe mejorar su métrica objetivo más
  que el ruido y al menos un 5 %.
- Una optimización orientada a memoria puede aceptarse con velocidad neutral si
  reduce al menos un 20 % los bytes reservados en su carga objetivo.
- No se compensará una regresión de frame/runtime con una mejora exclusiva de
  microbenchmark.
- Si hay un intercambio legítimo de memoria por velocidad, debe existir un
  consumidor real que lo justifique y la decisión se documentará expresamente.

### Presupuesto de complejidad

La mejora se rechaza aunque gane el benchmark si exige comportamiento
dependiente de UB, flags permanentes para seleccionar dos implementaciones,
duplicación sustancial del algoritmo, o invariantes que no puedan cubrirse con
tests deterministas.

## Fase 0 — Inventario y baseline ampliada

### Trabajo

- [x] Registrar commit, toolchain y comandos exactos de la baseline.
- [x] Conservar las mediciones finales anteriores como referencia histórica,
      pero volver a medirlas con el harness de este ciclo.
- [x] Inventariar cada `bool`, `nullptr`, `optional` y out-parameter de `engine`
      y clasificarlo con la tabla de resultados.
- [x] Marcar qué errores son recuperables y en qué capa se consumen.
- [x] Añadir benchmarks comparativos para `bool + out`, puntero anulable y
      status enum usando `u32`, `u64`, puntero, `Texture`, `str` y `dyarr<u8>`.
- [x] Medir allocator con bloques de 16, 64, 256 y 4096 bytes y alineaciones
      normal, 32, 64 y 256 en Release; validar tracking activo por separado en
      Debug para no comparar configuraciones y layouts distintos.
- [x] Medir `map` con hits y misses separados, cargas de 50 % y 80 %, y claves
      `u32`, `strview` y `AllocationKey`.
- [x] Medir inserción, lookup, remove, churn insert/remove, bytes reservados y
      bytes por elemento vivo; cubrir colisiones forzadas con tests
      deterministas hasta que se evalúe un algoritmo de probing distinto.
- [x] Medir crecimiento, append repetido, inserción y borrado de `dyarr` con el
      tipo trivial representativo; validar move-only y no triviales mediante
      contadores exactos hasta medir las operaciones aditivas reales.
- [x] Medir `str` en longitudes 0, 8, 23, 24, 64 y 256 y registrar su tamaño de
      objeto.
- [x] Repetir el smoke test Wayland/Niri de tres frames.

### Criterio de salida

Existe una evidencia reproducible y no se ha cambiado comportamiento del
runtime.

### Commit esperado

`test(performance): capture error and container baselines`

### Reversión

Sólo se añaden tests, harness y documentos. Si el harness introduce ruido o
efectos laterales, se corrige antes de continuar; ninguna optimización puede
aprobarse con una baseline dudosa.

## Fase 1 — Implementación aislada de `result`

### Trabajo

- [x] Implementar `result<T, E>` y `result<void, E>` sin consumidores del
      runtime.
- [x] Implementar `ok`/`err` y desambiguar correctamente cuando `T == E` o hay
      conversiones entre ambos.
- [x] Preservar trivialidad condicional; un destructor manual no debe volver no
      trivial a todas las instanciaciones.
- [x] Verificar que no existe construcción por defecto y que
      copy/move/destruction sólo están disponibles cuando los tipos contenidos
      los soportan.
- [x] Probar valores move-only, errores move-only, self-assignment, cambio de
      alternativa y acceso incorrecto.
- [x] Probar contadores exactos de construcción y destrucción.
- [x] Probar `result<void, E>` y propagación de errores compactos.
- [x] Verificar que construir, mover y destruir `result` no asigna memoria.
- [x] Añadir checks de tamaño informativos para las instanciaciones objetivo.
- [x] Inspeccionar código generado y comparar los benchmarks de la fase 0.

### Puerta específica

- `result<void, enum_u8>` debe caber en 2 bytes en las plataformas de 64 bits
  actuales.
- `result<u64, enum_u8>` no debe superar 16 bytes.
- Una instanciación con alternativas triviales debe ser trivially destructible
  y, cuando aplique, trivially copyable.
- El success path no puede presentar una regresión material frente al contrato
  equivalente `bool + out`.

Estos tamaños se validan como objetivos de implementación, no como ABI pública
persistente.

### Criterio de salida

El tipo supera tests de lifetime, sanitizers y benchmarks sin que ninguna API
del runtime dependa todavía de él.

### Commit esperado

`feat(core): add allocation-free result values`

### Reversión

Al no tener consumidores, el commit puede revertirse completo. Si sólo falla
una optimización de layout, se conserva el contrato y se vuelve a una unión más
simple antes de migrar call sites.

## Fase 2 — Errores tipados de archivo y shaders

### Trabajo

- [x] Definir `file_error` sin strings propietarios.
- [x] Mantener `File::exists` como `bool`.
- [x] Convertir `open` a `result<void, file_error>`.
- [x] Convertir el cierre explícito en una operación capaz de reportar errores;
      el destructor mantiene un cierre best-effort que no propaga ni lanza.
- [x] Representar EOF de `read_line` como outcome normal, separado de I/O
      fallida, conservando un `str&` reutilizable si evita allocations.
- [x] Convertir `read`/`write` a `result<u64, file_error>` y usar `slice` donde
      elimina puntero+tamaño inválidos.
- [x] Evaluar `result<dyarr<u8>, file_error>` para `read_all_bytes` frente a una
      salida reutilizable; escoger mediante allocations, movimientos y claridad
      de ownership medidos.
- [x] Propagar el error en carga de shaders y loguearlo una sola vez en el
      límite que conoce el path y el contexto.
- [x] Añadir casos deterministas para open, seek, EOF, read parcial, OOM y
      escritura; validar close normal/idempotente sin introducir un backend
      virtual sólo para forzar un fallo no portable de `fclose`.

### Criterio de salida

Los fallos de archivo conservan su causa, el buffer tiene un owner inequívoco y
la ruta de carga de shaders no pierde información ni duplica logs.

### Commit esperado

`refactor(io): propagate typed file failures`

### Reversión

El cambio de File y el call site de shaders permanecen en el mismo commit. Si
la devolución propietaria de bytes resulta peor, se revierte sólo esa decisión
y se usa `result<void, file_error>` con salida explícita; no se elimina
`result` ni se vuelve a un `bool` sin causa.

## Fase 3 — Outcomes y errores del renderer/Vulkan

### Trabajo

- [x] Definir `renderer_error_code`, `renderer_error` y outcomes normales de
      frame/swapchain.
- [x] Tratar argumentos inválidos de `Renderer::create` como contrato y
      distinguir OOM de fallo de inicialización en su resultado.
- [x] Hacer fallible `init` sin dejar un renderer parcialmente publicable.
- [x] Convertir creación de textura a un resultado con ownership claro.
- [x] Separar `frame_outcome::rendered` y
      `frame_outcome::skipped_swapchain_recreation` de fallos reales.
- [x] Preservar `VkResult` relevante sin filtrar detalles Vulkan por toda la API
      pública.
- [x] Eliminar logs internos que provoquen duplicación al propagar el mismo
      error.
- [x] Probar device lost, out-of-date/suboptimal, fallo de fence, submit,
      allocation de textura e inicialización parcial.
- [x] Repetir smoke test Wayland/Niri y validar shutdown después de cada punto
      de fallo inyectado.

### Criterio de salida

Saltar un frame por recreación no se confunde con error, ningún objeto parcial
escapa de su factory y el rendimiento del frame exitoso permanece dentro de la
banda de ruido.

### Commit esperado

`refactor(renderer): distinguish frame outcomes and failures`

### Reversión

La migración se divide internamente por límites: factory/init, textura y frame.
Si un límite no resulta adecuado, se revierte su commit antes de continuar. Un
adapter temporal puede existir durante la migración, pero debe eliminarse en el
cierre y nunca convertirse en una segunda API permanente.

## Fase 4 — Ruta rápida del allocator raw

### Trabajo

- [x] Perfilar qué parte del coste corresponde a `posix_memalign`, tracking y
      lifecycle.
- [x] En Linux, usar `malloc` para alineaciones satisfechas por
      `alignof(std::max_align_t)` y conservar `posix_memalign` para tipos
      sobrealineados.
- [x] Mantener `free` como par válido de ambas rutas Linux.
- [x] No mezclar `_aligned_malloc` con `free` en Windows. Conservar la ruta
      Windows actual salvo que el contrato de free reciba información suficiente
      para distinguir backends.
- [x] Probar tamaños cero, overflow, alineaciones inválidas y todos los tamaños
      y alineaciones de la baseline.
- [x] Verificar que los eventos y estadísticas del tracker sean idénticos.

### Puerta específica

- Mejorar al menos 15 % la mediana de allocate/free de 64 bytes con alineación
  normal, o recuperar al menos la mitad de la diferencia documentada respecto a
  la baseline histórica.
- No empeorar más de 3 % las rutas sobrealineadas.
- No modificar el número de eventos ni debilitar validaciones.

### Commit esperado

`perf(memory): add a fast path for fundamental alignment`

### Reversión

La selección del backend debe quedar contenida en `os.cpp`. Si no supera la
puerta, se revierte ese commit y se conserva `posix_memalign`; no se toca la API
de `Allocator` ni el tracking.

## Fase 5 — API de `map` y resultados de inserción

### Trabajo

- [x] Separar `insert_outcome::{inserted, already_present}` de
      `map_error::{out_of_memory, capacity_overflow}`.
- [x] Tratar un mapa no inicializado como violación de contrato, no como OOM.
- [x] Migrar `map_init`, `reserve` e inserciones fallibles a `result` donde el
      caller pueda actuar sobre la causa.
- [x] Mantener `find` como puntero anulable, `contains` como predicado y
      `remove` como presencia/ausencia normal.
- [x] Añadir lookup heterogéneo para poder almacenar `str` y buscar con
      `strview`, sin crear texto propietario.
- [x] Añadir `try_emplace` para no construir `V` cuando la clave ya existe.
- [x] Instrumentar construcciones/movimientos para comprobar el ahorro real.
- [x] No cambiar todavía el layout de buckets ni el algoritmo de probing.

### Criterio de salida

La API ya no confunde duplicados con fallos, lookup heterogéneo no asigna y
`try_emplace` evita construir el valor en inserciones rechazadas.

### Commit esperado

`feat(collections): add typed and heterogeneous map operations`

### Reversión

Las extensiones de API son independientes del layout. Si `result` no es
adecuado para una operación status-only, se conserva un enum directo. Si el
lookup heterogéneo aumenta ambigüedad o code size sin consumidor, se revierte
ese overload sin afectar inserción ni `result`.

## Fase 6 — Metadata compacta y lookup escalar de `map`

### Diseño candidato

Separar lógicamente metadata de slots dentro de un backing storage:

```text
metadata: [fingerprint, probe_distance]...
slots:    [K, V][K, V][K, V]...
```

La primera versión será escalar. El fingerprint evita comparar claves que no
pueden coincidir y la distancia conserva la terminación Robin Hood. Si la
distancia elegida se satura, el mapa debe crecer o fallar transaccionalmente;
nunca trunca silenciosamente el valor.

### Trabajo

- [x] Medir `sizeof` efectivo del bucket actual para todos los tipos de la
      baseline.
- [x] Definir estados empty/full y fingerprint sin colisionar con valores de
      control.
- [x] Escoger `u8` o `u16` para distancia con pruebas de clusters extremos.
- [x] Eliminar el hash completo por slot sólo si recomputarlo en rehash resulta
      neutral o favorable globalmente.
- [x] Calcular sin overflow un único bloque con metadata, padding de alineación
      y slots. Mantener una allocation por tabla salvo que dos bloques demuestren
      una ventaja suficiente para justificar otro evento y otra liberación.
- [x] Reimplementar insert, find, remove, iteration y rehash preservando los
      contratos públicos.
- [x] Probar wrap-around, distancia máxima, colisiones, move-only, aliasing,
      fallos de cada allocation y backward-shift.
- [x] Medir hits/misses, claves pequeñas/grandes, tablas pequeñas/grandes y
      churn.

### Puerta específica

El candidato se acepta si cumple una de estas condiciones sin regresiones
materiales en las otras cargas:

1. Reduce al menos 20 % los bytes reservados en `AllocationKey`/
   `AllocationRecord` y al menos 40 % en `map<u64, u64>`, manteniendo velocidad
   neutral.
2. Mejora al menos 8 % lookup o inserción en una carga primaria y reduce memoria
   de manera medible.

Ninguna operación primaria puede empeorar más de 5 %, y tablas pequeñas no
pueden empeorar materialmente sólo para favorecer tablas grandes aún sin
consumidor.

### Commit esperado

`perf(collections): compact robin hood map metadata`

### Reversión

No se conservarán dos implementaciones en el árbol. Si el candidato falla la
puerta después de como máximo dos iteraciones controladas, se revierte el
commit de layout y se conservan las mejoras de API de la fase anterior. Los
benchmarks y el documento registran la hipótesis rechazada.

## Fase 7 — Filtrado SIMD opcional de `map`

Esta fase sólo comienza si la metadata separada y escalar fue aceptada.

### Trabajo

- [x] Conservar siempre un lookup escalar correcto.
- [x] Evaluar comparación agrupada de fingerprints con SSE2 en x86-64; al
      fallar la puerta en la primera plataforma baseline, no extender el
      candidato rechazado a NEON en AArch64.
- [x] Integrar la condición de distancia Robin Hood en la máscara o conservar
      una terminación escalar demostrablemente correcta.
- [x] Probar inicios no alineados, wrap-around, grupos parciales y capacidad
      mínima.
- [x] Medir por separado tablas de 0-16, 17-128 y más de 128 entradas.
- [x] Medir code size y tiempo de compilación de las instanciaciones reales.

### Puerta específica

- Mejora mínima de 8 % en lookup de tablas mayores a 128 entradas.
- Regresión máxima de 3 % en tablas pequeñas, inserción y remove.
- Sin requerir AVX2, SVE ni flags de CPU no presentes en la plataforma base.

### Commit esperado

Si se acepta: `perf(collections): vectorize map fingerprint lookup`.

Si se rechaza: `docs(performance): record map vector lookup findings` después
de revertir el experimento.

### Reversión

SIMD estará en un commit posterior al lookup escalar. Puede revertirse completo
sin cambiar layout, archivos serializados ni API. El fallback escalar no es un
flag experimental: es la implementación portátil obligatoria.

## Fase 8 — Capacidad y construcción directa de `dyarr`

### Trabajo

- [ ] Exponer `reserve` público con el mismo tracking de source location.
- [ ] Implementar `emplace_back` construyendo directamente en el slot final.
- [ ] Resolver y probar argumentos que aliasen elementos del propio `dyarr`
      durante crecimiento.
- [ ] Implementar `append(slice<const T>)` con una única reserva y operación por
      rango.
- [ ] Evaluar inserción por rango sólo si existe consumidor.
- [ ] Mantener crecimiento por factor 2 como default; medir 1,5 sólo como
      experimento, sin añadir una policy pública.
- [ ] Conservar `optional<T>` para `pop/remove`, donde vacío es normal.
- [ ] Evaluar `result<void, storage_error>` para reserve/emplace/append. El mapa
      no inicializado o un índice inválido siguen siendo contratos, no errores
      de storage.

### Puerta específica

- `emplace_back` debe eliminar al menos una construcción/movimiento temporal en
  el caso objetivo.
- `append` de POD debe superar el loop de pushes en al menos 10 % para bloques
  medianos/grandes y nunca reservar más veces.
- Ninguna operación existente puede empeorar materialmente.
- No se cambia el factor de crecimiento si la mejora de memoria no compensa las
  allocations y relocalizaciones adicionales en cargas reales.

### Commit esperado

`feat(collections): add direct and ranged dyarr growth`

### Reversión

Las operaciones son aditivas. Si una no supera su puerta, se elimina junto a
sus call sites sin tocar el núcleo actual. Un cambio experimental de factor de
crecimiento se revierte por separado y nunca comparte commit con `reserve` o
`emplace_back`.

## Fase 9 — Experimento de layout compacto de `str`

### Trabajo

- [ ] Registrar histograma de longitudes de strings propietarios en
      inicialización y carga de recursos.
- [ ] Diseñar una representación de 32 bytes que conserve `Allocator*`, al
      menos 23 caracteres inline, terminador nulo y heap con length/capacity.
- [ ] Rechazar cualquier diseño basado en punteros etiquetados o lectura de un
      miembro inactivo de unión.
- [ ] Mantener intacta la API y la sintaxis `str{allocator, texto}`.
- [ ] Probar transición SSO/heap, copy/move entre allocators, reserve, clear,
      asignación fallida y strings en cada frontera de capacidad.
- [ ] Medir comparación, append, move, iteración y densidad dentro de `arr` y
      `dyarr`.

### Puerta específica

- Reducir `sizeof(str)` de 48 a 32 bytes conservando al menos 23 caracteres SSO.
- No introducir allocations nuevas.
- No empeorar más de 3 % assign/append/move en las longitudes más frecuentes.
- El layout debe ser legal y mantenible en GCC, Clang y MSVC.

### Commit esperado

Si se acepta: `perf(text): compact allocator-aware string storage`.

Si se rechaza: `docs(performance): record compact string layout findings`
después de revertir el experimento.

### Reversión

Como la API no cambia, el commit de representación puede revertirse sin migrar
call sites. Si no se alcanza 32 bytes de forma portable, se conserva el layout
actual de 48 bytes y se documenta el motivo.

## Fase 10 — Integración, regresión completa y cierre

### Trabajo

- [ ] Auditar que no se haya convertido mecánicamente predicados y ausencias
      normales a `result`.
- [ ] Auditar todos los `result` ignorados y activar warnings adecuados.
- [ ] Confirmar que ningún error contiene texto propietario o allocator.
- [ ] Eliminar adapters y nombres temporales de APIs migradas.
- [ ] Ejecutar la matriz completa Debug, Release y ASan/UBSan.
- [ ] Ejecutar build aislada mediante Nix.
- [ ] Ejecutar el motor en Wayland/Niri durante al menos tres frames.
- [ ] Confirmar cero eventos de allocation tras el primer frame y cero fugas al
      cerrar.
- [ ] Comparar todos los benchmarks con las baselines de este ciclo y con la
      referencia histórica.
- [ ] Registrar resultados aceptados, rechazados y revertidos.
- [ ] Actualizar ejemplos y contratos públicos.

### Criterio de salida

La causa de los errores recuperables llega a un límite capaz de decidir; cada
optimización aceptada supera su puerta; no quedan feature flags experimentales,
APIs duplicadas ni implementaciones alternativas muertas.

### Commit esperado

`test(runtime): validate result and container performance contracts`

## Plan de regresión y reversión

### Estados permitidos

Cada elemento de seguimiento usa uno de estos estados:

- `pendiente`;
- `en progreso`;
- `aceptada`;
- `rechazada`;
- `revertida`;
- `bloqueada`.

Una fase no se marca `aceptada` sólo porque compile. Debe incluir evidencia
funcional y de rendimiento. `rechazada` significa que el experimento fue
evaluado y no entrará al producto; `revertida` significa que llegó a integrarse
temporalmente y después violó una puerta.

### Unidad de rollback

- Cada optimización vive en un commit distinto de sus benchmarks.
- Una migración de API no comparte commit con un cambio de layout.
- SIMD nunca comparte commit con metadata escalar.
- El cambio de factor de crecimiento nunca comparte commit con nuevas
  operaciones de `dyarr`.
- El layout de `str` no comparte commit con cambios semánticos de texto.
- Se usa `git revert` para preservar trazabilidad; no se reescribe ni resetea la
  historia compartida.

### Procedimiento ante una regresión

1. Detener nuevas migraciones sobre el componente afectado.
2. Reproducir contra el commit baseline y el candidato en el mismo entorno.
3. Clasificarla como corrección, lifetime, rendimiento, memoria, plataforma o
   complejidad.
4. Reducirla a un test o benchmark estable.
5. Permitir como máximo dos iteraciones de corrección acotadas.
6. Si sigue incumpliendo una puerta, revertir el commit de producto.
7. Ejecutar nuevamente tests y benchmarks sobre el árbol revertido.
8. Conservar el caso de prueba si representa un riesgo válido y documentar la
   hipótesis, datos y decisión.

### Matriz de rollback

| Cambio | Señal de regresión | Acción primaria | Qué se conserva |
| --- | --- | --- | --- |
| `result` base | lifetime, tamaño o success path inválido | revertir antes de migrar callers | tests/benchmark que detectó el problema |
| Migración File | ownership peor o logs duplicados | volver a salida explícita con error tipado | `result` y `file_error` |
| Migración renderer | hot path peor o estados confusos | revertir el límite afectado | errores de capas ya validados |
| Fast path allocator | alineación/tracking incorrecto o sin mejora | volver a `posix_memalign` | harness por tamaño/alineación |
| API de `map` | ambigüedad/code bloat | retirar overload concreto | layout Robin Hood existente |
| Metadata compacta | lookup/remove peor o complejidad excesiva | revertir layout | API tipada y heterogénea |
| SIMD | tablas pequeñas peores o plataforma fallida | revertir SIMD | metadata escalar compacta |
| `dyarr` aditivo | no elimina moves o aliasing complejo | retirar operación | núcleo actual de `dyarr` |
| Factor de crecimiento | más allocations o peor P95 | restaurar factor 2 | nuevas operaciones aceptadas |
| Layout de `str` | UB, portabilidad o hot path peor | restaurar 48 bytes | API y SSO actuales |

### Revisión posterior

Si una optimización aceptada provoca una regresión descubierta más adelante:

- se añade primero el workload que faltaba a la suite;
- se revierte el commit más pequeño que restablezca el contrato;
- no se cambia un umbral retroactivamente para justificar el resultado;
- sólo se reabre la optimización cuando exista una hipótesis distinta y una
  medición capaz de demostrarla.

## Matriz de seguimiento

| ID | Entrega | Estado | Commit | Evidencia/decisión |
| --- | --- | --- | --- | --- |
| R0 | Inventario y baseline ampliada | aceptada | `test(performance): capture error and container baselines` | [Evidencia](result-containers-phase-0-baseline.md) |
| R1 | `result<T, E>` y `result<void, E>` | aceptada | `feat(core): add allocation-free result values` | [Evidencia](result-phase-1-evidence.md) |
| R2 | File y shaders con errores tipados | aceptada | `refactor(io): propagate typed file failures` | [Evidencia](result-phase-2-io-evidence.md) |
| R3 | Renderer/Vulkan con outcomes y errores | aceptada | `refactor(renderer): distinguish frame outcomes and failures` | [Evidencia](result-phase-3-renderer-evidence.md) |
| O1 | Fast path de allocator raw | aceptada | `perf(memory): add a fast path for fundamental alignment` | [Evidencia](result-phase-4-allocator-fast-path.md) |
| O2 | API tipada y heterogénea de `map` | pendiente | — | — |
| O3 | Metadata compacta escalar de `map` | pendiente | — | — |
| O4 | Filtrado SIMD de `map` | pendiente | — | — |
| O5 | Capacidad y construcción directa de `dyarr` | pendiente | — | — |
| O6 | Layout compacto de `str` | pendiente | — | — |
| C1 | Integración y cierre | pendiente | — | — |

## Fuera de alcance

- Migrar a C++23 o usar `std::expected`.
- Incorporar una librería externa de `expected`.
- Excepciones, RTTI de errores o jerarquías polimórficas de errores.
- Mensajes dinámicos dentro de `result`.
- Pointer tagging o niche optimization dependiente de ABI.
- Un macro equivalente a `?` basado en extensiones GCC.
- Dos mapas públicos o sustitución inmediata de Robin Hood por Swiss Table/F14.
- Hash seleccionable, cambio de rapidhash o hash criptográfico.
- Nuevos contenedores, pools o handles.
- Ampliar trivial relocatability mediante traits opt-in no portables.
- Reducir `arr`/`slice` sin evidencia de consumo significativo.

## Definición de terminado

El ciclo termina cuando:

- `result` es allocation-free, nunca vacío y seguro para tipos triviales y
  move-only;
- File y renderer ya no pierden la causa de errores recuperables;
- los outcomes normales de swapchain/frame no se reportan como fallos;
- toda optimización aceptada posee comparación antes/después reproducible;
- toda optimización que incumplió sus puertas fue revertida y documentada;
- Debug, Release, ASan/UBSan, Nix y el smoke test Wayland/Niri pasan;
- el frame estable conserva cero allocations inesperadas;
- no quedan adapters temporales, flags experimentales ni código alternativo sin
  uso.
