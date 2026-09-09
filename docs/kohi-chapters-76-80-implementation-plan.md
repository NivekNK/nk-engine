# Plan de implementación de Kohi 76–80 en NK Engine

- Estado: implementado y validado en Linux; validación Win32 pendiente
- Fecha de análisis: 2026-09-08
- Punto de partida: texto y fuentes de Kohi 73–75 ya adaptados
- Baseline del repositorio al preparar el plan: `81b2012`

## Objetivo

Adaptar las ideas útiles de Kohi 76–80 al estado actual de NK Engine sin copiar
su arquitectura literalmente. El resultado debe aportar render passes y render
targets configurables, picking por GPU sin stalls, una separación limpia entre
engine y aplicación, métricas compartidas y frustum culling conservador.

El capítulo 76 se tratará como una auditoría y no como un port: NK Engine no
incorporará macOS, Objective-C, GLFW ni el build system antiguo de Kohi. Linux
seguirá usando Wayland/xdg-shell bajo niri y Windows conservará su backend
Win32.

Clay no se integrará en este tramo. Las APIs de texto, picking y construcción
de UI sí deben dejar puntos de extensión claros para que una futura capa sobre
Clay no dependa directamente de Vulkan ni del lifecycle interno del engine.

## Decisiones cerradas

- Todo shader nuevo será Slang; no se añadirán fuentes GLSL ni pasos con
  `glslc`.
- NoGraphicsAPI seguirá siendo referencia para una API Vulkan explícita y de
  bajo overhead, no una dependencia enlazada.
- La ruta moderna usará dynamic rendering y synchronization2 cuando estén
  disponibles. El backend legacy producirá la misma semántica mediante render
  passes y barreras clásicas.
- El picking usará un attachment entero `R32_UINT`, sujeto a validación de
  capacidades. Vulkan exige soporte de color attachment y transfer para este
  formato en el conjunto base de formatos de 32 bits; aun así, el backend
  comprobará la capacidad y devolverá un error explícito si el dispositivo no
  cumple el contrato. Véase la
  [tabla oficial de soporte de formatos](https://docs.vulkan.org/spec/latest/chapters/formats.html).
- La lectura del píxel será asíncrona mediante buffers persistentes por frame.
  No se introducirán `vkDeviceWaitIdle`, `vkQueueWaitIdle` ni submissions
  inmediatos en el hot path. Las transiciones seguirán las
  [recomendaciones de sincronización de Vulkan](https://docs.vulkan.org/guide/latest/synchronization_examples.html)
  y usarán stages/accesos tan precisos como sea posible para evitar barreras
  excesivamente amplias.
- La identidad seleccionable será un valor opaco de 32 bits respaldado por un
  handle generacional. `0` significará “ningún objeto”. No se copiará el
  registro global de `void*` ni los UUID aleatorios de Kohi.
- El culling se ejecutará por vista en el renderer, antes de ordenar
  transparencia. No quedará escondido en la escena de ejemplo.
- Se usarán `arr`, `dyarr`, `map`, `slice`, `str`, allocators y `result`
  propios. Un frame estable no debe reservar memoria en el heap general.
- No se prevén dependencias nuevas. Si durante la implementación aparece una
  necesidad real, se detendrá ese cambio para justificarla; sólo se aceptará
  como submódulo fijado a un tag y registrada en `.scripts/libraries.csv`.
- Cada avance importante se cerrará con un commit semántico cuyo asunto
  describa el resultado y no el número del capítulo.

## Segmentación histórica

| Capítulo | Tema real | Vídeo | Commit funcional |
|---|---|---|---|
| 76 | Port a macOS | [vídeo 76](https://youtu.be/g9tvP4qf5sA?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj) | [`7f67f7f`](https://github.com/travisvroman/kohi/commit/7f67f7fae8ea6120f751c9a59f70ba270634db1e) |
| 77 | Render View Configurability, Part 1 | [vídeo 77](https://youtu.be/wjiCBUXFVow?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj) | [`5acd990`](https://github.com/travisvroman/kohi/commit/5acd990c22df113a91a384ff9a27771e1dd5c6bb) |
| 77.5 | Render View Configurability, Part 2 | [vídeo 77.5](https://youtu.be/akxXEfbZcrk?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj) | [`784ebad`](https://github.com/travisvroman/kohi/commit/784ebad990d8e6d6a1d90ea865c05a1e4a536152) |
| 78 | Pixel-perfect object selection | [vídeo 78](https://youtu.be/mx1TyxNFrdo?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj) | [`50b49dc`](https://github.com/travisvroman/kohi/commit/50b49dc137fa188d28fb7b86a79c7bb1cb3bb2ab) |
| 79 | Métricas y organización de la aplicación | [vídeo 79](https://youtu.be/UWaUUhOFCec?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj) | [`8c834f3`](https://github.com/travisvroman/kohi/commit/8c834f3e85833b5102319387145904d9c69aa80b) |
| 80 | Frustum culling | [vídeo 80](https://youtu.be/vJrfcI_bMTA?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj) | [`56bf83b`](https://github.com/travisvroman/kohi/commit/56bf83b07d30dc7eb140c999bda20ea2c7256577) |

### Commits auxiliares

| Commit | Clasificación | Acción en NK Engine |
|---|---|---|
| [`65e7bb7`](https://github.com/travisvroman/kohi/commit/65e7bb784ffa529c34080c65a6e5ba992789630e) | Merge del port 76; su árbol no añade estado funcional sobre `7f67f7f`. | Conservar como procedencia, sin implementación duplicada. |
| [`440a2d7`](https://github.com/travisvroman/kohi/commit/440a2d71b729ecbdd1c2e477a84508479beba6d0) | Actualización de README y captura después del port. | No trasladar; no cambia el engine. |
| [`7be4a68`](https://github.com/travisvroman/kohi/commit/7be4a68483dce5e031e488b3ad97e2c73837cc21) | Archivo de financiación de GitHub entre 79 y 80. | Registrar como no funcional; fuera del alcance. |

Los nueve commits entregados quedan así inventariados. Ninguno se descarta sin
clasificación y ninguno de los tres auxiliares genera trabajo técnico falso.

## Estado actual y brechas reales

| Área | Estado en NK Engine | Brecha para este tramo |
|---|---|---|
| Plataformas | Wayland/xdg-shell y Win32 propios, escala de contenido y Vulkan configurado por CMake/Nix. | Sólo auditar los pequeños cambios portables de 76. |
| Render targets | `RenderPassConfig`, attachment color/depth, load/store, source, clear y validación ya existen. | El backend aún construye world/UI de forma rígida; una vista sólo referencia un `RenderPassKind`. |
| Vistas | `RenderViewSystem` ya posee handles generacionales, orden, proyección y construcción de packets. | Falta que cada vista posea descriptores de passes y targets recreables. |
| Texturas | Formatos color/depth, cubemap, writable/external y uploads explícitos. | Falta formato entero, usos explícitos y readback de una región. |
| Comandos Vulkan | Estados `ImageUse`, barreras modernas/legacy y dynamic rendering/legacy. | Faltan copia image-to-buffer y viewport/scissor dinámicos en la fachada. |
| Identidad | Recursos y vistas usan handles, pero un draw no expone identidad seleccionable. | Falta `PickId`, registro generacional y propagación hasta el shader. |
| Texto | FreeType/HarfBuzz, atlas, layout, batches, clipping y Slang ya están implementados. | Debe poder participar opcionalmente en picking sin acoplarse a Clay. |
| Aplicación | `App` sólo tiene `update`, `render` y `on_resized`. | La escena Sponza/Falcon, UI y overlay aún viven mayormente en `Engine`. |
| Métricas | Hay mediciones parciales en renderer, benchmark y overlay. | Falta una fuente central, estable y allocation-free. |
| Geometría | Meshes poseen bounds locales; renderer reúne y ordena draws. | Falta transformar bounds correctamente y filtrar por cada frustum. |

## Orden de implementación

```text
76 Auditoría portable
        │
        ▼
77 Descriptores de pass/target ──► 77.5 Ownership por vista e identidades
                                            │
                                            ▼
                                  78 Picking asíncrono
                                            │
                                            ▼
                              79 App layer y métricas
                                            │
                                            ▼
                                  80 Frustum culling
```

77 y 77.5 forman una sola migración arquitectónica, pero se validarán por
separado. 78 necesita attachments offscreen e identidades estables. 79 mueve la
escena sólo cuando picking y render views ya tienen contratos claros. 80 usa
las métricas y la construcción de packets resultantes para demostrar su efecto.

## Capítulo 76 — Auditoría portable del port de macOS

### Alcance de la referencia

El commit de Kohi incorpora plataforma Objective-C/Cocoa, MoltenVK, cambios en
Makefiles, generación de versión y normalización de algunas teclas. La mayor
parte contradice deliberadamente el alcance de NK Engine.

### Trabajo

- [x] Confirmar que Page Up/Down, Print Screen, Super izquierda/derecha y las
  teclas de puntuación tienen nombres canónicos y equivalentes en Wayland y
  Win32.
- [x] Verificar que el logging de teclas y eventos nunca dependa de enums
  específicos de una plataforma.
- [x] Auditar que las variables y extensiones Vulkan de release/debug se
  seleccionen por configuración, no por una suposición de Windows o Linux.
- [x] Confirmar que CMake, Nix y los scripts Bash/PowerShell cubren el mismo
  producto sin importar los Makefiles introducidos por Kohi.
- [x] Documentar explícitamente que Cocoa, Objective-C, MoltenVK, GLFW y el
  generador de versión del commit quedan fuera.

NK Engine ya contiene la mayoría de las normalizaciones de teclado. Si la
auditoría no detecta una brecha funcional, este capítulo se cerrará sin cambios
de código y sin fabricar un commit vacío.

### Validación y criterio de salida

- Tests de traducción de teclas Wayland y Win32 existentes en verde.
- Build Debug Linux y compilación de la ruta Win32 sin regresiones.
- No aparecen dependencias, fuentes ni flags para macOS/GLFW.

Commit sólo si hay una corrección real, por ejemplo:
`fix(input): normalize portable key names across platform backends`.

## Capítulo 77 — Render View Configurability, Part 1

### Diseño adaptado

Se ampliará el contrato renderer-neutral existente, no se crearán copias
paralelas de los tipos de Kohi.

- `RenderPassConfig` describirá una secuencia explícita de attachments: rol,
  source, formato, sample count, load/store, uso final y política de resize.
- Una `RenderPassSignature` inmutable contendrá únicamente lo que hace
  compatible a un pipeline: formatos, sample count y presencia/formato de
  depth/stencil. `RenderPassKind` seguirá siendo semántica de alto nivel, no la
  clave de compatibilidad Vulkan.
- Los shaders/pipelines recibirán esa signature. Así un pipeline world puede
  renderizar tanto al swapchain como al target entero de picking sin hardcodear
  el nombre de un pass.
- El backend materializará la misma descripción como dynamic rendering o como
  `VkRenderPass`/framebuffer legacy. No habrá dos modelos públicos.
- `TextureFormat` añadirá `r32_uint`. Los usos de textura se expresarán mediante
  flags explícitos (`sampled`, `color_attachment`, `depth_attachment`,
  `transfer_source`, `transfer_destination`) en vez de inferirlos de
  `writable`.
- `GraphicsCommands` incorporará viewport/scissor dinámicos y copia de una
  región image-to-buffer. Sus barreras continuarán aceptando estados semánticos
  `ImageUse`, ocultando synchronization2 frente al fallback.

### Trabajo

- [x] Extender y validar las descripciones de attachment, incluida la
  incompatibilidad entre formato, rol, source, load/store y uso final.
- [x] Mantener el límite requerido de dos attachments mediante una constante
  pequeña y comprobada; no introducir listas con heap por pass. No se elevó
  porque los passes actuales necesitan como máximo color + depth.
- [x] Introducir `RenderPassSignature` con igualdad/hash deterministas para el
  cache de pipelines.
- [x] Migrar la comprobación de compatibilidad del backend desde
  `RenderPassKind` a la signature
  para compatibilidad real.
- [x] Mover la definición rígida de los passes world/UI fuera de
  `VulkanRenderer` hacia una descripción renderer-neutral consumida por el
  backend.
- [x] Añadir formato `r32_uint`, flags de uso y conversiones `TextureFormat` ↔
  `VkFormat`.
- [x] Añadir viewport, scissor y `copy_image_to_buffer` a `GraphicsCommands` con
  rutas sync2/legacy equivalentes.
- [x] Hacer que creación y resize sean transaccionales: un fallo conserva el
  target anterior válido.
- [x] Auditar los fixes de allocator contenidos en Kohi 77; aplicar sólo una
  corrección reproducible que no esté ya cubierta por nuestros allocators y
  tests.

### Pruebas

- Configuraciones válidas e inválidas, formatos incompatibles, attachment
  stale, load sin contenido previo y source incorrecta.
- Igualdad/hash de signatures y rechazo de pipelines incompatibles.
- Conversión de `r32_uint` y validación de usage bits.
- Rutas moderna y legacy con el mismo estado lógico de imagen.
- OOM inyectado durante creación/resize sin estado parcial ni fuga.
- Frame estable sin allocations nuevas después del calentamiento.

### Criterio de salida

World y UI se describen fuera del backend; ambas rutas Vulkan consumen la misma
configuración, renderizan como antes y pasan Validation Layers. No se comienza
picking mientras quede un pass codificado de forma especial en Vulkan.

Commit sugerido:
`refactor(renderer): drive passes from explicit attachment descriptions`.

## Capítulo 77.5 — Ownership de vistas e identidad seleccionable

### Diseño adaptado

Kohi termina el ownership de passes por vista y agrega un registro global de
identificadores. NK conservará la primera idea y reemplazará la segunda por una
API tipada.

- Cada slot de `RenderViewSystem` poseerá su configuración validada y un handle
  generacional. Los targets dependientes de la ventana serán materializados y
  publicados transaccionalmente por el backend, porque sus objetos nativos no
  deben filtrarse a la vista renderer-neutral.
- Los packets sólo tomarán prestada memoria cuyo lifetime esté ligado al frame;
  no conservarán punteros a configuración temporal.
- `PickId` será un `u32` opaco, con `0` reservado. Internamente empacará slot y
  generación; la distribución exacta se fijará con constantes y tests antes de
  exponer capacidad máxima.
- `PickRegistry` usará slots, free list y allocator propios. Resolver un id
  comprobará generación y devolverá un handle/referencia tipada, nunca un
  `void*` sin ownership.
- `GeometryRenderData`, `Mesh` y los comandos relevantes de texto/UI aceptarán
  un `PickId` opcional. Un texto no será seleccionable por defecto.

### Trabajo

- [x] Mantener una vista por pass semántico y ordenar las vistas, con ownership
  estable de sus configuraciones. Con sólo world, UI y picking, no se añadió un
  render graph multi-pass que todavía no tiene consumidores reales.
- [x] Crear/recrear attachments window-sized al cambiar extent, conservando el
  estado anterior si alguna reserva o creación Vulkan falla.
- [x] Invalidar handles/targets con generaciones, sin eventos broadcast para
  coordinar una operación local del renderer.
- [x] Implementar `PickId` y `PickRegistry` generacionales con capacidad fija o
  reservada.
- [x] Propagar el identificador opcional por los packets de world, UI y texto.
- [x] Definir que el futuro adapter de Clay asignará ids a primitivas
  interactuables y mantendrá el mapping widget ↔ id fuera del renderer.

### Pruebas

- Vistas duplicadas, pass order inválido, source incompatible y capacidad
  agotada.
- Resize exitoso, resize con OOM y rollback completo.
- Id válido, liberación/reutilización de slot, generación stale y resultado
  tardío después de destruir un objeto.
- Construcción de packets sin ownership colgante y sin heap por frame.

### Criterio de salida

El backend puede materializar un target offscreen recreable desde descripciones
renderer-neutral y cada draw puede cargar un identificador estable sin que el
backend conozca objetos de gameplay.

Commit sugerido:
`feat(renderer): own view passes and generational pick identities`.

## Capítulo 78 — Pixel-perfect object selection

### Diferencias deliberadas respecto de Kohi

Kohi codifica un identificador de 24 bits en RGB, renderiza el pass de picking
y lee el píxel de forma síncrona. Esa implementación sirve para explicar el
flujo, pero causaría stalls evitables y limita el espacio de ids.

NK usará un attachment `R32_UINT`, salida fragmentaria `uint` en Slang y un ring
de readback persistente asociado a los frames in flight. La copia de 1×1 se
grabará en el mismo command buffer gráfico y se consumirá únicamente cuando el
fence de ese frame ya haya sido retirado.

### Flujo objetivo

```text
movimiento/click/revisión de escena
              │
              ▼
      PickRequest secuenciada
              │
              ▼
world pick pass ──► UI pick pass ──► barrera ──► copia 1×1 a readback[N]
                                                        │
                                      fence de N retirado en begin_frame
                                                        │
                                                        ▼
                                  validar PickId ──► PickResult tipado
```

### Trabajo

- [x] Crear shaders Slang de picking para geometría world y UI. La identidad se
  enviará como push/root data; no habrá una instancia de shader por objeto.
- [x] Configurar un target `R32_UINT` window-sized y depth apropiado.
- [x] Encadenar world y UI: world limpia color/depth; UI carga el color y
  sobrescribe sólo donde dibuja. Skybox y texto decorativo emitirán id cero.
- [x] Respetar alpha/cutout y scissor. Una zona transparente o recortada no debe
  volver seleccionable el rectángulo completo.
- [x] Convertir coordenadas lógicas de input a píxeles físicos con
  `content_scale`, redondeo y clamp. Cubrir la orientación Y usada por el
  viewport Vulkan y la escala fraccional de Wayland.
- [x] Crear un readback buffer persistente por frame in flight, mapeado una vez
  cuando la memoria lo permita.
- [x] Transicionar color attachment → transfer source, copiar un texel y
  restaurar el uso requerido sin ampliar la barrera al pipeline completo.
- [x] Consumir resultados tras el fence correspondiente, validar generación y
  publicar `PickResult { request, id, position, scene_revision }`.
- [x] Coalescer hover: renderizar picking sólo al moverse el puntero o cambiar
  cámara/escena. Un click conservará su secuencia y posición para no seleccionar
  con un resultado anterior.
- [x] Exponer polling/evento tipado al App; el renderer no modifica selección ni
  estado de widgets.
- [x] Añadir `NK_PICKING=0` como rollback/A-B temporal sin afectar el render
  principal.

### Pruebas

- Codec/registry de ids, incluidos cero, capacidad, wrap documentado y stale.
- Conversión logical → physical con escalas 1.0, 1.25, 1.5 y bordes de ventana.
- Coalescing de hover, preservación de click y descarte por scene revision.
- Lectura retardada después de destruir/reutilizar el objeto.
- Política de alpha y scissor en UI/texto.
- Resize/minimize entre request y retiro del fence sin leer recursos viejos.
- Smoke Wayland/niri en Vulkan moderno y legacy con Validation Layers, hover y
  click sobre Sponza/Falcon/UI, sin VUID, stalls globales ni allocations por
  frame estable.

### Criterio de salida

Hover y click identifican el draw visible correcto con latencia acotada de uno
o dos frames. La CPU nunca espera específicamente por picking y deshabilitarlo
no cambia la imagen principal.

Commits sugeridos:

- `feat(renderer): render object identities to offscreen targets`
- `perf(renderer): retire object picking without GPU stalls`

## Capítulo 79 — Métricas y organización de la aplicación

### Métricas

Kohi agrega un promedio móvil de 30 frames. NK tendrá una única fuente de
métricas, evitando el acumulador incorrecto y las mediciones duplicadas que se
pueden producir al dejar lógica separada en overlay, benchmark y renderer.

- `FrameMetrics` será un snapshot de valor con frame time, FPS, CPU total,
  update, construcción de packets, render, GPU retirado, draws, candidatos,
  visibles, culled y latencia de picking.
- El historial tendrá capacidad fija y suma acumulada O(1); no recorrerá ni
  reservará un array cada frame.
- GPU y picking se etiquetarán con el frame real al que pertenecen. No se
  presentará un sample retrasado como si fuera el frame actual.
- `TextOverlay` será consumidor de un snapshot y mantendrá su actualización
  visual a baja frecuencia; dejará de ser dueño de la medición.

### Separación Engine/App

El engine seguirá controlando plataforma, memoria, jobs, recursos, renderer,
loop y teardown. La aplicación será dueña de la escena y decidirá qué dibujar.

- Ampliar `App` con lifecycle C++ explícito y resultados tipados:
  `initialize(AppServices&)`, `update(FrameContext&)`,
  `build_frame(FrameBuilder&)`, `on_resized(...)` y `shutdown()`.
- `AppServices` expondrá fachadas/referencias acotadas, no globals ni acceso al
  estado privado de `Engine`.
- Mover Sponza, Falcon, cubos, skybox, luces, overlay, selección y controles de
  demostración desde `Engine` a `editor`/aplicación de ejemplo.
- Mantener en `Engine` sólo mecanismos reutilizables: configuración, input,
  frame loop, lifecycle de sistemas, render views y métricas.
- Usar `LinearAllocator` o scratch por frame para comandos y packets. Se
  reseteará cuando sus datos hayan sido consumidos/copiados, nunca mientras el
  backend conserve referencias.
- No colocar en el arena objetos con destructores no triviales salvo que exista
  un mecanismo explícito que ejecute esos destructores.
- Durante shutdown: impedir nuevas publicaciones async, drenar/cancelar jobs,
  permitir que `App::shutdown` libere recursos mientras sus sistemas siguen
  vivos y finalmente destruir los sistemas en orden inverso.
- Preparar la futura integración de Clay haciendo que la aplicación sea dueña
  de estado/layout/foco y traduzca sus comandos a `TextDrawList`/draws UI. No se
  añade Clay ni una API específica de Clay ahora.

### Trabajo

- [x] Implementar el agregador central y snapshot de métricas.
- [x] Conectar timers CPU/GPU, counters del renderer y latencia de picking.
- [x] Migrar benchmark y overlay para consumir la misma fuente.
- [x] Diseñar `AppServices`, `FrameContext` y `FrameBuilder` con ownership
  documentado.
- [x] Migrar el lifecycle de `App` desde bools ambiguos a `result` donde el fallo
  sea recuperable o de inicialización.
- [x] Trasladar todos los assets y comportamiento de la escena de demostración
  a `editor` sin introducir singletons.
- [x] Activar el allocator de frame y comprobar su frontera temporal.
- [x] Eliminar de `Engine` nombres/rutas específicas de Sponza, Falcon y UI de
  ejemplo.

### Pruebas

- Ring y promedio con primer sample, wrap, frame largo y precisión numérica.
- Etiquetado correcto de samples GPU retrasados.
- Orden initialize/update/build/shutdown y rollback de initialize fallido.
- Shutdown con jobs de recurso pendientes, sin callback hacia un App destruido.
- Reset del frame arena sólo después de terminar todos sus consumidores.
- Test arquitectónico o búsqueda que impida reintroducir assets de demo en
  `Engine`.
- Escena visual equivalente y selección funcional tras la migración.
- Cero allocations en frames estables después del warm-up.

### Criterio de salida

`Engine` puede ejecutar otra aplicación sin conocer Sponza, Falcon ni el
overlay. La escena actual sigue funcionando desde `editor`, y todos los
consumidores observan el mismo snapshot de métricas.

Commits sugeridos:

- `feat(core): expose allocation-free frame metrics`
- `refactor(core): move scene ownership into the application layer`

## Capítulo 80 — Frustum culling

### Diseño matemático

Se implementarán `Plane`, `Frustum` y `Aabb` sobre los tipos GLM que ya usa el
engine. Las convenciones de handedness, clip depth y orientación se documentarán
y probarán contra el mismo `projection * view` usado para renderizar.

La extracción de planos no asumirá silenciosamente depth `[0, 1]`. Hoy el
engine debe conservar su convención GLM/Vulkan existente; una eventual migración
global a `GLM_FORCE_DEPTH_ZERO_TO_ONE` será un cambio separado. Los tests usarán
un oráculo de clip space para near/far y evitar una matriz que parece correcta
pero culla geometría visible.

Para transformar bounds no se copiará el atajo de Kohi de transformar sólo el
punto máximo. La AABB world conservadora será:

```text
world_center       = model * vec4(local_center, 1)
world_half_extents = abs(mat3(model)) * local_half_extents
```

Esto cubre rotación, escala no uniforme y escala negativa. Matrices o bounds no
finitos se tratarán como visibles para fallar de forma segura.

### Integración

- [x] Implementar planos normalizados, frustum y pruebas sphere/AABB con una
  política explícita para tangencia.
- [x] Calcular el frustum por cada vista world usando exactamente sus matrices
  finales.
- [x] Transformar los bounds locales de cada `GeometryRenderData` mediante
  center/extents conservadores.
- [x] Filtrar candidatos antes de construir/ordenar las listas opaca y
  transparente.
- [x] Excluir skybox y vistas UI del culling de mundo.
- [x] Usar scratch/capacidad reservada por vista; no crear `dyarr` crecientes en
  el hot path.
- [x] Registrar candidatos, visibles, culled y draws finales en `FrameMetrics`.
- [x] Añadir `NK_FRUSTUM_CULLING=0` como interruptor de diagnóstico y rollback.
- [x] Mantener el orden de transparencia calculado sólo sobre objetos visibles.

### Pruebas

- Planos normalizados y puntos delante/detrás.
- Sphere/AABB dentro, fuera, intersectando y tangente.
- Near/far, aspect ratios distintos y cámara rotada.
- Modelos trasladados, rotados, con escala no uniforme y espejada.
- Jerarquía de transforms y bounds que cruzan el frustum.
- NaN/infinito y bounds degenerados con política fail-open.
- Comparación contra el oráculo de clip space para las convenciones actuales.
- Escena idéntica con culling on/off desde posiciones de cámara conocidas.
- Smokes Vulkan moderno y legacy sin VUID ni popping visible.

### Benchmark y criterio de salida

Medir en Release una ruta de cámara fija por Sponza con culling desactivado y
activado. Registrar CPU de preparación/render, GPU, candidatos, visibles y draws
en vez de afirmar una mejora sólo por FPS.

La escena Sponza actual está agrupada en relativamente pocos meshes/bounds
grandes, por lo que el culling puede reducir menos draws de lo esperado. Eso no
es un fallo del sistema: BVH, octree, meshlets, occlusion culling o dividir los
assets son optimizaciones posteriores y quedan fuera de este capítulo.

El capítulo termina cuando no hay falsos negativos en los tests/recorridos, el
toggle produce la misma imagen y las métricas demuestran cuánto trabajo se
eliminó en esta escena concreta.

Commits sugeridos:

- `feat(math): add conservative frustum intersection primitives`
- `perf(renderer): cull invisible world geometry per view`

## Resultado de la implementación

La implementación quedó segmentada en los siguientes commits semánticos:

- `025667d fix(input): normalize the keypad decimal key name`
- `9769234 refactor(renderer): drive passes from explicit attachment descriptions`
- `975c4da feat(renderer): add generational pick identities`
- `4a4bd1d feat(renderer): add asynchronous GPU object picking`
- `a3ea1e2 feat(core): expose allocation-free frame metrics`
- `55d5f4b refactor(core): move scene ownership into the application layer`
- `3c2fd0b feat(math): add conservative frustum intersection primitives`
- `0c286c3 perf(renderer): cull invisible world geometry per view`
- `0bea717 fix(tests): support builds without memory tracking`
- `6641050 fix(renderer): publish picking targets transactionally`

La adaptación conserva `RenderPassKind` sólo como routing semántico de alto
nivel. La compatibilidad real de pipelines se valida con
`RenderPassSignature`, que ahora también tiene un hash estable independiente
del padding de C++. Las vistas poseen configuración y handles generacionales;
los objetos Vulkan de los targets permanecen en el backend y se publican de
forma transaccional. Dado que hoy sólo existen world, UI y el pass especial de
picking, cada vista representa un pass ordenado. Un render graph multi-pass se
pospone hasta que exista un consumidor real que justifique esa complejidad.

### Evidencia de validación Linux

| Comprobación | Resultado |
|---|---|
| Debug | 359 tests de 73 suites aprobados. |
| Release | Build completa y 349 tests de 72 suites aprobados; las 10 pruebas adicionales dependen intencionalmente del tracking desactivado en Release. |
| ASan/UBSan | 359 tests aprobados sin error de address ni undefined behavior. LeakSanitizer externo no puede arrancar bajo el `ptrace` del harness; el tracker interno del engine informó 0 fugas. |
| Nix | `nix flake check` aprobado para `x86_64-linux`. |
| Wayland/niri moderno | 660 frames, dynamic rendering + synchronization2, 0 VUID y 0 allocations en 606 frames estables. |
| Wayland/niri legacy | 660 frames, render pass + barreras clásicas, 0 VUID y 0 allocations en 643 frames estables. |
| Dependencias | `.gitmodules` y `.scripts/libraries.csv` no cambiaron; no se añadió Clay, GLFW, MoltenVK ni una dependencia nueva. |

### Benchmark Release de culling

La misma cámara y ventana de 936×999 se midieron durante 600 frames después de
60 frames de warm-up:

| Ruta | CPU/frame | FPS | GPU/frame | Candidatos | Visibles | Culled | Draws |
|---|---:|---:|---:|---:|---:|---:|---:|
| Moderna, culling off | 2.277 ms | 439.15 | 1.963 ms | 29 | 29 | 0 | 32 |
| Moderna, culling on | 2.265 ms | 441.56 | 1.935 ms | 29 | 25 | 4 | 28 |
| Legacy, culling on | 2.215 ms | 451.48 | 1.916 ms | 29 | 25 | 4 | 28 |

En esta posición se eliminó el 13.8 % de los candidatos world y el 12.5 % de
los draws totales. La diferencia temporal moderna es pequeña (aproximadamente
0.5 % en CPU/frame y 1.4 % en GPU/frame), como era esperable con sólo 29
candidatos y bounds grandes. Los números son una medición local, no una promesa
de rendimiento entre drivers o ejecuciones.

La ruta Win32 conserva las equivalencias de input y no recibió código de
plataforma nuevo, pero su compilación y smoke Windows 11/NVIDIA permanecen
pendientes porque este entorno Linux no dispone del toolchain ni del host
Windows necesarios.

## Validación transversal

Después de cada commit importante:

- Build Debug mediante el flake y suite completa de tests.
- Tests nuevos del cambio y regresiones de memoria/containers/result.
- Formato y análisis estático configurados por el proyecto.

Al cerrar el tramo:

- Suite Debug completa; la baseline actual es 331 tests y deberá crecer, nunca
  disminuir silenciosamente.
- Suite ASan/UBSan con detección de fugas.
- Build Release y `nix flake check`.
- Smoke Wayland/xdg-shell bajo niri de al menos 600 frames en ruta moderna y
  con `NK_VULKAN_LEGACY=1`, incluyendo resize, minimize/restore, picking y
  recorrido de culling.
- Validation Layers sin VUID y cierre limpio con ESC.
- Auditoría de frames estables: cero allocations del heap general.
- Compilación de la ruta Win32 y posterior smoke en Windows 11/NVIDIA.
- Búsqueda que confirme que no se añadió GLSL, GLFW, macOS ni Clay.
- `.gitmodules` y `.scripts/libraries.csv` permanecen sin cambios salvo que una
  dependencia haya sido discutida y aprobada expresamente.

## Estrategia de regresión y rollback

| Riesgo | Prevención | Rollback operativo |
|---|---|---|
| Descriptores incompatibles rompen pipelines | Signature validada y tests moderno/legacy antes de migrar todos los passes. | Revertir el commit atómico de configuración; no mantener dos APIs públicas indefinidamente. |
| Resize deja attachments parciales | Construcción temporal y swap sólo al completar. | Conservar la generación/targets anteriores y omitir el frame. |
| Picking introduce stalls | Readback por frame y consumo tras fence ya retirado. | `NK_PICKING=0` elimina pass/copia sin tocar render principal. |
| Resultado selecciona un objeto reutilizado | Id generacional, request y scene revision. | Descartar stale; nunca resolver por índice solamente. |
| Separación App/Engine altera teardown | Tests de lifecycle y cancelación/drenaje de jobs. | Revertir el commit de migración como unidad, sin ownership dual. |
| Culling produce popping | AABB conservadora, oráculo de clip space y fail-open. | `NK_FRUSTUM_CULLING=0` conserva todos los candidatos. |
| Optimización no mejora Sponza | Métricas de candidatos/draws y benchmark reproducible. | Mantener el sistema correcto y desactivable; no añadir estructuras espaciales sin medir. |

Los toggles son herramientas de diagnóstico, no sustitutos de corregir errores.
Una vez estabilizada cada función se decidirá si permanecen como opciones de
runtime o sólo como configuración de desarrollo.

## Fuera de alcance

- Port a macOS, Cocoa, Objective-C, MoltenVK o GLFW.
- Integración de Clay, widgets, editor visual o gestión de foco completa.
- Picking CPU por ray casts, selección de primitivas/triángulos o gizmos.
- Readback síncrono por evento, `vkDeviceWaitIdle` o una queue exclusiva sólo
  para leer un píxel.
- Occlusion queries, Hi-Z, BVH, octree, portals, meshlets o GPU-driven culling.
- Cambio global de convención de profundidad GLM/Vulkan.
- Reimplementación completa o incorporación directa de NoGraphicsAPI.

## Checklist de cierre

- [x] Capítulo 76 auditado y exclusiones documentadas.
- [x] Passes y attachments son configuración renderer-neutral real.
- [x] Vistas poseen configuración estable; el backend publica sus targets
  window-sized transaccionalmente.
- [x] Identidades seleccionables son tipadas y generacionales.
- [x] Picking `R32_UINT` usa Slang y readback sin stalls globales.
- [x] Engine y aplicación tienen ownership/lifecycle separados.
- [x] Métricas tienen una única fuente allocation-free.
- [x] Frustum culling es conservador, por vista y medible.
- [x] Vulkan moderno y legacy pasan validación bajo niri.
- [ ] Win32 compila y queda listo para el smoke Windows/NVIDIA.
- [x] No se añadió Clay ni ninguna dependencia innecesaria.
- [x] Cada cambio importante quedó en un commit semántico independiente.
