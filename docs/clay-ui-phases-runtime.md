# NK UI: fases 0–3, integración nativa y consumidores

Revisión: 2026-09-12. Todas las tareas están pendientes.
[Plan principal](clay-ui-language-implementation-plan.md) ·
[Contratos C01–C11](clay-ui-technical-contracts.md) ·
[Siguiente tramo: lenguaje](clay-ui-phases-language.md).

Los IDs `P0.1`, etc. identifican paquetes de trabajo y evidencia, no commits
obligatoriamente indivisibles. Cada paquete se implementa con tests junto al
código. No ejecutar todavía estos cambios por existir este documento.

## Fase 0 — Contratos ejecutables y baseline

**Entrada:** checkout actual compilable o fallos existentes inventariados;
ninguna dependencia nueva. **Riesgo:** medio, por lifetimes e input transversal.
**Salida:** decisiones mínimas verificadas, harness y escenarios reproducibles.

Superficie de cambio prevista: `engine/include/core/app.h`,
`engine/src/core/engine.cpp`, `engine/include/core/frame_metrics.h`, tests de
app/input/texto, `benchmarks/` y documentación. En esta fase no migrar la escena.

### Paquetes de trabajo

- [ ] **P0.1 — Auditoría de ownership y estado.** Registrar quién inicializa,
  utiliza y destruye fuentes, atlas, overlay, input y recursos GPU. Anotar
  lifetimes de `FrameBuilder`, resultado de `TextRenderer::prepare`, contenido
  de `TextInputState` y orden real de `App::update/build_frame/frame_complete`.
  Producto: mapa «actual → propuesto» enlazado a C02/C03, no sólo diagramas ideales.
- [ ] **P0.2 — ADRs de arranque.** Fijar dueño de `TextServices`, momento de
  despacho de acciones, política de fallo de superficie y contrato de color.
  Especificar API mínima de builder/surface/draw sin headers Clay/Vulkan públicos.
  Firma y ejemplo C++ pequeño deben permitir allocator explícito y early return.
- [ ] **P0.3 — Harness de UI.** Configuración demo seleccionable, escenas
  deterministas y input reproducible; no depender de cargar Sponza para tests
  unitarios. Guardar tamaño, escala, cámara, fuentes y semilla de cada fixture.
  Añadir captura/inspección de comandos CPU antes de exigir screenshots GPU.
- [ ] **P0.4 — Instrumentación.** Separar CPU layout, texto, comandos, uploads,
  draw calls y memoria. Medir baseline del overlay actual y de escena sin UI.
  Reutilizar `.scripts/benchmark.sh` cuando corresponda; definir datos faltantes
  en vez de atribuir toda diferencia de FPS a Clay.
- [ ] **P0.5 — Presupuestos y protocolo de evidencia.** Configuración de límites
  C10, tests de OOM con allocators fallibles y formatos de métricas. Fijar valores
  iniciales con tamaños reales; distinguir reserva inicial de máximo permitido.

### Fixtures de referencia

| ID | Escenario | Defecto que debe detectar |
| --- | --- | --- |
| R01 | Fondo → texto → imagen → modal → texto del modal. | Reordenamiento de transparencias o texto siempre encima. |
| R02 | Tres clips, uno vacío y restauración al salir. | Scissor heredado incorrectamente y hits fuera del clip. |
| R03 | Escala 1, 1.25, 1.5 y 2; resize repetido. | Doble aplicación de DPI, drift y UV/picking incorrectos. |
| R04 | Misma fuente/texto en dos superficies con distintos tamaños. | Caché con clave insuficiente y atlas duplicado. |
| R05 | Presión de nodos/comandos/atlas/candidato y OOM. | Lista parcial, abort inesperado o referencias colgantes. |
| R06 | Suspender/reanudar, pérdida de foco y cierre por ESC. | Input atascado o shutdown con memoria viva. |

### Aceptación y cierre G0

- Existe un informe con SHA, hardware, driver, toolchain y comandos reales;
  resultados inexistentes aparecen como «pendiente», no «aprobado».
- Los tests del harness distinguen orden de comandos y lifetimes sin GPU.
- La propuesta no precisa inicializar UI antes de memoria ni un nuevo allocator
  global. `str` sigue con allocator primero.
- No hay regresión intencional de escena/input. El coste del harness desactivado
  no afecta al ejecutable normal.

**Rollback:** cambios de instrumentación aislados y desactivables; no revertir
arreglos del usuario ni la implementación de texto existente.
**Commits sugeridos:** `test(ui): add deterministic rendering fixtures`,
`feat(metrics): measure UI preparation costs`.

## Fase 1 — Clay desde C++ y rendering ordenado

**Entrada:** G0 y contratos C02/C04. **Riesgo:** alto por orden visual,
font caches y recursos en vuelo. **Salida:** UI C++ dibujada por nuestro renderer.

Archivos existentes afectados: `engine/CMakeLists.txt`, `.gitmodules`,
`.scripts/libraries.csv`, `flake.nix`, `flake.lock`,
`engine/include/text/text_overlay.h`, `engine/src/text/text_overlay.cpp`,
`engine/include/renderer/render_packet.h`, `engine/include/renderer/text_renderer.h`,
`engine/src/renderer/text_renderer.cpp`, `engine/src/renderer/vulkan/text.cpp`,
`engine/src/renderer/vulkan/vulkan_renderer.cpp`, `editor/src/editor.cpp`.
Nuevos módulos propuestos: `engine/include/ui/`, `engine/src/ui/`, renderer UI y
shaders Slang correspondientes. Tests dentro de `tests/src/ui/` y renderer.

### P1.A — Dependencia y adaptador aislados

- [ ] **P1.1 — Pin reproducible.** Verificar tag Clay elegido, SHA/licencia,
  submódulo y registro CSV; el flake consume el mismo gitlink. Compilar una sola
  unidad `CLAY_IMPLEMENTATION`; evitar contaminar headers del engine con macros.
  Verificar C++20 y compiladores usados, sin introducir backend de ejemplos.
- [ ] **P1.2 — Contexto y memoria.** Wrapper `UiSurface` con límites antes de
  calcular arena, callback de error, resize, destrucción y restauración de
  contexto. Fallo del callback se propaga al resultado del builder, no queda sólo
  en un log. Ensayar dos contextos y un retorno temprano dentro de un scope.
- [ ] **P1.3 — Builder público.** Métodos C++ para cajas/texto/imágenes y scopes
  balanceados. Identidades explícitas, rangos de texto con longitud y handles de
  recursos. No depender de `Clay__*` fuera del adaptador privado; si se necesitan
  internals, registrar cuáles y cubrirlos contra el tag fijado.

**Gate parcial G1A:** layouts CPU y error de capacidad reproducibles sin Vulkan;
ningún contexto/cadena sobrevive a su memoria. No afirma que exista rendering.

### P1.B — Servicios comunes de texto

- [ ] **P1.4 — Extraer ownership.** Un `TextServices` con lifecycle explícito;
  overlay deja de poseer sus propios servicios. Exponerlo a apps/UI con tests de
  shutdown parcial, headless y UI deshabilitada. No crear un atlas por panel.
- [ ] **P1.5 — Medición y shaping.** Adaptar `Clay_StringSlice` a `strview` sin
  asumir NUL; normalizar opciones/font size según representación admitida.
  Contrastar medida, wrap, ascender y línea dibujada; invalidar cachés NK y Clay
  ante fuente, idioma, dirección, escala o revisión de métricas relevantes.
- [ ] **P1.6 — Atlas multiconsumidor.** Acumular todas las listas antes de capturar
  uploads, preservar dirty regions si se omite el frame, retener páginas mientras
  existan draws. Tests con la segunda superficie introduciendo glyphs nuevos.

**Gate parcial G1B:** dos consumidores comparten fuente/atlas, sin dirty rects
perdidos ni diferencias medida/dibujo en fixtures cubiertos.

### P1.C — Lista visual y backend

- [ ] **P1.7 — `UiDrawList`/`UiFrame`.** Rect/border/image/text/clip tipados,
  orden estable y validación de rangos. Agregar camino de packet nuevo y un
  adaptador temporal del overlay; no ordenar por textura atravesando otros draws.
- [ ] **P1.8 — Renderer Slang.** Primitivas, alpha, UV y borde/radio visual;
  intercalar batches de texto sin duplicar rasterización. Clip rectangular con
  stack e intersección; radios no implican clip redondeado de hijos todavía.
- [ ] **P1.9 — Buffers y sync.** Reutilizar slots por frame, crecimiento fuera
  del hot path, uploads agrupados y retirement según finalización. Validar tanto
  dynamic rendering/synchronization2 como passes/barreras legacy; ningún requisito
  de bindless o GPU address por UI.
- [ ] **P1.10 — Demo y transición.** Migrar el overlay de diagnóstico al camino
  nuevo con comparación A/B; mantener un fixture del camino anterior durante la
  validación, sin mantener indefinidamente dos APIs públicas de UI en producción.

### Aceptación y cierre G1

- R01–R06 pasan en el alcance de rendering; orden CPU exacto y capturas con
  tolerancia documentada. Bordes, glyphs e imágenes tienen alpha coherente.
- Alternar dos superficies, redimensionar y omitir frames no filtra memoria ni
  invalida atlas; apagado limpia font handles y recursos antes del renderer.
- Frames calientes dentro de capacidad no hacen allocations en heap general.
  Casos que crecen tienen coste y fallo explícitos.
- Linux/Wayland con Vulkan moderna/legacy probado. Win32 se registra por separado;
  si no hay ejecución disponible, gate de portabilidad sigue pendiente.

**Rollback:** seleccionar temporalmente overlay anterior; retirar por separado
adaptador/draw path sólo después de guardar fixtures y métricas. No borrar fuentes.
**Commits sugeridos:** `build(deps): pin Clay layout library`,
`refactor(text): share font and atlas services`,
`feat(renderer): render ordered UI commands`.

## Fase 2 — Input, widgets y edición

**Entrada:** G1; C03/C05/C10. **Riesgo:** alto, por polling existente, IME y Unicode.
**Salida:** controles reales con foco y edición, no sólo elementos dibujados.

Modificar `engine/include/core/input.h`, `engine/src/core/input.cpp`,
`engine/src/systems/input_system.{h,cpp}`, `engine/src/core/engine.cpp`,
`engine/src/platform/platform.h` y backends `platform_wayland`/`platform_win32`.
Extender tests de input, UTF-8 y sistemas; nuevos widgets en `engine/src/ui/`.

### P2.A — Enrutamiento e identidad

- [ ] **P2.1 — Snapshot y eventos.** Eventos ordenados con serial/target/consumo,
  coordenadas UI sin truncado a i16, wheel acumulado con precisión suficiente.
  Conservar raw polling y añadir la vista filtrada usada realmente por cámara/app.
- [ ] **P2.2 — Foco y captura.** Identidad generacional, hit order/clip, hover,
  capture, focus traversal y cancelación. El motor despacha antes de gameplay;
  un relayout no repite acciones. Resize/reload invalidan geometría antigua.
- [ ] **P2.3 — Widgets de acción.** Button, toggle, slider, scroll, tooltip y
  modal con estados normal/hover/pressed/focused/disabled. Teclado y mouse dan
  las mismas acciones; rol/etiqueta/foco visible separados del dibujo.

**Gate parcial G2A:** controles activables y input aislado. Es suficiente para
integrar templates interactivos de fase 5; no declara completo el campo de texto.

### P2.B — Documento editable y plataforma

- [ ] **P2.4 — Modelo de edición.** Buffer UTF-8 con allocator, selección ancla/
  cursor, revisión y transacciones insert/delete/replace. Límites por bytes,
  cursor por grapheme; undo/redo acotados con política de agrupación explícita.
  Composición es un overlay editable temporal, no texto committed duplicado.
- [ ] **P2.5 — Unicode verificable.** Usar corpus UAX #29 de versión fijada y
  pruebas de mapping grapheme/cluster/byte. Elegir solución especializada si las
  librerías actuales no cubren segmentación; ADR, tag y CSV antes de integrarla.
  No afirmar bidi completo por renderizar una cadena RTL explícita.
- [ ] **P2.6 — API de clipboard/IME.** Requests asíncronos, rango surrounding,
  caret rect y propósito de input; estados unsupported/denied/cancelled. Evitar
  waits en el frame; resultados llevan foco y revisión, no sólo un puntero al widget.
- [ ] **P2.7 — Backends nativos.** Implementar Wayland y Win32 sin GLFW: adaptar
  índices UTF-16/UTF-8, seriales, cancelación y permisos del protocolo usado.
  Guardar evidencias de preedit/update/commit/cancel, copy/paste y focus-out.
- [ ] **P2.8 — Input monolínea.** Caret/selección y desplazamiento horizontal,
  home/end, delete/backspace, atajos, validación y límites. Pegar múltiples líneas
  sigue política explícita; validar antes de alterar el documento/undo.

### Aceptación y cierre G2

- Tests reproducen doble click/press/release entre frames y no pierden acciones.
  Arrastrar un slider fuera del rect mantiene captura; eliminarlo la cancela.
- Typing en input no mueve cámara; al perder foco las teclas no quedan presionadas.
  No cambiar implícitamente la política temporal de ESC.
- Edición de tildes, combinantes y secuencias emoji cubiertas no parte graphemes.
  Casos fuera del perfil tienen limitación explícita, no corrupción del buffer.
- Preedit no aparece dos veces; paste que llega a un foco viejo se descarta.
  OOM/rechazo de edición conserva texto, selección e historial anterior.
- Validación nativa Linux/Win32 separada de unit tests del modelo puro.

**Rollback:** widgets/editing detrás de configuración; restaurar consumidor de
input de escena mediante su adaptador, sin eliminar el estado crudo de plataforma.
**Commits sugeridos:** `feat(input): route UI events before gameplay`,
`feat(ui): add grapheme-safe text editing`, `feat(platform): integrate UI clipboard requests`.

## Fase 3 — Game UI y editor con viewport

**Entrada:** G1 + G2A para P3.A/B; G2 completo para entregar inspector editable.
**Riesgo:** alto en targets/sync/picking, medio en composición de pantallas.
**Salida:** dos consumidores del mismo toolkit, sin editor acoplado al juego.

Archivos actuales: `editor/src/editor.cpp`, `engine/include/core/app.h`,
`engine/src/renderer/render_view.{h,cpp}`, `engine/include/renderer/render_packet.h`,
render targets/textures/picking y sus implementaciones Vulkan. Añadir demo de game
o modo de demo con dependencias comprobadas, no sólo llamarlo «game» en un panel.

### P3.A — Separación de consumidores

- [ ] **P3.1 — Paquetes y roots.** HUD/menú compartible y shell del editor en
  módulos separados. Servicios inyectados; no acceder desde widgets a la clase
  editor ni duplicar widgets de juego con otro sistema de input.
- [ ] **P3.2 — Acciones del host.** Toolbar/jerarquía mínima/inspector enlazados
  a handles de selección y comandos del host. UI refleja estado; no modifica
  memoria de una entidad mediante dirección guardada en un nodo.

### P3.B — Viewport real

- [ ] **P3.3 — API neutral por vista.** Tamaño, cámara, target y generación
  independientes. Extender la limitación actual world/UI + `RenderPassKind`
  sólo lo necesario; no implementar un render graph general como prerrequisito.
- [ ] **P3.4 — Offscreen y sampling.** Render color/depth del viewport, hacer
  color muestreable en UI, resize candidato y retiro diferido del target anterior.
  Testear cambio de extensión manteniendo varios frames en vuelo.
- [ ] **P3.5 — Input/picking transformados.** Coordenada de panel → contenido
  visible → texel del target; tomar en cuenta scale/letterbox/clip. Requests de
  picking llevan vista, generación y revisión de escena; rechazar readback viejo.
- [ ] **P3.6 — Integración final.** HUD/menú del juego, paneles editor y viewport
  simultáneos; inspector modifica una propiedad por comando y presenta el resultado.
  Resize a cero, ocultar panel y restaurar no recrea targets cada frame.

### Aceptación y cierre G3

- Selección corresponde al objeto bajo el cursor dentro del viewport con DPI
  fraccional; paneles fuera de él no generan picks sobre la escena.
- Camera control no atraviesa modales/inputs; escena responde al input permitido.
- Ejecutable de game no enlaza shell/inspector/editor tools. Recursos compartidos
  conservan una sola propiedad de memoria y no exigen un contexto por widget.
- Pasar a render-to-texture no añade stalls globales o VUIDs en moderna/legacy.
  Artefactos muestran escena, selección y UI, no sólo tests del descriptor.

**Rollback:** viewport directo a ventana como modo de diagnóstico mientras se
estabiliza offscreen; mantener separación de servicios y tests del toolkit.
**Commits sugeridos:** `feat(renderer): support sampled scene view targets`,
`feat(editor): compose scene viewport with shared UI widgets`.

## Evidencia a adjuntar al cerrar cada gate

Archivo propuesto por fase: `docs/evidence/clay-ui/phase-N.md`, creado al ejecutar
el trabajo, no rellenado ahora con resultados supuestos. Debe contener:

- IDs completados y commits, contratos alterados y razones.
- Comandos exactos, outputs relevantes, entorno y plataformas realmente ejecutadas.
- Fixtures antes/después, allocations/picos/CPU/GPU y limitaciones conocidas.
- Gate completo o parcial; tarea que bloquea el resto y paso seguro para continuar.
- Instrucciones de rollback y confirmación de que el modo anterior sigue válido.
