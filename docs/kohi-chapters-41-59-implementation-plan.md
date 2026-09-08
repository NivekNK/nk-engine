# Plan de implementación de Kohi 41–59 en NK Engine

- Estado: completado; capítulos 41–59 adaptados
- Fecha de análisis: 2026-09-02
- Punto de partida de NK Engine: capítulos 34–40 adaptados; rama `feature/textures`

## Objetivo

Ordenar la historia de Kohi entre los capítulos 41 y 59, separar los commits que
quedaron mezclados o comprimidos, y convertirla en un plan implementable sobre
NK Engine. Este documento no propone copiar literalmente el motor en C: mantiene
las semánticas C++ de NK, sus allocators, `arr`, `dyarr`, `map`, `slice`, `str`,
`result`, sus sistemas con ownership explícito y su backend Linux Wayland nativo.

## Criterios de adaptación

- La referencia funcional es Kohi, pero la API y el ownership deben ser propios
  de NK Engine.
- No se portará código de macOS ni GLFW. En Linux, las pruebas visuales se harán
  primero sobre Wayland/xdg-shell en niri; XCB queda sólo como fallback existente.
- No se introducirán contenedores STL en runtime para reemplazar los contenedores
  propios. Se admiten utilidades estándar de lenguaje sin ownership dinámico
  oculto cuando ya forman parte de la base actual.
- Toda operación recuperable nueva debe devolver `nk::result<T, E>`; no se usarán
  excepciones.
- Una dependencia nueva sólo se agrega como submódulo fijado a un tag y se anota
  en `.scripts/libraries.csv`. No se anticipa ninguna dependencia adicional en
  estos capítulos.
- `tinyobjloader` ya existe como submódulo en el tag `v2.0.0rc13` y ya está
  registrado en `.scripts/libraries.csv`; el capítulo 55 debe reutilizarlo.
- Cada capítulo se cierra con build, tests, validación Vulkan y un smoke test en
  Wayland/niri cuando tenga salida gráfica.
- Los commits sugeridos son semánticos y describen el cambio. No deben mencionar
  el número del capítulo ni agrupar trabajo ajeno al alcance indicado.

## Resultado del análisis histórico

1. El vídeo 41 es una hoja de ruta y no tiene un commit funcional propio.
2. Los capítulos 42–44 poseen límites claros: free list, dynamic allocator y
   subasignación de buffers Vulkan.
3. Los capítulos 45–48 fueron publicados en `main` como un único squash,
   [`af39ef5`](https://github.com/travisvroman/kohi/commit/af39ef567da7f7b2607e1eca0a8d1eedc2fe01f5).
   Para poder implementarlos por separado, este plan recupera la secuencia del
   [PR #38](https://github.com/travisvroman/kohi/pull/38) y coloca los commits
   originales según contenido y fecha de publicación de los vídeos. Es una
   reconstrucción fundada, no una división oficial del squash.
4. Los capítulos 49–50 tienen ramas de trabajo y merges entrecruzados. Se toma el
   commit final de cada tema como referencia autoritativa y los anteriores como
   evidencia de desarrollo o correcciones.
5. Los capítulos 51–53 también aparecen como commits de desarrollo más un squash
   final. Sólo se implementará el estado final; los commits previos sirven para
   localizar correcciones, no para aplicar dos veces el mismo cambio.
6. Los capítulos 54–59 vuelven a tener límites funcionales claros.

### Segmentación resumida

| Capítulo | Inicio/fin funcional de referencia | Estado del límite |
|---|---|---|
| 41 | Sin cambio funcional; documentación `7abec4f` → `6dbb907` | Intervalo auxiliar, no implementación del vídeo. |
| 42 | `c4ae920`; fixes `185ae02` y `5f910b6` | Claro, con dos correcciones posteriores. |
| 43 | `351ac92` → `7973c53`; integración global `195c60a` | Claro en la rama de desarrollo. |
| 44 | `86e0dcf` | Claro; los commits circundantes son fixes/merges separados. |
| 45 | `e26d5e2` → `b18c0b8` dentro del PR #38 | Reconstruido por contenido y fecha. |
| 46 | `5af67df` dentro del PR #38 | Reconstruido; entrega puente de interfaz/backend. |
| 47 | `5a8d58d` → `ed28914` dentro del PR #38 | Reconstruido por contenido y fecha. |
| 48 | `0c36783` → `0ece16d`; squash final `af39ef5` | Reconstruido; el squash es el estado autoritativo. |
| 49 | `06575c3` → `6960d48`; final `bc05433`; math fix `532a8af` | Dos ramas convergentes; usar el final como autoridad. |
| 50 | `6a96cf6` → `a1cf8d8`; integración `0f6edd7`; fix `ce5970a` | Claro al considerar la integración con 49. |
| 51 | `490b042` + `2544589`; final `8d3a9d2` | El último es el squash autoritativo. |
| 52 | `5947269`; final `6e41e3d` | El último es el squash autoritativo. |
| 53 | `875c03f` + `ad134a7`; final `37e0d20` | El último es el squash autoritativo. |
| 54 | `58b3554` | Claro. |
| 55 | `6f5b979` | Claro; squash del PR #46. |
| 56 | `920035f` | Claro; squash del PR #50. |
| 57 | `879ccc4` | Claro; squash del PR #56. |
| 58 | `28817f7` | Claro; squash del PR #57. |
| 59 | `4ba9e70` | Claro; squash del PR #61. |

## Orden global y dependencias

```text
deuda retroactiva 35–40
        │
        ├─ 41 documentación/hoja de ruta
        │
        └─ 42 FreeList ── 43 FreeListAllocator ── 44 subasignación de Buffer
                                                    │
                                                    └─ 45 → 46 → 47 → 48 ShaderSystem
                                                                         │
                                                                         └─ 49 → 50 → 51 → 52 iluminación
                                                                                              │
                                                                                              └─ 53 → 54 → 55 → 56 meshes
                                                                                                                   │
                                                                                                                   └─ 57 → 58 → 59 texturas/targets
```

La dependencia dura del capítulo 44 es la estructura del 42, no necesariamente
el allocator general del 43. Esto permite que los buffers administren rangos de
GPU sin hacer que `Buffer` sea un allocator de memoria CPU.

## Paso previo — deuda retroactiva de los capítulos 35–40

Estos commits son anteriores al 42. No deben mezclarse artificialmente con el
41, pero sí auditarse antes de construir encima del renderer actual.

### Correcciones que sí deben evaluarse

| Referencia | Hallazgo | Acción en NK Engine |
|---|---|---|
| [`c760904`](https://github.com/travisvroman/kohi/commit/c7609045ba393605acf4f7f12d585438e5a028b5) | No todo dispositivo ofrece memoria host-visible y device-local para UBO. | Verificar selección de tipos de memoria y fallback host-visible/coherent. |
| [`9f2d315`](https://github.com/travisvroman/kohi/commit/9f2d31585d0a666c808ec9664bb1acacdfb20979) | Dereferencia nula al fallar la carga de un recurso. | Añadir regresión de argumentos/salidas nulas en `ResourceSystem`. |
| [`e266181`](https://github.com/travisvroman/kohi/commit/e266181807360c3b1cf0fbc83486e5ad1ac9406a) | Se reservaba GeometrySystem con el tamaño de MaterialSystem. | Auditar que cada sistema use su tamaño y allocator correctos. |
| [`63994f4`](https://github.com/travisvroman/kohi/commit/63994f4d8472789d45cd8157b0997cc6270c4e35) | Estado de plataforma sin inicializar. | Verificar inicialización total del backend Wayland y del fallback XCB. |
| [`8c1b685`](https://github.com/travisvroman/kohi/commit/8c1b685fc1b2f16338aaac52d31605c3850dfc2b) | Build Linux y stride de UBO de instancia alineado a 256 bytes. | Ignorar el build script antiguo, pero calcular stride con `minUniformBufferOffsetAlignment`. |
| [`c126dec`](https://github.com/travisvroman/kohi/commit/c126dec11b8747544b564f7fbb5b7fa0d282cc21) | Actualización de descriptor después de bind. | Asegurar orden update-before-bind y añadir validación Vulkan. |
| [`185ae02`](https://github.com/travisvroman/kohi/commit/185ae02c2be3ecdd728128ab385c9dcfa84d41fb) | Cálculo incorrecto de capacidad de metadata de la free list. | Se absorbe como regresión obligatoria del capítulo 42. |
| [`c176d1b`](https://github.com/travisvroman/kohi/commit/c176d1bd21b067d030b8ec0a0386d04b3be740ff) | El render pass UI no actualizaba su área al redimensionar. | Probar resize de world y UI bajo Wayland. |
| [`c50e824`](https://github.com/travisvroman/kohi/commit/c50e82498bdf38e55c38ae076a5505984cd47c2f) | Selección de colas Vulkan subóptima. | Preferir una familia compartida graphics/present y conservar fallback separado. |
| [`805a53b`](https://github.com/travisvroman/kohi/commit/805a53b60cd287ddc0fe0fd3b5518825cf7f335a) | Doble indirección incorrecta de `VkFence`. | Verificar los tipos y lifetime de fences por imagen. |
| [`2cbd684`](https://github.com/travisvroman/kohi/commit/2cbd684787246681f1a37d670d925fde077bfcf2) | Descriptor sets fijados a tres imágenes. | Dimensionar siempre con el image count real del swapchain. |
| [`de996b0`](https://github.com/travisvroman/kohi/commit/de996b0b926c66e11bd23417e41d36181d5df273) | Compilación más estricta destapó tipos e inicializaciones incorrectas. | Mantener `-Wall -Wextra -pedantic-errors -Wvla` y corregir, no silenciar, warnings nuevos. |
| [`ce5970a`](https://github.com/travisvroman/kohi/commit/ce5970a3b06f0fd478453f5d0abf2d3b322107ad) | `SPECULAR` repetía el bit de `DIFFUSE` y había una conversión angular incorrecta. | Se absorbe en capítulos 49–51: enums únicos y convención angular consistente. |

### Criterio de cierre previo

- [ ] Cada corrección aplicable está cubierta por código actual o por un test de
  regresión nuevo.
- [ ] Validation Layers no reporta errores en inicio, resize, dibujo UI y cierre.
- [ ] Si se requiere corregir código, usar commits pequeños como
  `fix(renderer): respect swapchain image counts in descriptor state` y
  `fix(resources): preserve failure-state invariants`.

## Capítulo 41 — Roadmap and Series Plans

- Vídeo: [Kohi #041](https://youtu.be/SS8Zn13cZus?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj)
- Commit funcional: ninguno.
- Estado NK: completado documentalmente en
  [`renderer-resource-lifetime-contracts.md`](renderer-resource-lifetime-contracts.md).
- Lote documental cronológicamente asociado:
  [`7abec4f`](https://github.com/travisvroman/kohi/commit/7abec4fd52902697ad525a87a9c874d271ef423a),
  [`642fe9f`](https://github.com/travisvroman/kohi/commit/642fe9f2413a5ecdc2925bac067de0c0dea60e51),
  [`52a229e`](https://github.com/travisvroman/kohi/commit/52a229ec0b9441fed456fd8c16a83a68b02beed9),
  [`b9c4e87`](https://github.com/travisvroman/kohi/commit/b9c4e876fb29736cf170b7740c56fdd21a8060ba),
  [`1dce8dd`](https://github.com/travisvroman/kohi/commit/1dce8dd605c02a528157defd43afd5c4ac08aa76) y
  [`6dbb907`](https://github.com/travisvroman/kohi/commit/6dbb9073ec10288da35d61183bdf238880a2fce3).

### Plan

- [x] No crear una implementación ficticia para este capítulo.
- [x] Revisar el roadmap sólo para confirmar el orden conceptual 42–59.
- [x] Establecer el documento vivo para contratos de las APIs nuevas; no portar
  Doxygen ni comentarios que describan APIs C inexistentes en NK.
- [x] Cerrar mediante un commit documental únicamente si se cambia documentación,
  por ejemplo `docs(engine): document renderer resource ownership`.

### Resultado

- Se confirmó la progresión conceptual
  allocator/rangos → buffers → shaders → iluminación → meshes → texturas/targets.
- Se documentaron el orden de construcción y destrucción, ownership, referencias,
  handles prestados, rollback y límites de threading del estado actual.
- Los requisitos de lifetime que deberán preservar los capítulos 42–59 quedaron
  registrados sin introducir por adelantado ninguna de sus APIs.

## Capítulo 42 — Free List

- Vídeo: [Kohi #042](https://youtu.be/sP7xRUyP3e0?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj)
- Referencia principal: [`c4ae920`](https://github.com/travisvroman/kohi/commit/c4ae9202dbd2ebd2dbb3af2f5c4fb3efd8836bed)
- Fix temprano: [`185ae02`](https://github.com/travisvroman/kohi/commit/185ae02c2be3ecdd728128ab385c9dcfa84d41fb)
- Fix tardío obligatorio: [`5f910b6`](https://github.com/travisvroman/kohi/commit/5f910b6811b5c1e50f6961df30a6e3c5a1ad4506)
- Merge de procedencia: [`a89c2df`](https://github.com/travisvroman/kohi/commit/a89c2df7b5ce6dc99b8d8d025c9237b0119fb1a2)
- Estado NK: completado en `cfcd45e` y documentado en
  [`memory-allocation-contracts.md`](memory-allocation-contracts.md).

### Diseño NK

- [x] Crear `nk::mem::FreeList` como administrador de rangos contiguos, no como
  contenedor público dentro de `nk::cl`.
- [x] Modelar cada rango con `u64 offset` y `u64 size`; usar aritmética comprobada
  en toda suma, resta y alineación.
- [x] Recibir el allocator de metadata explícitamente al inicializar. No depender
  de `MemorySystem` global ni reservar memoria durante `reserve`/`release`.
- [x] Ofrecer `reserve(size, alignment) -> result<MemoryRange, free_list_error>`,
  `release(range)`, `resize(new_size)` y consultas de espacio libre/usado.
- [x] Mantener rangos ordenados por offset, fusionar vecinos por ambos lados,
  rechazar double-free, solapamientos, rangos cero y rangos fuera de límites.
- [x] No copiar la fórmula defectuosa de metadata de Kohi. La capacidad mínima
  para regiones pequeñas debe derivarse y probarse expresamente; el fix tardío
  de Kohi que fuerza 20 entradas se trata como caso de regresión, no como número
  mágico obligatorio.

### Validación y commits

- [x] Tests: primera/mejor región disponible según política elegida, fragmentación,
  coalescing, alineación, región completa, OOM, overflow, resize grow/shrink,
  metadata mínima, double-free y secuencias aleatorias contra un modelo simple.
- [x] Commit funcional y de regresión:
  `feat(memory): add contiguous free-range management`.

### Resultado

- La metadata tiene capacidad fija, se obtiene en una sola reserva desde el
  allocator explícito y no depende del estado global de `MemorySystem`.
- La política implementada es first-fit por offset entre candidatos que pueden
  representarse con la metadata disponible. Las operaciones fallidas no mutan
  rangos ni contadores.
- Se cubrieron 16 escenarios específicos, incluida la región mínima de un byte,
  fragmentación, fusión bilateral, alineación, agotamiento de metadata y 2.000
  operaciones deterministas contrastadas con un modelo byte a byte.
- La suite completa pasó 146/146 tanto en Debug como con ASan/UBSan; LeakSanitizer
  se desactivó para la suite instrumentada porque el entorno de ejecución usa
  `ptrace`.

## Capítulo 43 — Dynamic Allocator

- Vídeo: [Kohi #043](https://youtu.be/BSBFBWwG8Ds?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj)
- Desarrollo:
  [`351ac92`](https://github.com/travisvroman/kohi/commit/351ac9256fa7c40ecda5eca08789e68004c250c2),
  [`5a030e1`](https://github.com/travisvroman/kohi/commit/5a030e12820a59ef3758a751c8b117e79f2155b3) y
  [`f977156`](https://github.com/travisvroman/kohi/commit/f9771567781bf67b0bc861218b06d9d9d467183f)
- Integración global: [`195c60a`](https://github.com/travisvroman/kohi/commit/195c60a1fbaf2b7abe797a98ce8529d232d14bd0)
- Corrección final: [`7973c53`](https://github.com/travisvroman/kohi/commit/7973c536d97cef8c9b7ef176619fc27e0d62a19e)
- Documentación: [`293f5a1`](https://github.com/travisvroman/kohi/commit/293f5a10b0f5c519d8181f4bd70bc9f8a52f26ad) y
  [`dccf6f8`](https://github.com/travisvroman/kohi/commit/dccf6f8a722785a27f9c47576bde25a76fe17986)
- Merges de procedencia: [`fa5ca8e`](https://github.com/travisvroman/kohi/commit/fa5ca8eaa0504ffc241543f37b7b514b49cb1133) y
  [`6c69e93`](https://github.com/travisvroman/kohi/commit/6c69e933208e9ab4bfa33cae34274279c05df731)

### Diseño NK

- [x] Comparar el comportamiento requerido con `Allocator`, `MallocAllocator`,
  `LinearAllocator`, `AllocatorOwner` y el tracking ya implementado.
- [x] No reemplazar el sistema global por la implementación C de Kohi. Añadir
  `nk::mem::FreeListAllocator` sólo para el caso que falta: un heap fijo capaz de
  liberar y reutilizar bloques.
- [x] Hacerlo derivar de `Allocator`, conservar tracking, alineación, estadísticas,
  ownership/move y macros de source location existentes.
- [x] Usar `FreeList` del capítulo 42 para los rangos y almacenar un header mínimo
  sólo si es imprescindible para validar `free`; evitar búsquedas o metadata
  redundante en el fast path.
- [x] Mantener el arranque de `MemorySystem` actual. El backing store y la metadata
  se inyectan al `init`, eliminando una dependencia circular.
- [x] Si el allocator actual ya cubre todo el uso real, cerrar el capítulo como
  auditoría y tests, sin crear una clase redundante.

### Validación y commits

- [x] Tests: distintas alineaciones, reuse, fragmentación, OOM, tamaño cero,
  overflow, free inválido, tracking balanceado, move y shutdown.
- [x] Benchmark separado frente a `MallocAllocator` para cargas repetidas; no usar
  el benchmark como sustituto de las pruebas de corrección.
- [x] Commits: `f7abafa feat(memory): add a reusable free-list allocator` y
  `f5bb8d5 perf(memory): benchmark reusable pool allocations`.

### Resultado

- `FreeListAllocator` admite backing propio o prestado, deriva de `Allocator` y
  reutiliza bloques mediante la `FreeList` existente. Su único coste por bloque
  es un guard de 8 bytes que valida dirección, offset y tamaño incluso sin
  tracking.
- `FreeList` ahora es movable y permite un sesgo de alineación; así el allocator
  devuelve direcciones absolutas alineadas sin desperdiciar permanentemente el
  prefijo de cada rango.
- El backing y la metadata se inyectan explícitamente. Tras `init`, los caminos
  `allocate`/`free` no solicitan memoria a los allocators padre y no introducen
  una dependencia de bootstrap con `MemorySystem`.
- Se cubrieron 18 tests de `FreeList` y 12 de `FreeListAllocator`. La suite pasó
  160/160 en Debug y con ASan/UBSan, y 151/151 en Release; LeakSanitizer se
  desactivó bajo el entorno instrumentado por su uso de `ptrace`.
- En tres ejecuciones Release aisladas de 200.000 ciclos allocate/free, la
  mediana del pool permaneció entre 19,097 y 19,205 ns/op. `MallocAllocator`
  obtuvo 12,071–12,452 ns/op para 16 B, 15,453–17,660 para 256 B y
  54,547–57,362 para 4 KiB. El resultado justifica conservar ambos: no se migra
  el allocator global y el pool queda disponible para dominios donde la
  reutilización, el límite fijo o bloques mayores compensen su metadata.

## Capítulo 44 — Dynamic Vulkan Buffers

- Vídeo: [Kohi #044](https://youtu.be/SNDJ-rGJd5A?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj)
- Referencia principal: [`86e0dcf`](https://github.com/travisvroman/kohi/commit/86e0dcf64808e609bb244f5d90298b9640704344)
- Merges de procedencia: [`50c4ba6`](https://github.com/travisvroman/kohi/commit/50c4ba60e8e52dce10e1a76ebaaa443f4c8aaf25) y
  [`d64730b`](https://github.com/travisvroman/kohi/commit/d64730bc775c5ffc391eefee47d49cc59f838f7e)
- Estado NK: completado en `585733a`, `dc3c988` y `7b328ca`.

### Plan

- [x] Integrar una `FreeList` en cada `Buffer` que admita subasignación; no hacer
  que `Buffer` herede del allocator de CPU.
- [x] Sustituir `m_geometry_vertex_offset` y `m_geometry_index_offset` por rangos
  reservados con alineación comprobada.
- [x] Añadir `reserve`, `release` y `resize` fallibles al buffer, preservando datos
  y offsets durante crecimiento.
- [x] Hacer la carga de geometría transaccional: si falla índices, staging o copy,
  devolver también el rango de vértices y dejar el slot de geometría intacto.
- [x] Liberar ambos rangos al destruir o reemplazar geometría. Evitar
  `vkDeviceWaitIdle` como solución general; documentar la sincronización necesaria
  antes de reutilizar rangos todavía en vuelo.
- [x] Absorber los fixes retroactivos de image count, fences, queue families y
  resize antes de dar por estable la nueva ruta.

### Validación y commits

- [x] Unit tests sin Vulkan para lifecycle, alineación, espacios independientes,
  resize, ausencia de allocations tras init y 256 ciclos de reservas/liberaciones
  que vuelven a reutilizar los mismos offsets.
- [x] Smoke test con Validation Layers y resize repetido bajo niri.
- [x] Commits: `585733a feat(renderer): add buffer range suballocation`,
  `dc3c988 feat(renderer): recycle geometry buffer ranges` y
  `7b328ca test(renderer): isolate stable frame allocation checks`.

### Resultado

- `BufferSuballocator` mantiene la política de offsets fuera de Vulkan, recibe
  explícitamente su allocator de metadata y sólo se activa en los buffers de
  vértices e índices que la necesitan. Staging y UBO no reservan esa metadata.
- `Buffer::resize` prepara, enlaza y copia el nuevo almacenamiento antes de
  modificar la free list o destruir el buffer anterior. Los errores devuelven
  `renderer_error` y preservan el estado previo.
- La geometría ya no consume offsets monotónicos: reserva rangos independientes,
  publica el slot sólo tras cargar vértices e índices, revierte fallos y recicla
  los rangos al reemplazar o destruir después de sincronizar la cola gráfica.
- Los fixes asociados ya estaban representados por el diseño NK y se auditaron:
  descriptor sets, command buffers, framebuffers, fences por imagen y sus arrays
  usan el image count real; las familias de cola se deduplican; las áreas world y
  UI se actualizan al recrear el swapchain.
- Se añadieron 6 tests y la suite completa pasó 166/166 en Debug y con
  ASan/UBSan, y 157/157 en Release. El smoke nativo Wayland/xdg-shell bajo niri
  completó cuatro recreaciones de swapchain con Validation Layers, cero
  allocations en 25.652 frames renderizados estables y cero fugas al cerrar.
- No se añadieron dependencias externas.

## Capítulo 45 — Shader System, parte 1

- Vídeo: [Kohi #045](https://youtu.be/wXLsGqck100?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj)
- Squash común 45–48: [`af39ef5`](https://github.com/travisvroman/kohi/commit/af39ef567da7f7b2607e1eca0a8d1eedc2fe01f5)
- Tramo reconstruido del PR #38:
  [`e26d5e2`](https://github.com/travisvroman/kohi/commit/e26d5e2127976ffae53748535d84b346db12c1c9),
  [`28dc2aa`](https://github.com/travisvroman/kohi/commit/28dc2aaa53e314251652425d847f72616c52a2b4),
  [`f476910`](https://github.com/travisvroman/kohi/commit/f476910ebac0e8f3333539d1faf0361f9e8482bb),
  [`3be3fe4`](https://github.com/travisvroman/kohi/commit/3be3fe43261ac7572af1f2aa6abafce0d73d525a),
  [`37318ae`](https://github.com/travisvroman/kohi/commit/37318ae397bf400617125e6931b09769f0d5492b),
  [`a1e5bea`](https://github.com/travisvroman/kohi/commit/a1e5bea9fe0a1f47d60ee6bfb0dd7b115dd329d3),
  [`194a37a`](https://github.com/travisvroman/kohi/commit/194a37add490480b981cb42607ba7ad348a1ed66),
  [`ce91bf4`](https://github.com/travisvroman/kohi/commit/ce91bf4449d7e257104e58f17b8606864ff2e9a7) y
  [`b18c0b8`](https://github.com/travisvroman/kohi/commit/b18c0b810c28f214b6df64399e67653a06e6b3c9).

### Plan

- [x] Extraer de `MaterialShader` un `Shader` backend configurable sin romper aún
  la ruta pública existente.
- [x] Modelar etapas, atributos, descriptor layouts, uniformes, samplers y push
  constants con enums/structs tipados y containers propios.
- [x] Mantener RAII y shutdown idempotente para módulos, layouts, pools, pipeline y
  buffers. Cualquier init parcial debe hacer rollback completo.
- [x] Permitir layouts 2D y 3D sin clases Vulkan duplicadas para material y UI.
- [x] No crear todavía el registro global de shaders: esta entrega termina con un
  objeto backend configurable probado directamente.

### Validación y commit

- [x] Tests de validación de configuración y rollback; renderizar los shaders
  built-in actuales mediante la nueva abstracción.
- [x] Commit sugerido: `refactor(renderer): introduce configurable Vulkan shaders`.

### Estado NK

- Completado en `299dcd7` y `bcc3d71`.
- `ShaderConfig` define y valida etapas gráficas, atributos, descriptor sets,
  uniformes, samplers y push constants sin incluir tipos Vulkan. La validación
  detecta etapas/locations/bindings/nombres duplicados, rangos fuera de bounds,
  atributos y push constants solapados, scopes incompatibles y uso de etapas no
  cargadas.
- `VulkanShader` materializa esa configuración en módulos, layouts, un pool
  dimensionado desde el image count real, UBOs con stride alineado a
  `minUniformBufferOffsetAlignment`, pipeline y push constants. Cada fallo de
  inicialización converge en `shutdown()`, que es idempotente y respeta el orden
  inverso de dependencias.
- `MaterialShader` quedó como adaptador temporal de materiales; world y UI usan
  el mismo backend con `Vertex3D`/depth y `Vertex2D`/sin depth respectivamente.
  No se adelantó el registro público ni la resolución por nombre de los capítulos
  siguientes.
- Se añadieron 8 tests del contrato de configuración. Pasaron 174/174 en Debug y
  Debug con ASan/UBSan, y 165/165 en Release. El smoke nativo Wayland/xdg-shell
  bajo niri renderizó ambos built-ins durante 5 frames con Validation Layers,
  cero allocations en los 4 frames estables comprobados y cero fugas al cerrar.
- No se añadieron dependencias ni assets externos.

## Capítulo 46 — Shader System, parte 2

- Vídeo: [Kohi #046](https://youtu.be/T8-Tv4UsKCk?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj)
- Squash común: [`af39ef5`](https://github.com/travisvroman/kohi/commit/af39ef567da7f7b2607e1eca0a8d1eedc2fe01f5)
- Límite reconstruido: [`5af67df`](https://github.com/travisvroman/kohi/commit/5af67df0060cda7ea49ea9bb19f1a3ea7eb8f36f)

### Plan

- [x] Definir la interfaz renderer-neutral para create/destroy/use, bind globals,
  bind instance, apply y set uniform.
- [x] Separar handles/IDs públicos de los punteros Vulkan internos.
- [x] Compactar metadata con anchos comprobados, sin truncar tamaños u offsets;
  representar uniformes custom de forma explícita.
- [x] Mover la creación temporal del shader fuera del frontend y preparar la
  entrada del sistema del capítulo 47.
- [x] Mantener el pipeline asociado a un render pass compatible, no a nombres
  hardcodeados.

### Validación y commit

- [x] Probar que material y UI usan la misma interfaz sin alterar el orden de sus
  render passes.
- [x] Commit sugerido: `refactor(renderer): expose backend-neutral shader operations`.

### Estado NK

- Completado en `7f0ba49` y `b3cb0c1`.
- La API de `Renderer` expone create/destroy/use, bind/apply global e instance,
  adquisición/liberación de instance IDs, samplers y setters tipados o custom.
  `ShaderHandle` ocupa 4 bytes y combina índice con generación; Vulkan conserva
  los punteros y rechaza handles inválidos, obsoletos o usados fuera del render
  pass compatible.
- `ShaderUniformMetadata` ocupa 8 bytes. La conversión de cantidad, offset,
  tamaño y binding se comprueba antes de reducirla a 16 bits, y los uniformes
  custom conservan su tamaño explícito. Un fallo de validación o asignación no
  publica metadata parcial.
- La creación temporal de los built-ins permanece en `VulkanRenderer`, detrás
  de la API neutral y fuera del frontend. `MaterialShader` sigue siendo un
  adaptador interno temporal, previsto para retirarse al finalizar el sistema
  de shaders, no una segunda API pública.
- World y UI ejecutan el mismo protocolo neutral en el orden use, globals,
  instance, local y draw. Un test de backend fake comprueba tanto el protocolo
  como el orden world → UI.
- Pasaron 178/178 tests en Debug y en Debug con ASan/UBSan, y 169/169 en Release.
  El smoke nativo Wayland/xdg-shell bajo niri renderizó 5 frames con Validation
  Layers, cero allocations en los 4 frames estables y cero fugas al cerrar.
- No se añadieron dependencias ni assets externos.

## Capítulo 47 — Shader System, parte 3

- Vídeo: [Kohi #047](https://youtu.be/4E0v_5Wva-8?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj)
- Squash común: [`af39ef5`](https://github.com/travisvroman/kohi/commit/af39ef567da7f7b2607e1eca0a8d1eedc2fe01f5)
- Tramo reconstruido:
  [`5a8d58d`](https://github.com/travisvroman/kohi/commit/5a8d58dc4ca48bd7edbdec5e567f196148577923),
  [`ef36b50`](https://github.com/travisvroman/kohi/commit/ef36b504e2aee0aff05e73d0757a2a43f924f287),
  [`67ffde7`](https://github.com/travisvroman/kohi/commit/67ffde776459ac3e791a1667ca4c25015bf954b3),
  [`1f5f0d1`](https://github.com/travisvroman/kohi/commit/1f5f0d1d85c5ae6f101afea3017e2fc41538a595) y
  [`ed28914`](https://github.com/travisvroman/kohi/commit/ed28914f27760e748f941380513c84b8a2934ac1).

### Plan

- [x] Crear `ShaderResourceLoader` sobre la interfaz virtual actual de
  `ResourceLoader`.
- [x] Definir un formato de configuración de shader propio, con versión y errores
  de parseo tipados. Puede conservar el concepto `.shadercfg`, no su parser C.
- [x] Crear `ShaderSystem` con lookup por nombre e ID usando `nk::cl::map<str, ...>`
  y storage estable; definir ownership y límites desde config.
- [x] Implementar create/use/bind/apply y lookup de uniformes sin construir `str`
  temporales durante cada draw; aceptar `strview`.
- [x] Resolver scopes global, instance y local, junto con ubicación, tamaño,
  offset, array length y sampler index.
- [x] Toda carga fallida debe devolver `result` y liberar shader/recursos parciales.

### Validación y commit

- [x] Tests del loader, nombres duplicados, límites, scopes, tipos inválidos,
  lookup heterogéneo y shutdown.
- [x] Commits sugeridos: `feat(resources): load declarative shader configs` y
  `feat(renderer): add managed shader resources`.

### Estado NK

- El commit `3449782` añadió el loader integrado a `ResourceSystem`, un formato
  `.shadercfg` estricto y versionado, errores de parseo tipados, arrays de
  uniformes y los configs declarativos world/UI. El backend ya crea ambos
  pipelines desde estos recursos y descarga siempre el payload temporal.
- El commit `a2bc13d` añadió `ShaderSystem` con capacidad fija, handles estables,
  lookup por nombre con `nk::cl::map`, metadata compacta propia y la ruta
  create/load/destroy/use/bind/apply/uniform/sampler. Los límites y fallos del
  loader o renderer hacen rollback antes de publicar estado.
- `ShaderSystem` queda por ahora como componente explícitamente caller-owned que
  toma prestados `Renderer` y `ResourceSystem`. Integrarlo en `Engine`, transferir
  los shaders built-in y migrar `MaterialSystem` se mantiene como trabajo del
  capítulo 48; así no conviven prematuramente dos dueños del mismo shader.
- Pasaron 185/185 tests en Debug y en Debug con ASan/UBSan, y 176/176 en Release.
  El smoke nativo Wayland/xdg-shell bajo niri renderizó 5 frames con Validation
  Layers, cero allocations en los 4 frames estables y cero fugas al cerrar.
- No se añadieron librerías ni assets externos; los únicos assets nuevos son las
  configuraciones declarativas propias de los dos shaders integrados.

## Capítulo 48 — Finalizing the Shader System

- Vídeo: [Kohi #048](https://youtu.be/9nP3aBsrCXs?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj)
- Squash/final autoritativo: [`af39ef5`](https://github.com/travisvroman/kohi/commit/af39ef567da7f7b2607e1eca0a8d1eedc2fe01f5)
- Tramo reconstruido:
  [`0c36783`](https://github.com/travisvroman/kohi/commit/0c3678363cbec2473af66a9a1c54f001e492669f),
  [`364cccf`](https://github.com/travisvroman/kohi/commit/364cccf95ded6ea511b9a025526bd3129e79e2a0),
  [`c3cfbdd`](https://github.com/travisvroman/kohi/commit/c3cfbdd7fe1b1b153b6b65998a43fc6030ce15c5),
  [`0c43dcf`](https://github.com/travisvroman/kohi/commit/0c43dcf03a3178082e948d84a456bdb0a758b025),
  [`ae39c3a`](https://github.com/travisvroman/kohi/commit/ae39c3a52db1681bc572ffe5df4b963a7449f6d1),
  [`fee4164`](https://github.com/travisvroman/kohi/commit/fee4164b06a59a8d50418a28c05aacef6456df84) y
  [`0ece16d`](https://github.com/travisvroman/kohi/commit/0ece16d359efc4d3eb9b29a30ee6286d709e003d).
- Los reverts de settings `b8bec4c` y `36f5111` no tienen implementación.

### Plan

- [x] Completar set/update de uniformes y el binding global/instance/local.
- [x] Calcular strides UBO desde las propiedades físicas del dispositivo y validar
  todos los offsets antes de map/copy.
- [x] Migrar MaterialSystem y los shaders world/UI al `ShaderSystem` genérico.
- [x] Eliminar `MaterialShader` únicamente cuando ambas rutas hayan pasado el
  mismo smoke test; no conservar dos implementaciones activas.
- [x] Aplicar cada material una vez por frame cuando no cambie, pero invalidar el
  cache ante cambios de generación, shader, mapa o descriptor.
- [x] Cubrir destrucción, reload y cierre en orden inverso de dependencias.

### Validación y commits

- [x] Render world/UI, cambio de textura, resize y cierre con Validation Layers.
- [x] Commits sugeridos: `feat(renderer): complete shader uniform binding` y
  `refactor(materials): route materials through the shader system`.

### Estado NK

- Completado en `62c2b62` y `2d0c891`.
- `VulkanShader` es ahora la única implementación de shaders. Posee metadata,
  storage CPU, UBOs global/instance con stride alineado desde las propiedades
  físicas, descriptor layouts/pools/sets, samplers simples o en array y push
  constants locales. Los rangos y multiplicaciones se comprueban antes de todo
  map/copy, y la antigua clase `MaterialShader` fue eliminada.
- `Engine` posee `ShaderSystem`, carga los built-ins después de `TextureSystem` y
  antes de `MaterialSystem`, y destruye Geometry → Material → Shader → Texture →
  Renderer. `Renderer` ya no crea materiales ni es dueño de los shaders world/UI.
- Los `.kmt` declaran su shader, con fallback compatible según `MaterialType`.
  `MaterialSystem` resuelve los uniformes una vez, adquiere/libera instancias por
  `ShaderSystem` y mantiene el protocolo global/instance/local para world y UI.
- El cache de material evita reescribir UBO/descriptores más de una vez por frame
  cuando no hay cambios, pero siempre enlaza el descriptor requerido por el draw.
  Se invalida por frame, generación del material o textura, shader e instance ID.
- Pasaron 189/189 tests en Debug y en Debug con ASan/UBSan, y 180/180 en Release.
  El build puro `nix build --offline .#nk-engine --no-link` también pasó.
- El smoke nativo Wayland/xdg-shell bajo niri renderizó world/UI durante 5 frames
  con Validation Layers, cero allocations en los 4 frames estables y cero fugas.
  Una segunda ejecución cambió la ventana de 936×999 a 700×999 y luego 700×720,
  ejercitó los frames de recreación del swapchain y cerró sin errores de Vulkan.
- No se añadieron librerías ni assets externos; sólo se extendió el metadata de
  los materiales existentes.

## Capítulo 49 — Directional Lighting

- Vídeo: [Kohi #049](https://youtu.be/gXMMrPgsAas?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj)
- Referencia final: [`bc05433`](https://github.com/travisvroman/kohi/commit/bc05433030703151b5cdcb472de35f234510ee6f)
- Desarrollo/correcciones:
  [`06575c3`](https://github.com/travisvroman/kohi/commit/06575c31f930bcbd08408991ad4a3e77c3043e05),
  [`6960d48`](https://github.com/travisvroman/kohi/commit/6960d48b056b310e45f5e52163ad712682febc90) y
  [`532a8af`](https://github.com/travisvroman/kohi/commit/532a8af4ba4a3501ab204d9d151c6063a4e9b925)
- Merges de integración: [`3d76931`](https://github.com/travisvroman/kohi/commit/3d76931b339f79d7f484b6723669c338c7768c34) y
  [`0f6edd7`](https://github.com/travisvroman/kohi/commit/0f6edd7daad0600511aa2ec6ad55643101c6b4a5)
- Estado NK: completado en `95e2cd5` y `9f8c6bc`; convenciones en
  [`renderer-coordinate-conventions.md`](renderer-coordinate-conventions.md).

### Plan

- [x] Añadir normal a `Vertex3D` y actualizar layout, generación de planos/cubos y
  cargas existentes.
- [x] Implementar generación/normalización segura de normales y probar winding,
  degenerados y transformaciones no uniformes.
- [x] Definir `DirectionalLight` renderer-neutral y su contrato de uniformes.
- [x] Extender el shader material con ambient + diffuse direccional y mantener
  color/textura/alfa del material.
- [x] Fijar una convención documentada para handedness, orden de matrices,
  dirección de luz y ángulos. El commit de math se usa como regresión.
- [x] No incorporar assets si los PNG ya presentes permiten demostrar el resultado.

### Validación y commits

- [x] Tests matemáticos de normales y layout CPU/GPU; smoke con una malla rotando.
- [x] Commits sugeridos: `feat(geometry): generate vertex normals` y
  `feat(renderer): add directional material lighting`.

### Resultado

- `Vertex3D` usa un layout de 32 bytes y `GeometrySystem` genera normales
  area-weighted sin allocations temporales, valida la topología antes de mutar y
  omite triángulos degenerados sin producir NaN. El cubo usa 24 vértices y 36
  índices para conservar normales planas por cara.
- `SceneLighting` viaja en `RenderPacket`; `MaterialSystem` resuelve los uniformes
  una sola vez y sólo los aplica al shader world. UI permanece sin cambios.
- La normal matrix se calcula una vez por draw en CPU y se envía junto a model en
  128 bytes de push constants. Es correcta con escalas no uniformes y devuelve
  cero ante transformaciones singulares.
- El shader conserva RGB y alfa de material/textura, y agrega ambient más diffuse
  direccional con normalización segura. No se añadieron librerías ni assets.
- Pasaron 194/194 tests en Debug y con ASan/UBSan, y 185/185 en Release. El smoke
  Wayland/xdg-shell bajo niri completó 120 frames con Validation Layers, cero
  eventos de allocation en los 119 frames estables y cero fugas reportadas.

## Capítulo 50 — Specular Lighting

- Vídeo: [Kohi #050](https://youtu.be/9O--xSmf5NU?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj)
- Desarrollo:
  [`6a96cf6`](https://github.com/travisvroman/kohi/commit/6a96cf6c26e1d597beae0d981d5ce611f91b5f8b),
  [`f88fc3f`](https://github.com/travisvroman/kohi/commit/f88fc3fff260cd8cbb022b258c805f2779d89ecb) y
  [`a1cf8d8`](https://github.com/travisvroman/kohi/commit/a1cf8d87725089a1a1d537d13966bfca7819a570)
- Integración con 49: [`3d76931`](https://github.com/travisvroman/kohi/commit/3d76931b339f79d7f484b6723669c338c7768c34) y
  [`0f6edd7`](https://github.com/travisvroman/kohi/commit/0f6edd7daad0600511aa2ec6ad55643101c6b4a5)
- Fix posterior: [`ce5970a`](https://github.com/travisvroman/kohi/commit/ce5970a3b06f0fd478453f5d0abf2d3b322107ad)

### Plan

- [x] Añadir `TextureUse::specular` con valor único y mapa specular a config,
  material, loader y lifecycle.
- [x] Crear una textura specular por defecto y usarla cuando el asset no declare
  mapa; no compartir ownership de forma que se libere el default.
- [x] Exponer shininess y view position mediante uniformes tipados.
- [x] Implementar iluminación specular coherente con el modelo elegido y con la
  dirección del capítulo 49.
- [x] Hacer acquisition transaccional: si falla el segundo sampler, liberar sólo
  lo adquirido por esa operación.
- [x] Usar los PNG finales; no importar los JPG transitorios de la rama de trabajo.

### Validación y commits

- [x] Tests de parsing/defaults/release y actualización transaccional; smoke del
  material con mapa y regresión sin mapa mediante el default negro.
- [x] Commits sugeridos: `feat(materials): support specular texture maps` y
  `feat(renderer): add specular material lighting`.

### Estado NK

- Completado en `3ff43a3`, `5104ebc`, `3b2aa18` y `6f7b731`.
- `MaterialConfig` y `.kmt` exponen `specular_map_name` y `shininess`;
  `TextureUse::specular` usa un valor distinto de diffuse. El default specular
  es negro opaco de 1×1, no tiene ownership por material y desactiva el reflejo
  de forma determinista cuando el mapa se omite.
- La carga de ambos mapas publica el material sólo después de adquirirlos y
  revierte el diffuse si falla el specular. `set_texture_maps` aplica el mismo
  contrato al cambio interactivo y la tecla `T` recorre las parejas
  cobblestone, paving y paving2 con sus respectivos `_SPEC`.
- El shader world usa dos elementos del mismo binding de samplers y Blinn-Phong
  con normal, posición world, posición de cámara y shininess. La luz conserva la
  convención del capítulo 49, el specular no tiñe el diffuse ni altera alfa y el
  pase UI permanece sin cambios.
- Se importaron únicamente los PNG finales `cobblestone_SPEC`,
  `orange_lines_512_SPEC`, `paving_SPEC` y `paving2_SPEC`. No se añadieron
  librerías ni fue necesario modificar el inventario CSV.
- Validation Layers detectó que las imágenes declaraban cuatro mip levels sin
  generarlos; `5104ebc` fija un único nivel, coherente con uploads, image views y
  samplers actuales.
- Pasaron 198/198 tests en Debug y con ASan/UBSan, y 189/189 en Release. El
  paquete `nix build --offline .#nk-engine --no-link` también pasó.
- El smoke Wayland/xdg-shell bajo niri completó 120 frames con Validation Layers,
  cero eventos de allocation en los 119 frames estables y cero fugas reportadas.

## Capítulo 51 — Normal Maps

- Vídeo: [Kohi #051](https://youtu.be/I0259XQnKng?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj)
- Referencia final autoritativa: [`8d3a9d2`](https://github.com/travisvroman/kohi/commit/8d3a9d2c2b3c45219194c4004b2bc767625633e5)
- Desarrollo incluido en el squash:
  [`490b042`](https://github.com/travisvroman/kohi/commit/490b04265d04e739fd4ba9a24e7231eb70d3d739) y
  [`2544589`](https://github.com/travisvroman/kohi/commit/2544589c38dec704405f8fb279d63bba4ceb6a33)

### Plan

- [x] Añadir tangent con handedness a `Vertex3D`; revisar ABI/layout de pipeline.
- [x] Generar tangentes desde posiciones/UV/índices, acumulando y ortogonalizando;
  manejar UV degeneradas sin NaN.
- [x] Añadir `TextureUse::normal`, config/material map y textura normal por defecto
  `(0.5, 0.5, 1.0)` en espacio tangente.
- [x] Construir TBN de forma consistente en shader y transformar la muestra a
  espacio de iluminación.
- [x] Actualizar las configuraciones built-in y el formato de materiales.
- [x] No aplicar por separado los tres commits de referencia: `8d3a9d2` ya contiene
  el feature y el fix del default.

### Validación y commits

- [x] Tests de tangentes, degenerados, defaults y layout; smoke con los normal maps
  PNG finales.
- [x] Commits sugeridos: `feat(geometry): generate tangent space` y
  `feat(materials): support normal texture maps`.

### Estado NK

- Completado en `d5e31c3` y `a96d3f9`.
- `Vertex3D` usa un layout de 48 bytes y conserva una tangente `vec4`: xyz es
  ortogonal a la normal y `w` codifica handedness. La generación acumula por
  vértice, tolera UV degeneradas con fallback determinista y conserva el estado
  anterior frente a topología inválida u OOM.
- `MaterialConfig`, `.kmt`, `TextureMap` y `MaterialSystem` incorporan
  `normal_map_name`. La carga y el cambio interactivo de diffuse, specular y
  normal son transaccionales, actualizan una sola generación y mantienen
  referencias/ownership simétricos.
- El default normal usa exactamente `(128, 128, 255, 255)`, incluido el fix de
  `2544589`. El shader world expande el binding instance a tres samplers,
  reconstruye un TBN robusto y aplica el normal map en world space; el pase UI
  no cambia.
- Se importaron los PNG finales `cobblestone_NRM`, `paving_NRM` y `paving2_NRM`
  de la referencia. No se añadieron librerías ni se modificó el inventario CSV.
- Pasaron 204/204 tests en Debug y con ASan/UBSan, y 195/195 en Release. El
  paquete `nix build --offline .#nk-engine --no-link` también pasó.
- El smoke Wayland/xdg-shell bajo niri cargó `paving_NRM`, completó 120 frames
  con Validation Layers, registró cero allocation events en los 119 frames
  estables y terminó con cero fugas.

## Capítulo 52 — Point Lights and Debug Modes

- Vídeo: [Kohi #052](https://youtu.be/uD_vLeHvM1M?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj)
- Referencia final autoritativa: [`6e41e3d`](https://github.com/travisvroman/kohi/commit/6e41e3dd1349b14e623c0ac6e4eacfa521a6a8c8)
- Desarrollo incluido: [`5947269`](https://github.com/travisvroman/kohi/commit/59472695c01a0c6ce13ce0d6219d575196677883)

### Plan

- [x] Definir `PointLight` con position, color y atenuación; no dejar luces de
  producción hardcodeadas en el shader Slang. Si el tutorial las usa así como
  demostración, colocarlas en la escena/editor de prueba.
- [x] Definir capacidad máxima y count en UBO con layout CPU/GPU comprobado.
- [x] Acumular direccional + point lights con diffuse/specular y atenuación.
- [x] Crear un `RenderViewMode` tipado para default, lighting-only y normals.
- [x] Enlazar los modos a eventos/input portable. Las teclas deben entrar por
  `KeyCode`; no copiar `KeySym`/X11 al backend Wayland.
- [x] Mantener el modo fuera del backend Vulkan salvo el valor uniforme necesario.

### Validación y commits

- [x] Tests de atenuación, packing UBO, evento de cambio y teclas Wayland; smoke de
  todos los modos bajo niri.
- [x] Commits sugeridos: `feat(renderer): add point light accumulation` y
  `feat(renderer): add lighting debug views`.

### Estado NK

- Completado en `4837280` y `1e44bb5`.
- `SceneLighting` admite hasta dos `PointLight`; cada luz valida position, color
  y coeficientes antes del upload. Las dos luces del tutorial viven solamente en
  la escena de demostración, no dentro del shader.
- El packing CPU/GPU usa un `PointLightUniform` explícito de 48 bytes. El SPIR-V
  generado por Slang confirmó offsets `0/12/16/32/36/40`, stride `48`, count en
  `188`, array en `192` y `RenderViewMode` en `288`.
- El shader world está escrito en Slang y acumula ambiente una sola vez más luz
  direccional y luces puntuales Blinn-Phong con atenuación. El modo normals
  transforma `[-1, 1]` a `[0, 1]`; no existen fuentes GLSL activas.
- Los controles son `0` para default-lit, `1` para lighting-only y `2` para
  normals. Pasan por `KeyCode` y `SystemEventCode::SetRenderViewMode`; Wayland
  traduce el rango XKB numérico a esos códigos sin introducir una dependencia
  X11 en el backend nativo.
- Pasaron 208/208 tests en Debug y con ASan/UBSan, y 199/199 en Release. El
  paquete `nix build --offline .#nk-engine --no-link` también pasó.
- El smoke Wayland/xdg-shell bajo niri recorrió los tres modos durante 120 frames
  con Validation Layers, registró cero allocation events en los 119 frames
  estables y terminó con cero fugas.
- No se necesitaron dependencias, submódulos ni assets adicionales.

## Capítulo 53 — Meshes, parte 1

- Vídeo: [Kohi #053](https://youtu.be/73KBx90cD0M?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj)
- Referencia final autoritativa: [`37e0d20`](https://github.com/travisvroman/kohi/commit/37e0d209bfbd1bd8c90b63e4eec9c654e78cb30e)
- Desarrollo/fix incluido:
  [`875c03f`](https://github.com/travisvroman/kohi/commit/875c03f943c42f6e6f6c8daf27c0a698b38cbdaf) y
  [`ad134a7`](https://github.com/travisvroman/kohi/commit/ad134a765f8fba592799fbd6a98ac02660380d52)
- Merges de procedencia: [`e6b76ee`](https://github.com/travisvroman/kohi/commit/e6b76ee8a8131b4aa1c45b3c86c36e0344f5e6bf),
  [`0a92314`](https://github.com/travisvroman/kohi/commit/0a923140218fe87b31e9cd14c6fca6ab1188dd20) y
  [`dd32341`](https://github.com/travisvroman/kohi/commit/dd3234114f944f514099bd49c5a155ed9ef8aec2)

### Plan

- [x] Introducir `Mesh` como recurso que agrupa múltiples geometrías y conserva un
  transform, sin ser dueño ambiguo de punteros crudos.
- [x] Usar `dyarr`/handles estables y documentar quién adquiere/libera geometrías y
  materiales.
- [x] Extender `RenderPacket` para varias subgeometrías y evitar aplicar el mismo
  material varias veces por frame si su estado no cambió.
- [x] Agregar materiales de demostración sólo cuando correspondan a assets ya
  versionados y con rutas válidas.
- [x] Dejar el loader de disco fuera de este capítulo; aquí se prueba un mesh
  construido desde configuraciones en memoria.

### Validación y commits

- [x] Tests de lifecycle, mesh vacío/múltiple y cleanup parcial; smoke con dos o más
  geometrías/materiales.
- [x] Commits: `feat(resources): add owned multi-geometry meshes`,
  `feat(renderer): draw grouped mesh geometry` y
  `feat(demo): render multiple material meshes`; el rollback defensivo del
  arranque quedó en `fix(core): release meshes before failed startup teardown`.

### Estado implementado

- `Mesh` es move-only y RAII. Su `dyarr<Geometry*>` privado posee exactamente una
  adquisición de `GeometrySystem` por subgeometría, mientras sus accessors
  entregan sólo vistas prestadas. El sistema de geometría debe sobrevivir al
  mesh; el shutdown del engine destruye primero el `dyarr<Mesh>`.
- La creación desde `slice<const GeometryConfig>` publica el mesh sólo después de
  adquirir todas las geometrías. Un error intermedio informa el índice y la causa
  original y libera todo lo ya adquirido. El grupo vacío es válido.
- `RenderPacket` acepta una vista de meshes y el renderer expande sus geometrías
  durante el world pass sin allocation ni array plano temporal. El payload de un
  material se actualiza una vez por frame/generación y un material consecutivo ya
  enlazado no repite el descriptor bind; una secuencia `A -> B -> A` sí religa `A`.
- La demo conserva dos meshes en `dyarr`: uno usa `paving` y otro `cobblestone`.
  Los `.kmt` de `paving`, `paving2` y `cobblestone` sólo referencian las texturas
  ya versionadas. El error del ejemplo Kohi que reutilizaba la configuración del
  primer cubo al crear el segundo no fue trasladado.
- No se añadió loader de mesh, jerarquía ni `Transform`; esos alcances quedan para
  los capítulos posteriores. Tampoco se añadieron dependencias o submódulos.
- La ruta activa de shaders permanece exclusivamente en Slang: cuatro fuentes
  `.slang` y dos `.shadercfg`, compiladas por `slangc`.
- Pasaron 212/212 tests en Debug y con ASan/UBSan, y 203/203 en Release. También
  pasó `nix build --offline .#nk-engine --no-link`.
- El smoke Wayland/xdg-shell bajo niri renderizó 120 frames con dos geometrías y
  dos materiales, recorrió los tres modos de iluminación con Validation Layers,
  registró cero allocation events en los 119 frames estables y terminó con cero
  fugas.

## Capítulo 54 — Transforms

- Vídeo: [Kohi #054](https://youtu.be/r2535XLneiI?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj)
- Referencia principal: [`58b3554`](https://github.com/travisvroman/kohi/commit/58b3554a51d24c028f05f6e83af1ea8dfb2c7287)

### Plan

- [x] Crear `Transform` C++ con position, quaternion rotation y scale, métodos de
  mutación y cache de matriz local/world marcado dirty.
- [x] Representar parent como referencia no propietaria explícita o handle estable;
  prohibir self-parent y ciclos.
- [x] Propagar invalidación a descendientes sin asignaciones por frame.
- [x] Definir con claridad si `Mesh` contiene su transform o recibe uno en el render
  packet; evitar dos fuentes de verdad.
- [x] Reutilizar GLM ya instalado; no agregar otra biblioteca matemática.

### Validación y commits

- [x] Tests de identidad, TRS, parent/child, reparent, dirty cache, ciclos, lifetime
  y relocalización dentro de `dyarr`.
- [x] Commits: `feat(math): add safe hierarchical transforms` y
  `feat(renderer): render hierarchical mesh transforms`.

### Estado implementado

- `Transform` encapsula `glm::vec3`, `glm::quat` y `glm::mat4`, normaliza las
  rotaciones y compone la matriz local en orden `translation * rotation * scale`.
  Las matrices local y world se recalculan de forma lazy únicamente al estar
  marcadas dirty.
- La jerarquía es intrusiva y no propietaria mediante enlaces parent/child/sibling;
  no reserva memoria. `set_parent` devuelve `result`, rechaza self-parent y ciclos,
  y propaga la invalidación a todos los descendientes.
- El move constructor y move assignment reparan los enlaces de la jerarquía. Esto
  permite relocalizar transforms dentro de los contenedores propios; al destruir un
  parent, sus hijos se separan como raíces antes de que el puntero pueda quedar
  colgando.
- `Mesh` contiene el único `Transform` autoritativo. Se eliminó su matriz model
  paralela y el renderer obtiene `world_matrix()` directamente al expandir cada
  subgeometría.
- La demo mantiene tres meshes en `dyarr`, con materiales `paving`, `cobblestone`
  y `paving2`, formando la cadena first → second → third. Cada transform rota con
  delta time y el parentesco produce el movimiento compuesto.
- No se agregaron librerías, submódulos, assets ni shaders; la ruta de shaders
  activa continúa exclusivamente en Slang.
- Pasaron 220/220 tests en Debug y con ASan/UBSan, y 211/211 en Release. También
  pasó `nix build --offline .#nk-engine --no-link`.
- El smoke Wayland/xdg-shell bajo niri renderizó 120 frames con los tres meshes y
  materiales, recorrió los tres modos de iluminación con Validation Layers,
  registró cero allocation events en los 119 frames estables y terminó con cero
  fugas.

## Capítulo 55 — Meshes, parte 2: OBJ

- Vídeo: [Kohi #055](https://youtu.be/kIbW4Kv6p4c?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj)
- Referencia final: [`6f5b979`](https://github.com/travisvroman/kohi/commit/6f5b979bccd59e84afb2155440bdaf4e014ad3b5)
- PR de referencia: [#46](https://github.com/travisvroman/kohi/pull/46)

### Plan

- [x] Implementar `MeshResourceLoader` para `ResourceType::static_mesh` usando el
  submódulo existente `tinyobjloader` `v2.0.0rc13`.
- [x] Convertir OBJ/MTL a structs NK: posiciones, UV, normales, tangentes, índices,
  grupos por material y extents.
- [x] Deduplicar vértices con `nk::cl::map` y rapidhash; incluir en la clave todos
  los atributos que distinguen realmente un vértice.
- [x] Generar normales/tangentes sólo cuando falten y conservar el winding
  acordado por NK.
- [x] Cargar múltiples submeshes/materiales de forma transaccional y devolver
  errores de parseo, I/O, límites u OOM mediante `result`.
- [x] Validar primero con fixtures OBJ pequeños y después incorporar únicamente
  Falcon, Sponza y los mapas realmente referenciados. Excluir `Thumbs.db`, mapas
  de prueba y formatos que el runtime no necesita.
- [x] Convertir los TGA requeridos a PNG durante la importación del asset; no
  agregar otro decoder ni una dependencia runtime para TGA/JPG.

### Validación y commits

- [x] Tests: OBJ sin material, multi-material, índices negativos, caras no
  trianguladas según política, archivo inválido, extents y cleanup.
- [x] Commits: `feat(resources): load static meshes from OBJ`,
  `assets(meshes): add car and Sponza demo models` y
  `feat(demo): load car and Sponza meshes`.

### Estado implementado

- `StaticMeshResourceLoader` registra `static_mesh` como loader integrado y usa
  `tinyobjloader` `v2.0.0rc13` sólo como frontera de parseo. La información
  persistente se transforma a `dyarr`, `map`, `strbuf`, allocators y `result` de
  NK; el STL de la dependencia no se filtra hacia la API ni los recursos del
  motor.
- El importador hace triangulación fan de polígonos, soporta índices OBJ
  negativos, preserva smoothing groups, voltea V y deduplica por posición,
  normal, UV y semántica de suavizado mediante el `map` robin-hood de NK con
  rapidhash. Calcula center/extents y genera normales o tangentes finitas cuando
  corresponde.
- Los shapes se agrupan globalmente por material, por lo que Sponza produce 25
  draw groups en vez de cientos de objetos fragmentados. Los MTL se convierten
  a `MaterialConfig` inline y `Mesh::create` adquiere geometrías y materiales de
  manera transaccional; no se escriben `.kmt` durante la ejecución.
- La dependencia ya existente está fijada por submódulo y CSV, y ahora el flake
  también inyecta el commit exacto de `tinyobjloader` al construir desde una
  fuente Git pura.
- Se añadieron los OBJ/MTL de Falcon y Sponza del commit de referencia junto con
  67 mapas convertidos a PNG y reducidos para la demo. Se excluyeron archivos no
  utilizados y mapas de opacidad porque el material actual todavía no implementa
  transparencia. La procedencia y las limitaciones de redistribución están en
  `engine/assets/ATTRIBUTION.md`.
- La escena coloca el automóvil y Sponza juntos como en la referencia, conserva
  los tres cubos jerárquicos del capítulo anterior y rota sólo estos últimos. La
  ruta de shaders continúa exclusivamente en Slang.
- Implementado en `d8decd6`, `a8eacfb` y `5cb10e4`.
- Pasaron 222/222 tests en Debug y con ASan/UBSan, y 213/213 en Release. También
  pasó `nix build --offline .#nk-engine --no-link`.
- El smoke Wayland/xdg-shell bajo niri cargó Falcon como 1 grupo y Sponza como 25,
  renderizó 120 frames con Validation Layers, recorrió los modos de iluminación,
  registró cero allocation events en los 119 frames estables y cerró con cero
  fugas.

## Capítulo 56 — Custom Binary Mesh File Format

- Estado: completado en NK Engine (2026-09-06).
- Vídeo: [Kohi #056](https://youtu.be/Uk2p3vKBMXE?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj)
- Referencia principal: [`920035f`](https://github.com/travisvroman/kohi/commit/920035fa5f8184f36a27107eed0ad81582efa23b)

### Plan

- [x] Definir formato propio de NK, recomendado `.nkmesh`, con magic, versión,
  endianness, counts, tamaños, extents y nombres acotados. No heredar `.ksm`.
- [x] Validar todos los offsets/counts antes de reservar o leer; limitar tamaños
  para archivos hostiles o corruptos.
- [x] Serializar tipos de ancho fijo, nunca padding de structs C++ ni punteros.
- [x] En cache miss cargar OBJ y escribir cache mediante archivo temporal + rename;
  si el cache está corrupto o incompatible, regenerarlo desde la fuente.
- [x] Versionar el formato al cambiar layout de vértices o texture maps.
- [x] No versionar caches gigantes generados. Mantener como máximo un fixture
  binario pequeño y reproducible para compatibilidad.

### Validación y commits

- [x] Round-trip OBJ → `.nkmesh` → Mesh, truncados, magic/version inválidos,
  overflow, cache fallback y resultado determinista.
- [x] Avances segmentados en commits semánticos:
  - `7958861 feat(io): add bounded reads and atomic file replacement`
  - `86e6160 feat(resources): add validated static mesh binary codec`
  - `92ccc2b feat(resources): cache imported meshes with dependency validation`

### Adaptación y cierre

- Especificación y uso: [static-mesh-binary-format.md](static-mesh-binary-format.md).
  Header little-endian de 64 bytes, versión/revisión del importador, checksum
  rapidhash v3, strings acotados, materiales inline y vértices serializados por
  componente. El preflight valida todo el cuerpo antes de reservar el payload;
  la lectura del archivo también tiene límite previo de 128 MiB.
- El loader conserva triangulación, agrupación por material, deduplicación,
  normales/tangentes y convención UV de NK. Comprueba hashes de OBJ/MTL y presencia
  de PNG; detecta cambios del mismo tamaño/fecha y materiales antes ausentes.
  Sin OBJ admite un binario válido standalone. Caché dañada con fuente disponible
  se regenera; sin fuente falla con `result` sin publicar un recurso parcial.
- Escritura atómica con temporal exclusivo y rename/replacement, sin perder el
  destino anterior ante fallo. No poder cachear no impide cargar el OBJ. No se
  agregaron dependencias ni assets externos: se reutilizan los submódulos/tag/CSV
  existentes y Falcon/Sponza. Los `.nkmesh` generados están ignorados por Git.
- Se conserva la capa Vulkan inspirada en
  [NoGraphicsAPI](https://github.com/sebbbi/NoGraphicsAPI): el archivo sólo contiene
  datos CPU; `Mesh::create`/`GeometrySystem` siguen usando el upload del renderer
  y sus vidas explícitas. No se añadieron handles/direcciones GPU al formato,
  nuevos requisitos de GPU, GLSL, GLFW ni código macOS.
- Validación: **246/246 Debug**, **237/237 Release**, **246/246 ASan/UBSan**.
  Tras sanitizadores se restauraron y verificaron los ejecutables Debug normales.
  Build pura `nix build .#nk-engine` correcta; el paquete renderizó 60 frames con
  assets de sólo lectura y avisos no fatales por no poder escribir la caché.
- Wayland nativo, AMD Radeon Graphics RADV RENOIR: runs Debug de 120 frames en
  importación inicial, caché caliente y `NK_VULKAN_LEGACY=1`, recorriendo los modos
  de iluminación con validación de sincronización solicitada. Sin errores Vulkan
  detectados, cero allocation events en los 118 frames estables de cada run y cero
  fugas reportadas por los allocators del motor. Falcon conserva 1 grupo y Sponza
  25. Windows mantiene implementación Win32, pero no se ejecutó en esta máquina.
- Release, tres pares alternados `NK_MESH_CACHE=off`/caché caliente, 60 frames por
  run, sin compilar simultáneamente: mediana Falcon **8,733 → 0,446 ms** y Sponza
  **239,912 → 20,370 ms** (≈19,6× y ≈11,8×). El binario incluye validación y hashes
  de fuentes. Son tiempos del loader, no del arranque completo ni ganancias de
  FPS. Rango de Sponza: OBJ 236,923–245,421 ms; binario 18,887–20,966 ms. Primera
  importación Release incluyendo escritura: 265,555 ms. Cachés locales: Falcon
  ≈354 KiB y Sponza ≈11 MiB; no se versionaron.
- Regresión sin cambiar código: `NK_MESH_CACHE=off nix run .#run -- Release`.
  Ejecución normal: `nix run .#build -- Release` y `nix run .#run -- Release`.
  El primer run genera las cachés locales; los siguientes las reutilizan. En el
  store inmutable de Nix se necesita un binario pregenerado para aprovecharlas.

## Capítulo 57 — Enhancing Texture Maps

- Estado: completado en NK Engine (2026-09-07).
- Vídeo: [Kohi #057](https://youtu.be/hGA2veSznn8?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj)
- Referencia principal: [`879ccc4`](https://github.com/travisvroman/kohi/commit/879ccc411f44d5df28ffdcc9c30d27a0febd6a3e)

### Plan

- [x] Mover la configuración de sampler desde `TextureData` hacia `TextureMap`:
  filtros min/mag, wrap U/V/W y anisotropía.
- [x] Mantener `Texture` como imagen compartible y dar al renderer un recurso de
  sampler separado con ownership claro.
- [x] Traducir enums renderer-neutral a Vulkan en un único punto y validar soporte.
- [x] Limitar anisotropía a `maxSamplerAnisotropy`; desactivarla si el dispositivo
  no la soporta, en vez de fijarla siempre a 16.
- [x] Actualizar material configs y la versión de `.nkmesh` con defaults backward
  compatible o rechazo explícito de la versión antigua.
- [x] Cachear samplers idénticos sólo si las mediciones justifican la complejidad;
  el primer alcance puede administrar uno por map.

### Validación y commits

- [x] Tests de defaults, parsing, enum mapping, clamp, acquire/release y fallo
  parcial; smoke con wrap/filter visualmente distinguible.
- [x] Avances segmentados:
  - `89d1ef0 feat(resources): configure and serialize per-map sampling`
  - `4baf7ef feat(renderer): separate image and sampler resource lifetimes`
  - `b0e20bd feat(editor): add interactive sampler comparison demo`

### Adaptación y cierre

- API y ejemplos: [texture-map-sampling.md](texture-map-sampling.md).
  `SamplerConfig` contiene min/mag/mip filter, wrap U/V/W y anisotropía;
  `TextureMap` conserva imagen/uso y agrega configuración más `SamplerHandle`.
  Materiales `.kmt` aceptan `diffuse_sampler.*`, `specular_sampler.*` y
  `normal_sampler.*`; archivos existentes conservan linear/repeat/anisotropía16.
- `TextureData` sólo contiene imagen. El renderer administra samplers nativos
  independientes en `vk::Samplers`, sin caché de deduplicación: máximo
  `min(4096, maxSamplerAllocationCount)`, incluyendo el fallback. Handles con
  generación rechazan usos obsoletos tras reciclar slots. Anisotropía ya no es
  un requisito de selección de GPU; se habilita si existe soporte y se limita
  al dispositivo. `NK_VULKAN_ANISOTROPY=0` permite verificar el camino sin ella.
- `TextureBinding` es una vista copiada de imagen + sampler, nunca un puntero
  a un material temporal. Descriptores por frame detectan cambios de sampler,
  generación e identidad de imagen, incluso entre defaults con el mismo ID.
  `MaterialSystem::set_sampler` reemplaza transaccionalmente sin cambiar las
  referencias de la imagen. Defaults y fallos parciales liberan samplers/instancias.
- El diseño sigue la independencia imagen/sampler y vidas explícitas de
  [NoGraphicsAPI](https://github.com/sebbbi/NoGraphicsAPI), usando `VkSampler`
  convencional en nuestra capa compatible, sin descriptor heaps ni extensiones
  obligatorias nuevas. Mutaciones sólo entre frames; release espera submissions
  completadas. No hay creación de samplers ni espera idle en dibujos sin cambios.
  Destrucción diferida y deduplicación quedan para cuando estén justificadas.
- `.nkmesh` **v2** serializa tres configs de sampler (28 bytes cada una) por
  material. Rechaza v1 explícitamente y regenera desde OBJ; binarios standalone
  viejos requieren reexportar. La geometría y revisión del importador no cambian.
  Sin cambios de dependencias/submódulos/CSV, modelos, texturas ni shaders Slang.
- Demo opcional: `NK_SAMPLER_DEMO=1 nix run .#run -- Release`, **P** recorre cinco
  presets. Un panel muestra UV fuera de `[0,1]`, el otro amplía la misma textura;
  ambos se ajustan a ventanas tiled. La escena habitual sigue igual sin el flag.
- Verificación: **256/256 Debug**, **247/247 Release**, **256/256 ASan/UBSan**;
  ejecutables Debug normales restaurados y suite repetida. Build pura
  `nix build .#nk-engine` correcta; el paquete ejecutó 300 frames con los cinco
  presets y assets de sólo lectura (caché no escribible = warning no fatal).
- Wayland/niri sobre AMD Radeon Graphics RADV RENOIR: escena normal 120 frames;
  demo moderna 2400 frames; demo legacy sin anisotropía 300 frames; demo Release
  300 frames. Cierre final: dos runs Debug de 300 frames con
  `VK_LAYER_VALIDATE_SYNC=1`, moderno y legacy/sin anisotropía. Sin errores Vulkan
  detectados, cero allocation events en frames estables, cero fugas reportadas
  por los allocators del motor y exactamente el sampler fallback restante antes
  del shutdown del renderer. Los cambios de preset ocurren entre frames, fuera
  del contador estable. Smoke automatizado, no comparación automática de píxeles.
  Windows mantiene la ruta común, pero no fue ejecutado en esta máquina.

## Capítulo 58 — Writable Textures

- Vídeo: [Kohi #058](https://youtu.be/86022SGWaHc?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj)
- Referencia principal: [`28817f7`](https://github.com/travisvroman/kohi/commit/28817f7454101cce3a2310eccf0b3fa131587fab)

### Plan

- [x] Añadir tipo/flags y ownership explícito de textura: cargada, writable y
  external/swapchain. Usar la grafía `writable` en la API NK.
- [x] Implementar create, resize y write region mediante frontend renderer y
  backend Vulkan, con transiciones de layout y staging correctos.
- [x] Envolver imágenes del swapchain como texturas externas sin destruir su
  `VkImage` ni memoria durante release.
- [x] Incrementar generation sólo tras completar una actualización válida para que
  descriptores puedan detectar cambios.
- [x] Hacer resize transaccional y sincronizar sólo los recursos en uso; documentar
  cuándo aún se requiere esperar al dispositivo.
- [x] Separar recursos de archivo de texturas runtime para que auto-release no
  intente recargar una imagen externa.

### Validación y commits

- [x] Tests con backend fake para ownership/generation/rollback y smoke Vulkan de
  escritura + resize del swapchain bajo niri.
- [x] Commits sugeridos: `feat(textures): support writable runtime textures` y
  `refactor(renderer): expose swapchain images as external textures`.

### Implementación cerrada

- Commits funcionales: `223f0ce` (`feat(textures): support writable runtime
  textures`) y `2fe8232` (`refactor(vulkan): expose swapchain images as external
  textures`). El segundo conserva el scope sugerido con `vulkan` porque el
  ownership external se resuelve dentro del backend.
- Hardening: `0787575` (`fix(textures): reject unmanaged writable resources`)
  impide mutar texturas forjadas/no registradas, metadata inválida y recursos
  external mediante la ruta de ownership del renderer.
- Contratos y reproducción: [Writable textures](writable-textures.md).
- La escritura es una región 2D estricta, no el par offset/tamaño del tutorial que
  termina copiando la imagen completa. Resize publica el reemplazo sólo después
  de inicializarlo y sincronizar su submit; los fallos conservan el recurso viejo.
- Las imágenes WSI se exponen como `Texture` external/writable. NK posee sus views,
  mientras el swapchain sigue poseyendo los `VkImage` y su memoria.
- Cierre local: 259 tests Debug, 250 Release y 259 con ASan/UBSan; smoke Vulkan
  writable moderno y legacy de 120 frames, y smoke niri de 600 frames con cuatro
  recreaciones del swapchain. Sin VUID, sin allocations en frames estables y sin
  fugas. Windows conserva la ruta común pero no fue ejecutado en esta máquina.

## Capítulo 59 — Render Targets and Configurable Renderpasses

- Vídeo: [Kohi #059](https://youtu.be/tZjc_hSaUA4?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj)
- Referencia principal: [`4ba9e70`](https://github.com/travisvroman/kohi/commit/4ba9e704e525f73c46d5eec0bc145f02920d5f6c)
- PR de referencia: [#61](https://github.com/travisvroman/kohi/pull/61)
- Prerrequisito tardío: fix de metadata de FreeList
  [`5f910b6`](https://github.com/travisvroman/kohi/commit/5f910b6811b5c1e50f6961df30a6e3c5a1ad4506)

### Plan

- [x] Definir `RenderPassConfig`, `RenderTargetConfig` y attachment configs fuera
  del backend Vulkan: formato/tipo, load/store, clear y fuente del attachment.
- [x] Mantener `VkRenderPass`, `VkFramebuffer` y detalles de layouts dentro de
  Vulkan; el frontend sólo conserva handles/recursos renderer-neutral.
- [x] Construir targets desde texturas writable o externas del capítulo 58.
- [x] Regenerar attachments/targets al cambiar generación, tamaño o image count del
  swapchain, incluyendo world y UI.
- [x] Validar compatibilidad de dimensiones, formato, sample count y roles
  color/depth antes de crear objetos Vulkan.
- [x] Hacer init/rebuild transaccional y destruir framebuffers antes que sus views,
  images y render pass.
- [x] Mantener inicialmente el flujo world → UI. Un render graph queda fuera de
  alcance hasta que exista una necesidad distinta a estos dos passes.

### Validación y commits

- [x] Unit tests de config inválida en la frontera renderer-neutral sin Vulkan;
  Validation Layers en inicio, varios resize/restore y cierre bajo niri.
- [x] Commits sugeridos: `feat(renderer): add configurable render targets` y
  `refactor(vulkan): build render passes from renderer configs`.

### Implementación cerrada

- Commits funcionales: `5c70d70` (`feat(renderer): add configurable render
  target contracts`) y `0e90775` (`refactor(vulkan): build render passes from
  target configs`). Hardening transaccional: `7ff93c5` (`fix(renderer): preserve
  targets on failed rebuilds`).
- Contratos, ownership y reproducción: [Render targets and configurable render
  passes](render-targets.md).
- `RenderPassConfig`, `RenderTargetConfig`, formatos, sample counts y operaciones
  load/store son renderer-neutral. `VkRenderPass`, `VkFramebuffer`, views,
  layouts y dependencias permanecen en Vulkan.
- World usa color clear/store y depth clear/discard; UI carga y conserva el mismo
  color antes de presentarlo. Cada imagen del swapchain posee depth independiente.
- Dynamic rendering consume la misma config que el fallback clásico. Esto adopta
  el límite explícito de comandos/recursos de NoGraphicsAPI sin elevar el mínimo
  de GPU ni eliminar `NK_VULKAN_LEGACY=1`.
- El fix `5f910b6` de Kohi no se copió: la FreeList propia ya usa capacidad de
  metadata explícita y errores transaccionales, cubiertos por sus regresiones.
- Cierre local: 265 tests Debug, 265 con ASan/UBSan y 256 Release; smokes de 120
  frames en rutas dynamic y legacy, seis configure/resize/restore bajo niri y
  cierre limpio. Sin VUID, sin allocations en frames estables y sin fugas.
  niri no expone una acción universal de minimización; Windows no fue ejecutado.

## Matriz de verificación por entrega

Cada capítulo funcional debe satisfacer, como mínimo:

```bash
nix run .#build -- Debug --target tests
ctest --test-dir out/build/Linux-Debug --output-on-failure
NK_ENABLE_SANITIZERS=ON nix run .#build -- Debug --target tests
ctest --test-dir out/build/Linux-Debug-Sanitized --output-on-failure
nix run .#build -- Debug --target editor
NK_PLATFORM_BACKEND=wayland nix run .#run -- Debug
```

Además:

- [ ] `git diff --check` limpio.
- [ ] Sin errores ni warnings de Validation Layers relacionados con el cambio.
- [ ] AllocationTracker vuelve al baseline después del test de lifecycle.
- [ ] Fallos parciales dejan el objeto inválido pero destruible y no filtran
  memoria CPU/GPU.
- [ ] El commit es revertible por sí solo o declara de forma explícita su
  dependencia del commit inmediatamente anterior.

## Inventario completo de commits entregados

La tabla evita que un merge, fix o cambio no funcional sea interpretado como un
capítulo perdido. “Procedencia” significa que el contenido real vive en sus
padres y no se implementa una segunda vez.

| Commits | Clasificación |
|---|---|
| [`9cd8543`](https://github.com/travisvroman/kohi/commit/9cd8543b2007ce5ef4c9455a828baea46d72559d), [`df0a262`](https://github.com/travisvroman/kohi/commit/df0a2626fe32526313c404be40c3d0e4e4bf03b0), [`5fa6644`](https://github.com/travisvroman/kohi/commit/5fa66447ed2febf574f8573266f3b40085cf1a72), [`fa70696`](https://github.com/travisvroman/kohi/commit/fa70696d5abd366201f43cd31fef253339c2bdac), [`c39dde8`](https://github.com/travisvroman/kohi/commit/c39dde81cd4b18809663f6d3e923c35efe6da870), [`142da8f`](https://github.com/travisvroman/kohi/commit/142da8fcbeddad6421e9c0c9c929c7cf37b90e96), [`57712a3`](https://github.com/travisvroman/kohi/commit/57712a32e8f8fa4305a0c32b7b8299ea8e03db72) | Merges de procedencia 35–40. |
| [`7bfacf1`](https://github.com/travisvroman/kohi/commit/7bfacf18878bb0cb887bd4e2c0b09c0c422768d3), [`8311129`](https://github.com/travisvroman/kohi/commit/8311129761f181e88302e821918663957eb28136), [`d76fd4e`](https://github.com/travisvroman/kohi/commit/d76fd4eb8e9b49749dfd5c06a6ae5e8a294214db), [`6d586e1`](https://github.com/travisvroman/kohi/commit/6d586e131a2cc2319d9fd93a67a69bc446d7bb1c), [`abe4aab`](https://github.com/travisvroman/kohi/commit/abe4aab38014a89c60d4ce0537ab5354e31682eb), [`dbadba4`](https://github.com/travisvroman/kohi/commit/dbadba43231e6262afd796fc6585909462b6b193), [`ad36538`](https://github.com/travisvroman/kohi/commit/ad36538ebbf54b3d2bc77bfe364333ca4f6736f5), [`ca369fc`](https://github.com/travisvroman/kohi/commit/ca369fc63209ccf7ce069800cbed8ca846bbc1e3) | Merges de procedencia previos al 42. |
| [`c760904`](https://github.com/travisvroman/kohi/commit/c7609045ba393605acf4f7f12d585438e5a028b5), [`9f2d315`](https://github.com/travisvroman/kohi/commit/9f2d31585d0a666c808ec9664bb1acacdfb20979), [`e266181`](https://github.com/travisvroman/kohi/commit/e266181807360c3b1cf0fbc83486e5ad1ac9406a), [`63994f4`](https://github.com/travisvroman/kohi/commit/63994f4d8472789d45cd8157b0997cc6270c4e35), [`8c1b685`](https://github.com/travisvroman/kohi/commit/8c1b685fc1b2f16338aaac52d31605c3850dfc2b), [`c126dec`](https://github.com/travisvroman/kohi/commit/c126dec11b8747544b564f7fbb5b7fa0d282cc21) | Fixes retroactivos aplicables; auditarlos antes del 42. |
| [`0c52ad4`](https://github.com/travisvroman/kohi/commit/0c52ad4e0d917fd1e1d1a5a3f1c272c2f2886cf4), [`e5d084e`](https://github.com/travisvroman/kohi/commit/e5d084eb50aea71085106ef62f59d771a6403568), [`6bf0d10`](https://github.com/travisvroman/kohi/commit/6bf0d1029ea28adf57bd26a35122a7010c8f051a) | macOS: fuera de alcance. |
| [`79d5096`](https://github.com/travisvroman/kohi/commit/79d5096cc9e88e0a6aa26ee964385a63a760393b), [`8573cb6`](https://github.com/travisvroman/kohi/commit/8573cb65d50b6145d510dea4008f32a29251f427) | README: sin implementación. |
| [`7abec4f`](https://github.com/travisvroman/kohi/commit/7abec4fd52902697ad525a87a9c874d271ef423a), [`642fe9f`](https://github.com/travisvroman/kohi/commit/642fe9f2413a5ecdc2925bac067de0c0dea60e51), [`52a229e`](https://github.com/travisvroman/kohi/commit/52a229ec0b9441fed456fd8c16a83a68b02beed9), [`b9c4e87`](https://github.com/travisvroman/kohi/commit/b9c4e876fb29736cf170b7740c56fdd21a8060ba), [`1dce8dd`](https://github.com/travisvroman/kohi/commit/1dce8dd605c02a528157defd43afd5c4ac08aa76), [`6dbb907`](https://github.com/travisvroman/kohi/commit/6dbb9073ec10288da35d61183bdf238880a2fce3) | Documentación asociada al intervalo del 41. |
| [`c4ae920`](https://github.com/travisvroman/kohi/commit/c4ae9202dbd2ebd2dbb3af2f5c4fb3efd8836bed), [`a89c2df`](https://github.com/travisvroman/kohi/commit/a89c2df7b5ce6dc99b8d8d025c9237b0119fb1a2), [`185ae02`](https://github.com/travisvroman/kohi/commit/185ae02c2be3ecdd728128ab385c9dcfa84d41fb), [`5f910b6`](https://github.com/travisvroman/kohi/commit/5f910b6811b5c1e50f6961df30a6e3c5a1ad4506) | Capítulo 42 y fixes de FreeList. |
| [`fa5ca8e`](https://github.com/travisvroman/kohi/commit/fa5ca8eaa0504ffc241543f37b7b514b49cb1133), [`351ac92`](https://github.com/travisvroman/kohi/commit/351ac9256fa7c40ecda5eca08789e68004c250c2), [`5a030e1`](https://github.com/travisvroman/kohi/commit/5a030e12820a59ef3758a751c8b117e79f2155b3), [`f977156`](https://github.com/travisvroman/kohi/commit/f9771567781bf67b0bc861218b06d9d9d467183f), [`de996b0`](https://github.com/travisvroman/kohi/commit/de996b0b926c66e11bd23417e41d36181d5df273), [`6c69e93`](https://github.com/travisvroman/kohi/commit/6c69e933208e9ab4bfa33cae34274279c05df731), [`195c60a`](https://github.com/travisvroman/kohi/commit/195c60a1fbaf2b7abe797a98ce8529d232d14bd0), [`293f5a1`](https://github.com/travisvroman/kohi/commit/293f5a10b0f5c519d8181f4bd70bc9f8a52f26ad), [`dccf6f8`](https://github.com/travisvroman/kohi/commit/dccf6f8a722785a27f9c47576bde25a76fe17986), [`7973c53`](https://github.com/travisvroman/kohi/commit/7973c536d97cef8c9b7ef176619fc27e0d62a19e) | Capítulo 43; merges/docs no se duplican. |
| [`86e0dcf`](https://github.com/travisvroman/kohi/commit/86e0dcf64808e609bb244f5d90298b9640704344), [`50c4ba6`](https://github.com/travisvroman/kohi/commit/50c4ba60e8e52dce10e1a76ebaaa443f4c8aaf25), [`d64730b`](https://github.com/travisvroman/kohi/commit/d64730bc775c5ffc391eefee47d49cc59f838f7e) | Capítulo 44 e integración. |
| [`c144e17`](https://github.com/travisvroman/kohi/commit/c144e1720255c89a6460b9fab3be6dcd286f851b), [`6a8e03b`](https://github.com/travisvroman/kohi/commit/6a8e03b6cb7ff491d36c7d65595e83c21454b9a6), [`04d3244`](https://github.com/travisvroman/kohi/commit/04d3244880888df8d0a5386106e0052cecbabd64), [`cb70fd3`](https://github.com/travisvroman/kohi/commit/cb70fd3bab8875a667df36c639c0b58777abe782), [`716b35a`](https://github.com/travisvroman/kohi/commit/716b35a68df837639f0d496846be9e7ab8f65dcd), [`3e1596a`](https://github.com/travisvroman/kohi/commit/3e1596a24c9bfbc42138d04c154e15a6c0250aac) | Merges de fixes alrededor del 44; procedencia solamente. |
| [`4d75df7`](https://github.com/travisvroman/kohi/commit/4d75df779d6cf3241803b432fc2ba84d3d052e3c), [`cd22fa7`](https://github.com/travisvroman/kohi/commit/cd22fa7e3ba0035ed0aba86140008e547c90ff3e) | Plantilla/VS Code: sin implementación. |
| [`c176d1b`](https://github.com/travisvroman/kohi/commit/c176d1bd21b067d030b8ec0a0386d04b3be740ff), [`c50e824`](https://github.com/travisvroman/kohi/commit/c50e82498bdf38e55c38ae076a5505984cd47c2f), [`805a53b`](https://github.com/travisvroman/kohi/commit/805a53b60cd287ddc0fe0fd3b5518825cf7f335a), [`2cbd684`](https://github.com/travisvroman/kohi/commit/2cbd684787246681f1a37d670d925fde077bfcf2) | Fixes renderer/Vulkan alrededor del 44; se vuelven regresiones obligatorias. |
| [`af39ef5`](https://github.com/travisvroman/kohi/commit/af39ef567da7f7b2607e1eca0a8d1eedc2fe01f5) | Squash conjunto de capítulos 45–48. |
| [`06575c3`](https://github.com/travisvroman/kohi/commit/06575c31f930bcbd08408991ad4a3e77c3043e05), [`6960d48`](https://github.com/travisvroman/kohi/commit/6960d48b056b310e45f5e52163ad712682febc90), [`532a8af`](https://github.com/travisvroman/kohi/commit/532a8af4ba4a3501ab204d9d151c6063a4e9b925), [`bc05433`](https://github.com/travisvroman/kohi/commit/bc05433030703151b5cdcb472de35f234510ee6f) | Capítulo 49. |
| [`6a96cf6`](https://github.com/travisvroman/kohi/commit/6a96cf6c26e1d597beae0d981d5ce611f91b5f8b), [`f88fc3f`](https://github.com/travisvroman/kohi/commit/f88fc3fff260cd8cbb022b258c805f2779d89ecb), [`a1cf8d8`](https://github.com/travisvroman/kohi/commit/a1cf8d87725089a1a1d537d13966bfca7819a570), [`3d76931`](https://github.com/travisvroman/kohi/commit/3d76931b339f79d7f484b6723669c338c7768c34), [`0f6edd7`](https://github.com/travisvroman/kohi/commit/0f6edd7daad0600511aa2ec6ad55643101c6b4a5), [`ce5970a`](https://github.com/travisvroman/kohi/commit/ce5970a3b06f0fd478453f5d0abf2d3b322107ad) | Capítulo 50, integración con 49 y fix posterior. |
| [`490b042`](https://github.com/travisvroman/kohi/commit/490b04265d04e739fd4ba9a24e7231eb70d3d739), [`2544589`](https://github.com/travisvroman/kohi/commit/2544589c38dec704405f8fb279d63bba4ceb6a33), [`8d3a9d2`](https://github.com/travisvroman/kohi/commit/8d3a9d2c2b3c45219194c4004b2bc767625633e5) | Capítulo 51; el último es el squash final. |
| [`5947269`](https://github.com/travisvroman/kohi/commit/59472695c01a0c6ce13ce0d6219d575196677883), [`6e41e3d`](https://github.com/travisvroman/kohi/commit/6e41e3dd1349b14e623c0ac6e4eacfa521a6a8c8) | Capítulo 52; el último es el squash final. |
| [`875c03f`](https://github.com/travisvroman/kohi/commit/875c03f943c42f6e6f6c8daf27c0a698b38cbdaf), [`ad134a7`](https://github.com/travisvroman/kohi/commit/ad134a765f8fba592799fbd6a98ac02660380d52), [`e6b76ee`](https://github.com/travisvroman/kohi/commit/e6b76ee8a8131b4aa1c45b3c86c36e0344f5e6bf), [`37e0d20`](https://github.com/travisvroman/kohi/commit/37e0d209bfbd1bd8c90b63e4eec9c654e78cb30e), [`0a92314`](https://github.com/travisvroman/kohi/commit/0a923140218fe87b31e9cd14c6fca6ab1188dd20), [`dd32341`](https://github.com/travisvroman/kohi/commit/dd3234114f944f514099bd49c5a155ed9ef8aec2) | Capítulo 53 e integración hacia 54. |
| [`58b3554`](https://github.com/travisvroman/kohi/commit/58b3554a51d24c028f05f6e83af1ea8dfb2c7287) | Capítulo 54. |
| [`76055c5`](https://github.com/travisvroman/kohi/commit/76055c56c86d8b1cd85a9c0624236ea1b537d9d6), [`9be9f18`](https://github.com/travisvroman/kohi/commit/9be9f185c4fc78e5826f0b241b63322ef4bc154b), [`9424fda`](https://github.com/travisvroman/kohi/commit/9424fda2ca4d384d15383c7ee411bd37ebd66c8c), [`38c3c52`](https://github.com/travisvroman/kohi/commit/38c3c521017895ef26ef7862ad30428a0504665c), [`653f8d8`](https://github.com/travisvroman/kohi/commit/653f8d866b8395ad21f9228140c03fcb088c5a84) | Secuencia de pruebas de README sin cambio funcional neto. |
| [`6f5b979`](https://github.com/travisvroman/kohi/commit/6f5b979bccd59e84afb2155440bdaf4e014ad3b5) | Capítulo 55. |
| [`920035f`](https://github.com/travisvroman/kohi/commit/920035fa5f8184f36a27107eed0ad81582efa23b) | Capítulo 56. |
| [`879ccc4`](https://github.com/travisvroman/kohi/commit/879ccc411f44d5df28ffdcc9c30d27a0febd6a3e) | Capítulo 57. |
| [`28817f7`](https://github.com/travisvroman/kohi/commit/28817f7454101cce3a2310eccf0b3fa131587fab) | Capítulo 58. |
| [`4ba9e70`](https://github.com/travisvroman/kohi/commit/4ba9e704e525f73c46d5eec0bc145f02920d5f6c) | Capítulo 59. |

## Regla de ejecución futura

Al comenzar un capítulo:

1. Marcar sus tareas como en progreso sin modificar las de capítulos posteriores.
2. Comparar el estado final de referencia, no aplicar commits de desarrollo como
   parches literales.
3. Implementar primero contratos y tests, luego integración y assets.
4. Ejecutar la matriz de verificación.
5. Crear los commits semánticos propuestos o equivalentes, sin mencionar capítulos.
6. Actualizar este documento con decisiones, desviaciones justificadas y hashes de
   los commits NK resultantes.
