# NK UI: fases 7–10, recarga, evolución y distribución

Revisión: 2026-09-10. Todas las tareas están pendientes.
[Plan principal](clay-ui-language-implementation-plan.md) ·
[Contratos](clay-ui-technical-contracts.md) ·
[Tramo anterior](clay-ui-phases-language.md).

La recarga se divide deliberadamente en datos y código. Fase 9 no necesita
esperar fase 8: sus mejoras visuales se pueden validar sobre AOT y reload de datos.
El cierre final exige todas las entregas obligatorias y sus pruebas por plataforma.

## Fase 7 — Build incremental y recarga de datos

**Entrada:** G6, fixtures AOT y contratos C08/C09. **Riesgo:** alto por versiones,
dependencias, estado y recursos. **Salida:** editar UI/CSS sin reiniciar el proceso.

Nuevas piezas: empaquetador/loader `.nkuib`, grafo incremental y watch en
`tools/ui-compiler/`; activación/runtime en `engine/src/ui/`; servicios nativos
de filesystem watch/proceso bajo plataforma o herramientas sin dependencia de
ventana. Integrar CMake/Nix y scripts `.sh`/`.ps1` de igual comportamiento.

### P7.A — Formato y coherencia con AOT

- [ ] **P7.1 — Formato versionado.** Documentar secciones, offsets/alineación,
  endianness, capacidades, schema IDs, hashes y límites. Reader sobre slices
  validadas; prohibir reinterpret_cast de archivo a estructuras con padding.
- [ ] **P7.2 — Ejecución común.** Cargar programa de datos al mismo modelo usado
  por tablas AOT, no a un toolkit distinto. Validar todos los símbolos y tipos
  disponibles antes de montar. Nueva lógica nativa se informa como rebuild
  requerido, no se transforma en C++ interpretado silenciosamente.
- [ ] **P7.3 — Differential tests.** Mismo input/tiempo/assets a AOT y datos:
  comparar árboles efectivos, eventos, bounds, estilos y draws normalizados.
  Fuzzear formato truncado, offsets solapados, índices, capacidades e imports.

### P7.B — Invalidación y herramientas

- [ ] **P7.4 — Grafo incremental.** Claves de C08 por bloque y dependencia;
  interfaces/schema/Clang/toolchain/configuración forman parte de caché. No-op
  sin writes ni proceso Clang. Registrar motivo exacto de invalidación, si exige
  reflect/native compile y cuántos componentes se recompilaron.
- [ ] **P7.5 — File watcher.** Watch de directorios para guardado mediante rename;
  altas/bajas, paths con espacios/UTF-8, cambio de symlink y overflow de eventos.
  Debounce configurable, rescan/reconciliación y cancelación de jobs obsoletos.
  No usar sleeps o polling pesado dentro del hilo de frame.
- [ ] **P7.6 — Worker y protocolo.** Build fuera del hilo UI y límite de trabajos
  concurrentes/candidatos. Manifest de snapshot: si las fuentes cambiaron mientras
  se leían, reintentar o rechazar. Trasladar payloads/diagnósticos por memoria
  propia y IDs; ninguna referencia al stack del worker.
- [ ] **P7.7 — Flujo de desarrollo.** Comandos propuestos `nk-uic build/watch`
  con mismo manifiesto; scripts Bash/PowerShell equivalentes. Artefactos en build
  mutable; tool host empaquetado por Nix, outputs nunca en el store. Build nativo
  ni `nk-ui-reflect` se invocan ante cambios exclusivamente UI/CSS, y se prueba
  contando ambos procesos.

### P7.C — Publicación y conservación de estado

- [ ] **P7.8 — Candidato listo.** Resolver assets/fonts/schema, memoria y plan de
  reconciliación. Requests de recursos asíncronos llevan generación. Si un recurso
  requerido falla, conservar programa actual; placeholders sólo donde sean parte
  del contrato del asset, no para ocultar fallo de toda la actualización.
- [ ] **P7.9 — Commit de datos.** Máquina C09, swap entre frames y eventos
  coherentes con C03. Conservar scroll/foco/selección/estado compatible por key;
  desmontar retirados y cancelar captura/IME. Rechazar resultado desactualizado.
- [ ] **P7.10 — Retirement y diagnóstico.** Retirar programa/chains/font refs tras
  último consumidor, GPU resources tras completion. Mostrar estado compiling/
  ready/rejected/active y error con origen en overlay residente que no dependa
  del componente que no compiló.

### Aceptación y cierre G7

- Cambiar color, padding, texto, orden de elementos keyed e import compartido
  actualiza pantalla manteniendo estado compatible, sin invocar compilador C++.
- Error de sintaxis, tipo, OOM, import borrado o resource load fallido deja la
  última UI válida. Corregirlo recupera la recarga sin reiniciar.
- Guardados sucesivos no aplican un job viejo; snapshot de dependencias se
  verifica aun con debounce. Renombrar/borrar no deja watch entries eternos.
- Cien recargas sin crecimiento sostenido de memoria/recursos, incluyendo
  clipboard/IME/captura activa y frames omitidos. Prueba prolongada aparte.
- p50/p95 guardar→visible y desglose registrado; budgets del plan principal
  comparados con medición. La UI estable no queda bloqueada esperando al worker.

**Rollback:** desactivar sólo data reload y arrancar el paquete AOT conocido;
conservar logs del candidato fallido. Fallo de feature visual no justifica un
loader permisivo que ignore campos desconocidos.
**Commits sugeridos:** `feat(ui-compiler): rebuild dependent component programs`,
`feat(ui): activate UI program updates transactionally`.

## Fase 8 — Recarga nativa de C++

**Entrada:** G7 y C09; toolchains Linux/Windows reales para cerrar portabilidad.
**Riesgo:** muy alto: un ABI inválido o callback colgante puede derribar el proceso.
**Salida:** hot reload bajo contrato de scripts UI, no de cualquier C++ del engine.

Nuevas piezas: SDK de bindings/host API de desarrollo, loader `.so`/DLL,
reflexión Clang + compilación/link incrementales y coordinador de migración.
Mantener privados los tipos dinámicos del módulo y los hooks de plataforma fuera
del renderer; `UiSchema` acompaña siempre a la generación nativa que describe.

### P8.A — Spike de ABI y loaders

- [ ] **P8.1 — SDK estrecho.** Versionar entrada, tamaños de tablas, result codes,
  valores/handles y servicios permitidos. No exportar vtables/contenedores NK ni
  cruzar exceptions/RTTI. Factory allocator-first usa wrapper de callbacks de
  allocation host; nunca hacer free en un CRT distinto.
- [ ] **P8.2 — Compatibilidad.** Fingerprint de plataforma, arquitectura,
  frontend Clang/plugin, toolchain target/runtime, configuración, defines de
  tracking, ABI y schema. Rechazar antes de llamar factories. Diferenciar formato
  UI vs schema vs ABI nativo; metadata de otro Clang no se reutiliza en silencio.
- [ ] **P8.3 — Spike mínimo.** Un componente contador en módulo independiente,
  cambio de handler y campo, carga de generación única y unload limpio. Comparar
  implementación pequeña con contratos de RCC++ como referencia; no instalar una
  librería de recarga sin justificar ahorro e integración de allocators.
- [ ] **P8.4 — Loader por plataforma.** Estrategia de exports/visibilidad, paths,
  búsqueda de dependencias controlada y nombres distintos por generación; Linux
  y Win32. En Windows incluir PDB/artefactos bloqueados en política de limpieza.
  No sobrescribir librería cargada ni depender de interposición accidental.

**Gate parcial G8A:** spike demuestra carga, rechazo de ABI y descarga de un
módulo mínimo en cada plataforma; no habilitar módulos de gameplay arbitrarios.

### P8.B — Estado y transacción

- [ ] **P8.5 — Schema de estado.** IDs/tipos/versiones de campos
  `[[nkui::state]]` extraídos por Clang y serialización de tipos soportados.
  Defaults para nuevos, descarte explícito de eliminados, adaptador para
  incompatibles; strings/listas reconstruidos con allocator del destino. Un
  rename sin ID estable se reporta como remove+add, no se adivina por posición.
- [ ] **P8.6 — Snapshot actual.** Capturar en frontera segura después del build,
  no guardar el contador de hace dos segundos cuando empezó a compilar. Si se
  migra fuera del hilo propietario, revalidar revisión antes de commit.
- [ ] **P8.7 — Prepare/commit.** Instancias nuevas sin side effects, reserva y
  validación completas; publicar módulo/programas/modelos como unidad. Migration
  failure deja todo el activo; no half-swap con handlers nuevos y state viejo.
- [ ] **P8.8 — Destrucción/retirada.** Bloquear nuevas entradas al saliente,
  drenar callbacks/jobs permitidos y cancelar subscripciones. Invocar destructores
  antes de unload; GPU sólo retiene datos/handles host, nunca código del módulo.
  Política explícita de no recarga si no puede retirarse; no acumular módulos.

### P8.C — Flujo y límites del usuario

- [ ] **P8.9 — Build de scripts.** Objetos/módulos afectados según depfiles del
  frontend Clang y compilador, flags equivalentes al target/host SDK, caché y maps
  a `.nkui`. Pipeline reflect → validate → generate → compile/link publica schema
  y módulo como candidato indivisible. No reconstruir engine/FreeType/Clay al
  cambiar un handler pequeño; un cambio sólo de body puede reutilizar schema si
  AST público y dependencias semánticas producen exactamente el mismo hash.
- [ ] **P8.10 — UX de desarrollo.** Rejected/needs-restart con razón, última
  versión válida visible y opción de reinicio controlado. Compilar módulos sólo
  de proyectos locales confiables; sin ejecución automática de código descargado.
- [ ] **P8.11 — Pruebas destructivas controladas.** En procesos de test aislados:
  ABI incorrecto, factory/OOM/migration failure, callback pendiente, close durante
  compile y cancelación. Documentar que UB/crash nativo no tiene rollback garantizado.

### Aceptación y cierre G8

- Incrementar mientras compila y recargar conserva el valor más reciente, no
  una copia tomada al inicio. Campo incompatible no se copia con `memcpy`.
- Un error de compile/link/migrate no cambia el módulo activo; un cambio del
  engine/ABI informa reinicio, no intenta hacerlo pasar como hot reload compatible.
- Un schema generado desde otra versión de script/plugin o un módulo cuyo export
  no coincide se rechaza antes de construir instancias; nunca hay handlers nuevos
  operando sobre la descripción anterior.
- Cien ciclos y cierre durante compilación: sin callbacks a librerías descargadas,
  módulos filtrados, memoria viva de CRT incorrecto ni resources GPU retenidos.
- Evidencia real de loader Linux y Win32. Medir reflect/schema, codegen, compile,
  link, load, migrate y retire por separado; objetivo de latencia no equivale a
  prometer C++ instantáneo.

**Rollback:** deshabilitar native reload; conservar data reload de G7 y scripts
compilados al arrancar. Mantener restart controlado como camino soportado.
**Commits sugeridos:** `feat(ui): load versioned native component modules`,
`feat(ui): migrate component state during native reload`.

## Fase 9 — Evolución visual y herramientas propias del editor

**Entrada:** G2/G3/G6/G7; G8 no es dependencia técnica. **Riesgo:** medio-alto
por clipping/transform/caches. **Salida:** ampliación acotada y medible de UI.

Alcance obligatorio de este hito: inspector de UI, listas virtualizadas de altura
fija, images contain/cover, unidades relativas acotadas, clip redondeado y
transiciones de propiedades visuales explícitamente soportadas. No «elegir algo»
al final para marcar la fase completa.

### Paquetes de trabajo

- [ ] **P9.1 — Inspector de UI.** Árbol de instancias/keys, bounds/clip, estilos
  computados con origen, tokens/fonts, memory y tiempos. Click en diagnóstico
  identifica archivo/span. Vista inspector no modifica estado mientras lo enumera.
- [ ] **P9.2 — Listas/árbol.** Virtualización de filas de altura conocida con
  overscan configurable; IDs no dependen del slot reciclado. Preservar foco y
  edición de fila fuera de viewport según política explícita. Árbol del editor
  usa lista aplanada y keys, sin requerir recursión infinita del lenguaje.
- [ ] **P9.3 — Unidades y tamaño.** `em`/`rem` y unidades de superficie con raíces
  definidas; `calc()` acotado a expresiones tipadas sin ciclos. Añadir min/max
  queries por tamaño de superficie, no un parser general de media queries web.
- [ ] **P9.4 — Imágenes.** `contain`/`cover` con clipping/UV correctos y placeholder
  de asset definido. No confundir background-size con tamaño intrínseco del layout.
- [ ] **P9.5 — Clip redondeado.** Spike de dos estrategias compatibles con nuestra
  GPU base (por ejemplo máscaras analíticas acotadas frente a máscara offscreen).
  Decidir por calidad, nesting, coste y legacy. Aplicar igual clip a paint/hits;
  no anunciar capacidad sólo porque se redondea el fondo.
- [ ] **P9.6 — Transiciones.** Color y transform 2D translate/scale con duración,
  interpolación/reemplazo definidos y clock de frame; hit test con inversa del
  transform. Opacity de grupo sólo si se compone como grupo: multiplicar alpha
  de cada hijo no equivale a CSS opacity en elementos superpuestos. Si no se
  implementa composición, limitar a hojas y diagnosticar el resto.
  La [semántica de opacidad de grupo](https://www.w3.org/TR/css-color-4/#transparency)
  es la referencia para este caso; la estrategia GPU sigue siendo propia de NK.
- [ ] **P9.7 — Controles compuestos.** Selector, tabs e inspector con undo/redo
  mediante comandos del host. Añadir `v-model` únicamente como azúcar de prop +
  evento tipados comprobados, sin segundo sistema de estado reactivo.

### Backlog explícito fuera del cierre G9

Flexbox completo/Grid, layout inline de navegador, docking/multiventana,
sombras/gradientes avanzados, rotación 3D, filtros, keyframes generales, SVG/HTML,
listas de altura arbitraria, tablas complejas, editor de código completo y
accesibilidad del SO. Cada ampliación necesita alcance/tests propios; no introducir
un nuevo solver o fork grande de Clay sin volver a decidir.

### Aceptación y cierre G9

- Inspector muestra reglas correctas y no necesita parsear UI en cada frame.
  Virtualizar 10.000 filas reduce nodos/draws a visibles + overscan y mantiene keys.
- R02/R03 ampliados con clips redondeados y transforms: paint e input coinciden
  y siguen funcionando en moderna/legacy. Recursos de máscaras tienen presupuesto.
- Transiciones son deterministas con clock de prueba; resize/reload no deja
  animaciones referenciando instancias retiradas. Desactivarlas restaura estilo final.
- Cada propiedad nueva tiene entry de matriz, diagnóstico de límites y benchmark;
  no se declara más CSS del realmente soportado.

**Rollback:** feature flags por extensión visual/cache; volver a perfil G6 válido,
no aceptar silenciosamente propiedades cuya implementación se desactivó.
**Commits sugeridos:** `feat(editor): inspect UI layout and style sources`,
`feat(ui): virtualize keyed fixed-height lists`,
`feat(renderer): support nested rounded UI clipping`.

## Fase 10 — Tooling, distribución y cierre

**Entrada:** gates obligatorios anteriores y sus contratos versionados.
**Riesgo:** medio; atención a dependencias y falsa validación cross-platform.
**Salida:** workflow mantenible y build de juego sin herramientas de desarrollo.

### Paquetes de trabajo

- [ ] **P10.1 — Editor de fuentes.** Resaltado y formato sin alterar script C++
  ni whitespace significativo, incluidos atributos `nkui`. Evaluar Tree-sitter
  para edición incompleta; su gramática no sustituye a Clang. clangd puede tratar
  los custom attributes como desconocidos si no carga nuestro frontend: suprimir
  sólo ese warning y superponer información desde `UiSchema`, sin fingir soporte
  semántico. Si se adopta Tree-sitter, registrar tag/CSV y generación de parser.
- [ ] **P10.2 — Servicio de lenguaje mínimo.** Diagnósticos, go-to import/componente,
  hover de props/estilos y completado de bindings conocidos. Versionar documentos
  y descartar resultados viejos. Combinar diagnósticos/spans de `nk-ui-reflect`
  con tooling C++ existente; no implementar semántica C++ otra vez ni exigir
  cargar un plugin binario no confiable dentro del proceso del editor.
- [ ] **P10.3 — Backend extensible.** Cerrar interfaz `ScriptBackend` con
  `CppBackend` dueño de extracción Clang, schema, saneado, compilación y migración;
  contratos de error y dobles de test. Otros backends producirán el mismo
  `UiSchema` mediante mecanismos propios. `lang` desconocido sigue rechazado;
  no instalar otro lenguaje para demostrar extensibilidad.
- [ ] **P10.4 — Artefactos de producto.** Targets/options separan game, editor,
  compilador host, data reload y native reload. Assets C++/binarios versionados,
  licencias y pins coherentes. Build empaquetado funciona sin fuentes, Clang/LLVM,
  SDK de compilación, watcher, parser, `.so`/DLL de desarrollo o red. El paquete
  de desarrollo sí verifica versión exacta de `nk-ui-reflect` antes de usar caché.
- [ ] **P10.5 — CI y matriz real.** Headless tests, corpus/fuzz, AOT/data diff,
  GPU smoke moderna/legacy, input/plataforma, sanitizers y tracking según soporte.
  Verificar reflexión y target nativos en Windows además de Linux, incluyendo
  paths/response files de clang-cl o Clang/MinGW; un cross-build no basta.
- [ ] **P10.6 — Evidencia y mantenimiento.** Revisar cada gate/tarea, registrar
  pendientes, límites y guía de cambios de versión. Dejar instrucciones de
  build/run/watch/recovery iguales en Bash y PowerShell, con ejemplos ejecutados.

### Aceptación y cierre G10

- Checkout limpio con submódulos, build local y Nix reproducen versiones de libs.
  Build host-tool cruzado no ejecuta binarios target durante compilación.
- Paquete de game arranca sin proyecto fuente ni herramientas; editor conserva
  preview/reload sólo en configuración de desarrollo.
- Fuente inválida mantiene diagnóstico útil; código generado permite navegar
  a `.nkui`; formatter no cambia semántica de fixtures.
- Matriz de gates aprobada con evidencia o pendientes explícitos: no hay cierre
  completo mientras falte una plataforma/feature obligatoria sin acuerdo de alcance.

**Rollback:** distribuir AOT validado, desactivar herramientas específicas;
no retirar versionado/validación para aceptar paquetes incompatibles.
**Commits sugeridos:** `feat(ui-tools): navigate component sources and bindings`,
`build(ui): separate shipping UI from development tooling`.
