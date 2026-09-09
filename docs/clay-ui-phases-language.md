# NK UI: fases 4–6, lenguaje, componentes y estilos

Revisión: 2026-09-09. Todas las tareas están pendientes.
[Plan principal](clay-ui-language-implementation-plan.md) ·
[Contratos](clay-ui-technical-contracts.md) ·
[Fases nativas](clay-ui-phases-runtime.md) ·
[Recarga y distribución](clay-ui-phases-reload.md).

Este tramo termina con componentes C++ compilados, no requiere hot reload de
módulos. El parser puede desarrollarse tras G0; integrar componentes interactivos
requiere además G1 y G2A. No bloquear el lenguaje por docking o viewport.

## Fase 4 — Gramática formal y compilador headless

**Entrada:** G0 y contratos C01/C06. **Riesgo:** medio-alto por diagnóstico,
extracción de C++ y dependencias. **Salida:** herramienta reproducible de análisis.

Targets propuestos: `nk-foundation`, `nk-ui-model`, `nk-ui-language` y `nk-uic`.
Directorios nuevos: `tools/ui-language/`, `tools/ui-compiler/`,
`tests/ui-language/`. No ubicar tests headless dentro del target actual que enlaza
engine y Vulkan sin separar su configuración. Ajustar CMake raíz y PCH sólo en
la medida necesaria; no refactorizar todo el repositorio de una vez.

### P4.A — Especificación y prototipo del parser

- [ ] **P4.1 — Especificación v1.** EBNF de bloques, imports, tags, atributos,
  expresiones y estructura CSS. Definir comentarios, BOM, CRLF/LF, UTF-8 inválido,
  escapes y límites. Semántica de C06 con ejemplos positivos/negativos por regla.
  El script se conserva opaco salvo su delimitación léxica.
- [ ] **P4.2 — Spike de parser.** Implementar en prototipo la misma muestra con
  lexy y un lexer/descenso recursivo mínimo: SFC con raw string, imports, expresión
  y error recuperable. Medir parse frío/caliente, allocations, tamaño binario,
  build de la herramienta y facilidad de diagnóstico. No implementar dos lenguajes
  completos para decidir. Elegir una ruta y archivar evidencia del experimento.
- [ ] **P4.3 — ADR/dependencia elegida.** Cerrar elección con resultados y límites
  tolerados. Si se usa lexy u otra, tag + CSV del proyecto herramientas + Nix.
  El candidato preferido es lexy por su control de estructuras/allocations,
  verificado en su [documentación](https://lexy.foonathan.net/); no por suponer
  que cualquier DSL de parser siempre es más rápido que código manual.

**Gate parcial G4A:** gramática/corpus revisados y decisión de parser, sin afirmar
que el compilador de producción está implementado.

### P4.B — Frontera fundacional y resolución

- [ ] **P4.4 — Build sólo-herramientas.** Extraer soporte mínimo compartido sin
  globals de engine, PCH pesado ni Vulkan/Wayland. Headers autocontenidos con
  tracking Debug/Release coherente. Allocators inicializados explícitamente en CLI.
- [ ] **P4.5 — Source manager y extractor.** File IDs, contenido persistente,
  spans de bytes y conversión a línea/columna; raw strings con delimitador,
  comentarios, strings, continuaciones y terminador reservado C06. Preservar
  líneas/includes C++ para la posterior emisión, sin macroexpansión.
- [ ] **P4.6 — AST y diagnósticos.** AST en arena con índices, no árbol de punteros
  sueltos entre candidatos. Recuperación por bloque/elemento/declaración; límite
  de errores y mensajes con ubicaciones relacionadas. Capacidad/OOM son errores
  recuperables, incluso durante formateo del diagnóstico.
- [ ] **P4.7 — Manifest/imports.** Fijar formato v1 del manifiesto; roots, aliases,
  entrypoints y assets. Resolver paths por importador, case exacto portable,
  symlinks dentro de roots y DAG con reporte completo de ciclos. Un mismo archivo
  puede estar compartido sin leerse/reparsearse por cada instancia.
- [ ] **P4.8 — CLI y corpus.** `nk-uic check --syntax` propuesto valida sintaxis,
  imports y restricciones conocidas; modo de salida humana y estructurada,
  exit codes estables. Separar «sintaxis válida» de «bindings nativos validados».
  Integrar corpus/fuzz en un target que no necesita display/GPU.

### Aceptación y cierre G4

- Compilación de herramienta desde checkout limpio y desde configuración sin
  engine/Vulkan; mismo resultado con allocator explícito y sin memoria global.
- Tests de `<script>` ausente, `lang` ausente/`cpp`, valor desconocido, duplicados,
  tags mal anidados, imports con ciclo/faltante y raw strings con `</script>`.
- Diagnósticos de CRLF/LF y UTF-8 apuntan al mismo carácter de origen; paths y
  orden de mensajes deterministas entre Linux y Windows.
- Fuzzing con presupuesto registrado no encuentra crashes, loops sin límite o
  leaks. Entradas demasiado grandes fallan antes de reservar sin límite.
- El AST puede inspeccionarse/exportarse para tests; todavía no promete layout
  ni compatibilidad CSS por reconocer una declaración sintáctica.

**Rollback:** conservar especificación/corpus y cambiar parser detrás de la
misma API si el spike falla; no filtrar tipos lexy a AST ni runtime.
**Commits sugeridos:** `build: isolate headless UI tooling targets`,
`feat(ui-compiler): parse component files with source diagnostics`.

## Fase 5 — Modelo de componentes y salida C++ AOT

**Entrada:** G4 + G1 + G2A; C05/C08. **Riesgo:** alto por tipos, scopes y estado.
**Salida:** componentes del lenguaje ejecutados sobre la API C++ existente.

Añadir schema/IR a `nk-ui-model`, lowering/codegen en `tools/ui-language/` y
ejecución/instancias en `engine/src/ui/`. Ejemplos `.nkui` con assets conocidos;
CMake añade custom commands y unidades generadas fuera del árbol de fuentes.

### P5.A — Contrato nativo e IR

- [ ] **P5.1 — Valores y schema.** Registro C++20 de props/state/getters/actions/
  events/slots, IDs estables y conversión comprobada. String y payload explican
  ownership; records/listas usan descriptores y acceso acotado, no `std::any`.
- [ ] **P5.2 — Modelo C++.** Alias `UiModel` del script y helpers de bindings con
  métodos, factories allocator-first, mount/unmount y estado. Un archivo sin
  script usa modelo vacío. Rechazar firma incompatible y registrar el resultado
  de factory fallida sin dejar instancia parcialmente montada.
- [ ] **P5.3 — IR versionada.** Tablas de nodos, slots, referencias a bindings,
  expresiones, styles base y source map. IDs e índices separados; no posiciones
  de pantalla baked-in. Validador compartido con el futuro loader de datos.
- [ ] **P5.4 — Evaluación común.** Definir precedencia, short-circuit, tipos y
  política de errores de C06. Lookup simbólico se resuelve al montar a slots;
  no hashes de nombres en cada glyph/draw. Builder manual y programa usan las
  mismas operaciones visuales y el mismo routing de eventos.

### P5.B — Composición y estado

- [ ] **P5.5 — Instancias.** Mount/update/unmount, props default/required,
  registro de acciones y caché de bindings por generación. Limpiar suscripciones,
  captura, foco y recursos al desmontar; no copiar estado del juego al árbol UI.
- [ ] **P5.6 — Templates.** Interpolación, props ligadas y clases en primitivas,
  v-if/v-else, v-for y keys. Reconciliar por identidad completa; duplicated keys
  son error. Eliminar/reordenar elementos conserva sólo estados compatibles.
- [ ] **P5.7 — Composición.** Imports con alias, slots default/nombrados y eventos
  hacia padre. Scope de slot sigue al autor. No propagar atributos del padre a
  internos del hijo sin contrato. Limitar eventos reentrantes y expansión de nodos.

### P5.C — Generación y build

- [ ] **P5.8 — Emisor C++.** Script a nivel de translation unit, tipos privados
  sin ODR collisions y fuera de unity builds; C++ generado con nombres escapados,
  includes mínimos y `#line`. Programa inicial como tablas C++ + adaptadores del
  evaluador común; especializar código sólo después de medir.
- [ ] **P5.9 — Integración CMake.** Outputs/depfiles explícitos por componente,
  paths con espacios, no-op sin reescritura, build paralelo sin carreras y
  toolchain host para `nk-uic`. No generar código en configure cada vez ni
  incluir headers del parser en el C++ de un juego.
  Usar los contratos de outputs/depfiles de
  [CMake 3.24](https://cmake.org/cmake/help/v3.24/command/add_custom_command.html),
  que es el mínimo actual, y no depender silenciosamente de una versión superior.
- [ ] **P5.10 — Validación de tipos por nivel.** Compilador C++ comprueba miembros
  y firmas locales; schemas accesibles constexpr permiten más static checks.
  Grafo completo entre componentes se verifica con tablas compiladas antes del
  mount, también en AOT. CI nativa ejecuta validación headless; en cross-build no
  ejecutar binarios del target en host ni afirmar validación no realizada.
- [ ] **P5.11 — Ejemplos funcionales.** Counter, Button con slot/evento y lista
  de inventario con key; una vista de juego y una del editor, aún con estilos base
  limitados. Documentar qué parte del ejemplo final necesita fase 6.

### Aceptación y cierre G5

- Instanciar Counter dos veces no comparte estado; `increment` funciona una vez
  por acción tanto con C++ manual como con archivo compilado.
- Prop faltante/tipo incorrecto, handler inexistente y payload inválido tienen
  diagnóstico. Indicar si se detecta en build C++ o validación previa al mount.
- Reordenar inventario conserva estado por key; removerlo ejecuta unmount una vez.
  Slots del hijo no capturan memoria temporal del caller.
- C++ generado produce el mismo `UiDrawList` y secuencia de eventos que fixtures
  equivalentes de builder manual. No hay parsing de fuentes durante el frame.
- Error C++ apunta al `.nkui` original; no-op build deja objetos intactos.

**Rollback:** habilitar la pantalla equivalente C++ sin retirar runtime/servicios.
No aceptar una segunda implementación semántica de widgets como fallback.
**Commits sugeridos:** `feat(ui): add typed component bindings`,
`feat(ui-compiler): generate native component programs`,
`feat(ui): reconcile keyed component instances`.

## Fase 6 — CSS local, imports y temas

**Entrada:** G5; C07 y matriz de capacidades del adaptador G1.
**Riesgo:** alto por promesas CSS falsas, cascada e invalidación.
**Salida:** perfil `nkcss/1` utilizable y probado, no un parser que ignora reglas.

Nuevos módulos en lenguaje: selector AST, valores/unidades, cascada/lowering y
diagnósticos de capacidades. Runtime: computed style/cache, estados interactivos
y adaptación Clay. Assets `.nkcss` para juego y editor, no fuentes GLSL.

### P6.A — Tabla de soporte antes del lowering

- [ ] **P6.1 — Registro de propiedades.** Por propiedad: valores exactos, rangos,
  default, herencia, cuándo se resuelve porcentaje, conversión Clay/render y
  fixture. Separar soportada/equivalente-en-perfil/extensión/no-soportada.
  Comprobar tipos enteros/rangos de Clay y decidir diagnósticos de no representable.
- [ ] **P6.2 — CSS parser semántico.** Declaraciones, selectors y valores con
  spans. Normalizar sólo shorthands soportados y diagnosticar el resto. Evitar
  expandir strings de `var()` por sustitución textual sin tipado/contexto.
- [ ] **P6.3 — Primer layout verificable.** Px, porcentajes en ejes definidos,
  filas/columnas, padding/gap/box-sizing acordados. Cada combinación que requiera
  shrink/basis/auto/wrap no resuelto se rechaza, no se aproxima a GROW/FIT.

### P6.B — Encapsulación y resolución

- [ ] **P6.4 — Scope y cascada.** Tipo/clase/ID, selector compuesto, descendiente/
  hijo; especificidad y orden, defaults y inline soportado. Importar AST una vez
  conservando posición de cada aparición en cascada. Tests de imports transitivos
  y misma clase en dos componentes; no scope extra por cada instancia.
- [ ] **P6.5 — Variables/temas.** `--token`, fallback `var()` y ciclos; herencia
  sólo de propiedades permitidas. Themes como valores de raíz de superficie;
  `.nkcss` importado no crea reglas globales. Invalidar dependientes, no toda la
  escena por un cambio de token de una sola superficie.
- [ ] **P6.6 — Pseudoestados.** hover/active/focus/disabled conectados al input
  real. Prioridad definida y cache key incluye estado/tokens/generaciones.
  Reglas que cambian layout no causan bucles de hover/medición ni nuevas acciones.
- [ ] **P6.7 — Pintura y tipografía.** Color, fuente/tamaño/alineación admitidos,
  borde/radio visual, clip/scroll del runtime. Cambiar font/scale invalida medidas
  y glyph layouts; cambiar sólo color no vuelve a hacer shaping ni uploads de atlas.

### P6.C — Ejemplos y equivalencia

- [ ] **P6.8 — Paquete compartido.** Button/Panel y hojas externas usadas desde
  juego/editor. Probar reglas más específicas importadas frente a reglas locales;
  local posterior no vence automáticamente a toda regla externa.
- [ ] **P6.9 — Corpus de compatibilidad.** Layout/estilos resultantes esperados
  por fixture; comparar con referencia CSS cuando se declare equivalencia. Captura
  visual sola no demuestra cascada correcta: guardar computed styles/bounds.
- [ ] **P6.10 — Documentar perfil.** Manual de unidades/selectores/propiedades,
  defaults NK, errores y ejemplos de adaptación. `nk-*` explícito para extensiones;
  custom properties no ejecutan mágicamente instrucciones del layout.

### Aceptación y cierre G6

- Ejemplo completo del plan funciona con imports y `<style>` scoped; dos
  instancias no duplican rulesets ni filtran estilos entre scopes.
- Reglas desconocidas/Grid/wrap no soportado/ciclos var/ejes indefinidos producen
  errores precisos. No afirmar soporte de una propiedad por parsearla.
- Cambios de color no alteran geometría, cambios de font sí invalidan texto;
  métricas demuestran cachés correctas sin allocations de frame estable.
- Fixtures de selected/focused/disabled conservan input y selección; estilos no
  cambian claves ni estado persistente del componente.

**Rollback:** activar stylesheet validado anterior; desactivar caches de estilos
si hay divergencia respecto de resolución de referencia. No ignorar errores.
**Commits sugeridos:** `feat(ui-compiler): resolve scoped style imports`,
`feat(ui): apply typed component styles and theme tokens`.

## Política de finalización del tramo

G4, G5 y G6 tienen evidencias separadas siguiendo la plantilla del plan principal.
No se marcan completos por tener headers, gramática parcial o un ejemplo estático.
Antes de fase 7, una UI empaquetada AOT debe poder arrancar sin watcher ni acceso
a archivos `.nkui` de fuentes. Esa ruta será el control de regresión de recarga.
