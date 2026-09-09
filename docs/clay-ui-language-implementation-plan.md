# Plan: Clay, UI compartida y lenguaje de componentes

Estado: **propuesto; ninguna fase implementada por este documento**.
Fecha de revisión: 2026-09-08.

## 1. Objetivo y decisiones de arquitectura

Construir una sola plataforma de UI para juego y editor: primero utilizable
directamente desde C++, y después mediante componentes declarativos compilables.
Clay será el motor de layout; NK seguirá siendo dueño del render, texto,
interacción, estado, recursos y herramientas del editor.

Decisiones propuestas:

- **Clay primero, lenguaje después.** La integración C++ debe ser útil y quedar
  probada antes de desarrollar el compilador.
- **NK UI** como nombre de trabajo; archivos `.nkui`, estilos `.nkcss`, herramienta
  `nk-uic`. Son nombres propuestos, no comandos disponibles actualmente.
- Componentes con `<import>`, `<script>`, `<ui>` y `<style>`. El script es C++ por
  defecto; si aparece `lang`, su único valor válido inicialmente será `cpp`.
- El `<style>` siempre tiene alcance local. Los estilos externos se importan y
  reutilizan sin convertirlos accidentalmente en reglas globales.
- Una representación intermedia común, `UiProgram`, alimentará tanto la ruta
  C++ compilada como la recarga de datos. No habrá un segundo sistema visual
  independiente para el lenguaje.
- **Dos velocidades de recarga:** templates/estilos como datos; lógica C++ como
  módulos nativos recompilados. Cambiar C++ no puede tener la misma garantía de
  latencia que cambiar un color.
- CSS será un perfil documentado y creciente, no una promesa de compatibilidad
  completa con navegadores. Las funcionalidades no soportadas darán diagnóstico.
- Librería del lenguaje separada en targets y dependencias desde el inicio,
  inicialmente dentro de este repositorio. Extraerla a otro repositorio será
  posible, pero no es requisito para empezar ni se hará como parte de este plan.
- Conservar `result`, allocators y contenedores propios; C++ con métodos en
  clases/structs. Ningún allocator implícito global ni `NK_ALLOCATOR_SCOPE`.
- Vulkan propio inspirado en NoGraphicsAPI, shaders Slang, rutas moderna y
  legacy. No añadir mínimos de GPU por integrar UI.
- Wayland/niri y Win32. No macOS, GLFW, MoltenVK, navegador embebido ni dependencia
  de Vue/JavaScript/Node para construir o ejecutar el juego.

La organización por bloques toma como inspiración los
[SFC de Vue](https://vuejs.org/api/sfc-spec.html), pero no importa su compilador,
DOM, modelo JavaScript ni todas sus directivas. Esta diferencia debe quedar
explícita en la documentación del lenguaje.

## 2. Punto de partida real del repositorio

La implementación debe partir del código actual, no asumir que los capítulos
anteriores ya proporcionan un toolkit completo.

| Área | Base existente | Trabajo pendiente para UI |
| --- | --- | --- |
| Texto | [FontSystem](../engine/include/systems/font_system.h), FreeType/HarfBuzz, `TextLayout`, medición sin rasterización, atlas y clipping. | Adaptador Clay, cachés compartidas, coherencia entre wrapping y shaping; selección/caret no son un widget ya implementado. |
| Ownership de fuentes | [TextOverlay](../engine/include/text/text_overlay.h) posee su propio `FontSystem`, `TextRenderer` y listas. | Extraer servicios reutilizables; el overlay pasa a ser consumidor, no propietario del sistema común de UI. |
| Render | [RenderPacket](../engine/include/renderer/render_packet.h) separa `ui_geometries` y `text`; [TextFrame](../engine/include/renderer/text_renderer.h) transporta rangos prestados. | Una secuencia ordenada capaz de intercalar fondos, texto, imágenes y clips. No basta con dibujar todo el texto al final. |
| Vistas | [RenderViewSystem](../engine/src/renderer/render_view.h) configura vistas world/UI y un `RenderPassKind` por vista. | Publicar el mínimo necesario para un viewport offscreen del editor. No existe todavía un render graph genérico por vista. |
| Input | [TextInputState](../engine/include/core/input.h), committed/preedit, eventos nativos y DPI. | Consumo de eventos, foco, captura, edición, clipboard, contexto IME y navegación de widgets. |
| App | [AppServices y FrameBuilder](../engine/include/core/app.h), lifecycle y memoria temporal explícita. | Exponer servicios UI sin acoplar el runtime al editor ni a su escena de ejemplo. |
| Memoria | Allocators, `arr`, `dyarr`, `map`, `slice`, `str` y `result` propios. | Presupuestos por contexto, fuentes, componentes, programa activo y candidato de recarga. |
| Recarga | Recursos y generaciones existentes, pero no una infraestructura propia de hot reload de módulos C++ de UI. | Watcher, compilador incremental, publicación transaccional, migración y descarga segura. |

Referencias locales adicionales: [texto](text-rendering-implementation.md),
[render targets](render-targets.md),
[lifetimes del renderer](renderer-resource-lifetime-contracts.md) y
[criterios Vulkan/NoGraphicsAPI](vulkan-performance-and-nographicsapi.md).

## 3. Capas y responsabilidades

```text
                      .nkui + .nkcss
                            │
             nk-ui-language / nk-uic [herramientas]
             sintaxis → imports → semántica → UiProgram
                            │
             ┌──────────────┴──────────────────┐
             │                                 │
      C++ generado + script          paquete UI de desarrollo
      compilador C++ normal          validado, sin código nativo
             │                                 │
             └──────────────┬──────────────────┘
                            │
       API C++ manual ──► nk-ui-runtime ◄── game / editor
                    estado, bindings, eventos, estilos
                            │
                    adaptador de layout Clay
                            │
                  UiDrawList ordenada + texto
                            │
              renderer NK / Slang / Vulkan moderno o legacy
```

### Distribución propuesta

- `engine/include/ui/`, `engine/src/ui/`: API C++ pública, runtime, widgets y
  adaptador Clay. Los headers públicos no expondrán tipos Vulkan ni obligarán a
  los consumidores a incluir `clay.h`.
- `engine/src/renderer/`: traducción de `UiDrawList` a comandos y recursos NK.
- `tools/ui-language/`: librería `nk-ui-language`, parser, AST, resolución,
  estilos, diagnósticos y emisión. Sin dependencias de ventana, GPU o editor.
- `tools/ui-compiler/`: CLI `nk-uic`, caché, manifiestos y modo watch.
- `tests/ui/`, `tests/ui-language/`, `benchmarks/ui/`: contratos y regresiones.
- Assets de juego y editor separados; componentes visuales comunes en un
  paquete compartido. El juego no enlaza paneles ni herramientas del editor.

Crear una frontera fundacional mínima para que las herramientas reutilicen
nuestros tipos, allocators y resultados sin enlazar el engine completo. Auditar
primero sus dependencias transitivas de logging/plataforma; extraer sólo lo
necesario a un target de soporte, no refactorizar todo el motor para este fin.
Mientras esa frontera no exista, no afirmar que la librería es autónoma.

### Ownership y memoria

- `UiSystem` se inicializa después de memoria, input, fuentes y recursos que
  necesite; se destruye antes que esos servicios. Fallos parciales se revierten.
- Un contexto/arena Clay por superficie lógica: HUD, editor y preview pueden
  coexistir. Se ejecutan serialmente en el hilo propietario; compilar archivos
  en otro hilo no autoriza a llamar Clay o FontSystem desde ese hilo.
- `UiSurface` es una región lógica con tamaño, escala, clipping, input y destino;
  no implica crear una ventana nativa por cada panel o componente.
- Estado de widgets y componentes persistente en memoria explícita. Programa
  activo y candidato en arenas distintas. Datos de frame en buffers reutilizables.
- No guardar cadenas temporales del parser ni `strview` del input más allá de
  su lifetime. `str name{allocator, "TEXTO"}` continúa siendo el contrato.
- El scratch actual de `Engine` es de 64 KiB: no colocar allí indiscriminadamente
  toda la UI, AST o atlas. Capacidades medibles, configurables y errores por OOM.
- Clay devuelve estructuras/cadenas prestadas: consumir o copiar antes de
  reutilizar su contexto; nunca dejar punteros al programa viejo tras una recarga.

La API de [Clay v0.14](https://raw.githubusercontent.com/nicbarker/clay/v0.14/clay.h)
expone contextos y arenas, y sus strings no garantizan terminación nula. El
adaptador encapsulará estos contratos, incluyendo su contexto actual y la
conversión de longitudes; no se filtrarán a los componentes.

## 4. Integración visual e interacción

### Render ordenado

`UiDrawList` tendrá comandos renderer-neutral para rectángulos, bordes,
imágenes, rangos de texto, clips y primitivas especiales registradas. Cada
comando incluye los handles/rangos necesarios, no callbacks Vulkan arbitrarios.

- Preservar el orden visual producido por layout y capas. Un panel posterior
  debe poder cubrir texto anterior. Sólo unir comandos **adyacentes** compatibles.
- Stack de clips con intersección, conversión consistente a scissor físico y
  restauración. Un clip vacío no emite draws ni recibe interacción.
- Coordenadas de layout en píxeles lógicos, escala aplicada una vez al raster.
  Acordar redondeo, UV, origen, color sRGB/lineal y alpha con los shaders existentes.
- Rectángulos/bordes redondeados requieren geometría o shader Slang adecuados.
  Recortar hijos a esquinas redondeadas **no** se resuelve sólo con scissor;
  necesita una estrategia adicional antes de anunciar soporte de ese caso.
- Reutilizar el pipeline de fuentes y sus atlas; no instalar otro rasterizador
  para los ejemplos de Clay. Medición y dibujado deben usar las mismas opciones.
- Uploads y buffers transitorios por frame en vuelo, atlas actualizado por
  regiones y recursos retirados tras finalización GPU. Sin esperas globales en
  el frame estable ni por un simple cambio de estilo.

Estos criterios conservan nuestra adaptación de
[NoGraphicsAPI](https://github.com/sebbbi/NoGraphicsAPI/blob/main/docs/no-graphics-api-comparison.md):
ownership explícito, rangos pequeños y seguimiento de finalización. No se copian
sus requisitos de extensiones ni se introduce descriptor indexing obligatorio.

### Texto e input

Medir palabras independientemente puede no dar el mismo resultado que dar
forma a una línea completa: ligaduras y scripts contextuales deben probarse.
La caché incluirá contenido, fuente/generación, tamaño, escala, idioma,
dirección y opciones pertinentes; no solamente puntero y longitud.
Auditar también la caché interna de Clay: invalidarla o versionar sus claves
cuando cambien métricas/opciones que su clave nativa no represente. No basta con
invalidar nuestra caché si Clay continúa devolviendo una medida antigua.

Primera entrega: texto estático/dinámico, wrapping validado para los idiomas
de las demos, botones, foco y scroll. La edición llega después de establecer
sus unidades: bytes UTF-8 para almacenamiento; clusters/graphemes para movimiento
y selección. Un codepoint tampoco equivale siempre a un carácter visible.

- Eventos con target, estado consumido y orden definido; acciones se despachan
  una vez. Reconstruir layout no debe repetir una acción.
- Hit testing CPU de UI con z-order, clip y superficies. Usar un snapshot
  coherente con la UI presentada; tras resize/reload invalidar o reconstruir
  antes de aceptar input sobre geometría obsoleta.
- Captura del puntero, foco de teclado, cancelación al desmontar y modales.
  Resolver primero la UI superior; enviar al gameplay sólo input no consumido.
- `Tab`, activación por teclado y navegación direccional; el adaptador de mando
  queda condicionado a incorporar soporte real de gamepad al input del motor.
- Rol, etiqueta, estado y foco de cada widget separados de su apariencia;
  foco visible desde el inicio. Las integraciones de accesibilidad con el SO
  quedan como extensión explícita, no se consideran resueltas por Clay.
- La política temporal de ESC para cerrar el motor debe preservarse inicialmente.
  Consumirlo para cerrar menús será un cambio explícito de política, no accidental.
- Clipboard y contexto IME requerirán ampliar los backends nativos. No confundir
  committed/preedit existentes con edición completa ya resuelta.
- Picking GPU permanece para objetos de la escena. En un viewport, convertir
  puntero de superficie a texel del target con su escala y versión correctas.

## 5. Lenguaje propuesto: formato y ejemplo

Contrato de `.nkui` v1:

- UTF-8; exactamente un `<ui>` con una raíz visual; cero o un bloque de cada
  tipo restante. El orden de bloques no cambia su significado.
- `<script>` y `<script lang="cpp">` son equivalentes. Cualquier otro valor,
  incluyendo vacío, es error; no hacer fallback silencioso a C++.
- `<ui>` no es HTML: tags cerrados explícitamente, primitivas en minúscula y
  componentes importados en PascalCase. Definir whitespace, entidades y escapes
  en la gramática; no adoptar reparaciones automáticas de un parser HTML.
- `<style>` es opcional, pero su alcance siempre es local; no necesita atributo
  `scoped`.
- `<import>` declara componentes, hojas de estilo y, si se necesitan, headers
  C++ mediante categorías distintas. No ejecuta scripts de instalación.

Ejemplo de sintaxis objetivo; las APIs `describe`, `field` y `action` aún no
existen. El componente importado `Button` declara el evento `click` y un slot
por defecto:

```html
<import>
  component Button from "@ui/components/Button.nkui";
  style "@ui/styles/panel.nkcss";
</import>

<script lang="cpp">
namespace {
struct Counter {
    nk::i32 count = 0;

    void increment() { ++count; }

    static constexpr auto bindings() {
        return nk::ui::describe(
            nk::ui::field("count", &Counter::count),
            nk::ui::action("increment", &Counter::increment));
    }
};
using UiModel = Counter;
}
</script>

<ui>
  <box class="panel counter">
    <text>Contador: {{ count }}</text>
    <Button @click="increment">Incrementar</Button>
  </box>
</ui>

<style>
  .counter {
    display: flex;
    flex-direction: column;
    gap: 12px;
    padding: 16px;
    background-color: var(--panel-bg, #20242c);
    color: #ffffff;
  }
</style>
```

El script se conserva como C++ y se emite a nivel de translation unit después
del preámbulo generado, antes de los adaptadores. Puede incluir headers de forma
normal. Cada componente tiene su `.cpp`, excluido de unity builds; tipos privados
en namespace anónimo evitan colisiones ODR. El contrato inicial exporta el alias
`UiModel` y su tabla
`bindings()`; sin script se genera un modelo vacío.

Esto no depende de reflexión inexistente en nuestro C++20 ni de interpretar
C++ con expresiones regulares. El compilador nativo verifica los tipos de los
miembros registrados. Una API futura más breve podrá generar estos descriptores,
pero no es necesaria para que el lenguaje funcione.

### Imports y componentes

```text
component InventorySlot from "./InventorySlot.nkui";
style "./inventory.nkcss";
header "game/inventory_view_model.h";
```

- Rutas relativas al importador; aliases declarados en un manifiesto del
  proyecto. Mismo comportamiento case-sensitive en Linux y Windows.
- Resolver dentro de raíces autorizadas, normalizar separadores y detectar
  duplicados, ciclos, faltantes y alias ambiguos, incluyendo symlinks.
- Un estilo importado aporta reglas al scope del importador. Reutilizar su AST
  no significa compartir identidad de scope ni estado de componentes.
- Imports de estilos transitivos permitidos desde `.nkcss` mediante una forma
  local de `@import "./tokens.nkcss";`, con las mismas reglas de resolución.
  Sin URLs remotas ni resolución de paquetes por red.
- Props tipadas e inmutables desde el hijo; estado local; eventos tipados hacia
  el padre; slots por defecto y nombrados. El estado del juego vive fuera de la
  UI y se consulta/cambia mediante servicios y comandos explícitos.
- Tipos iniciales de bindings: bool, enteros/floats NK, enums registrados,
  texto, valores visuales y handles del host. Listas/records mediante
  descriptores explícitos; definir copia/préstamo y conversiones comprobadas.
  No serializar punteros arbitrarios ni inferir reflexión de una clase entera.
- `:prop="expression"`, `@event="handler"`, `{{ expression }}`, `v-if`, `v-else`,
  `v-for` y `:key` son el subconjunto inicial previsto. `v-model` se añade cuando
  existan controles editables y un contrato de actualización, no como magia.
- Expresiones pequeñas: literales, acceso a campos/índices, operadores puros y
  condiciones. No C++ arbitrario, `new`, asignaciones o llamadas con efectos en
  el template; lógica compleja se registra desde el script.
- Listas requieren claves estables; detectar claves duplicadas. La identidad
  incluye superficie, instancia padre, componente y clave local. Ni el índice
  de una lista ni el número de línea son identidad persistente fiable.
- Separar la identidad del runtime de los IDs internos de Clay; detectar
  colisiones al mapearlos. Añadir `:key` también fuera de listas permite
  preservar explícitamente estado ante cambios estructurales del archivo.
- Layout se reconstruye de forma inmediata sobre estado persistente, sin DOM
  de navegador. Evaluar bindings una vez por frame al inicio; invalidación fina
  sólo donde las métricas la justifiquen. No implementar de entrada un sistema
  de proxies reactivos equivalente a Vue.

### Estilos: alcance, cascada y compatibilidad

El scope pertenece a la definición de componente; el estado pertenece a cada
instancia. No generar una copia de todas las reglas por instancia.

Un padre no selecciona clases internas de un hijo. Temas, props y un contrato
explícito de herencia tipográfica/custom properties cruzan esa frontera; no
añadir `:deep` o reglas globales implícitas. El contenido de un slot conserva el
scope del componente que lo declara. Una `class` en un componente no atraviesa
automáticamente hacia sus internos; el futuro contrato de host debe explicitarlo.

Para la primera cascada: defaults NK → reglas de autor por especificidad y
orden → propiedades inline soportadas. Imports preceden al `<style>` local en
orden de fuente, **pero mayor especificidad sigue ganando**. Desempate estable;
herencia y valores iniciales definidos por propiedad. Themes aportan tokens
explícitos desde la raíz, no un `<style>` con efectos globales ocultos.

Esta encapsulación se inspira en los
[estilos scoped de Vue](https://vuejs.org/api/sfc-css-features.html); la
[cascada CSS](https://www.w3.org/TR/css-cascade-5/) es la referencia para el
subconjunto que anunciemos como compatible. NK no incorpora inicialmente todas
sus capas, orígenes, selectores o excepciones.

| Área | Primera entrega del lenguaje | Evolución / límite |
| --- | --- | --- |
| Selectores | Tipo, clase, ID local, combinaciones simples, descendiente e hijo; `:hover`, `:active`, `:focus`, `:disabled`. | Selectores avanzados, `:has`, pseudoelementos y `!important` diferidos. |
| Caja | `width`, `height`, mínimos/máximos, padding, gap y bordes dentro de rangos admitidos. | Default `border-box` documentado; `content-box`, márgenes y casos intrínsecos requieren implementación y tests específicos. |
| Unidades | `px` lógicos, porcentajes con tamaño contenedor definido; keywords admitidos por propiedad. | `em`, `rem`, unidades de viewport y `calc()` después, con dependencias y ciclos controlados. |
| Flujo | `display: flex/none`, fila/columna y alineaciones expresamente soportadas. | Clay no equivale al algoritmo CSS Flexbox completo: pesos grow/shrink, basis, wrap y algunos repartos requieren trabajo adicional. |
| Estética | Colores, background sólido, borde y radio visual, font/size/color, imágenes como elementos. | `object-fit`, sombras, gradientes, transforms y clipping redondeado progresivos. |
| Overflow | Clip rectangular y scroll mediante widgets/runtime. | Semántica de hit testing y anidamiento consistente antes de extender masks/clip-path. |
| Variables | `--token` y `var()` con fallback y detección de ciclos. | Tokens tipados, temas y cambios por superficie. |
| Texto | Fuentes cargadas, tamaño, color, alineación y wrapping soportado. | Fallback por script, bidi de párrafo, line breaking y edición Unicode completos son trabajos propios. |
| Adaptación | Estilos por componente y tamaño conocido de superficie. | Media queries acotadas, transiciones y animaciones con presupuestos. |
| Layout avanzado | No Grid, tablas, floats, multicolumna ni flujo inline completo. | Requerirán algoritmos adicionales o un backend alternativo; no simples cambios de nombres en Clay. |

No traducir `flex: 1` ciegamente a `CLAY_SIZING_GROW`: deben verificarse todos los
valores/condiciones anunciados contra la
[semántica Flexbox](https://www.w3.org/TR/css-flexbox-1/). Si una propiedad sólo
representa una política propia de NK, usar nombre `--nk-*` reservado o una API
NK explícita, sin presentarla como CSS equivalente.

La paridad visual de muchas UIs es alcanzable sin implementar toda la web.
La paridad CSS completa convertiría este trabajo en un proyecto de layout mucho
mayor. Clay seguirá como base; antes de sustituir su algoritmo o mantener un fork
grande, presentar coste, alternativas y solicitar una decisión de alcance.

## 6. Compilador y librerías candidatas

Pipeline:

1. Extraer bloques con offsets y spans del archivo original.
2. Tokenizar y parsear imports, template, expresiones y estilos a AST propios.
3. Resolver el grafo de dependencias y símbolos; normalizar estilos.
4. Bajar a IR común de nodos, estilos, bindings, eventos, claves y referencias.
5. Emitir C++/paquete de datos, manifiesto, depfile y mapa de origen.
6. Validar el programa contra la tabla de tipos/bindings y capacidades del runtime.

El extractor del bloque C++ debe reconocer comentarios, strings escapados, raw
strings y continuaciones léxicas: un `"</script>"` dentro de C++ no cierra el
bloque. El terminador real se define en la gramática. Las macros no se expanden
para decidir el cierre; el bloque resultante se entrega al compilador C++.

La IR puede contener referencias simbólicas hasta conocer la tabla del modelo.
En AOT, el C++ generado produce verificaciones de tipos al compilar; en hot reload
de datos, se resuelven contra la tabla del módulo ya cargado antes de publicar.
**No ejecutar un binario de la plataforma objetivo durante cross-compilation**
para obtener reflexión. No embebemos Clang para entender todo el script.

| Alternativa | Utilidad real | Decisión propuesta |
| --- | --- | --- |
| [lexy](https://lexy.foonathan.net/) | DSL C++ de parsing, control explícito, diagnósticos y resultados en estructuras propias. | Candidato preferido para un prototipo acotado del parser; aislar en `.cpp` de herramientas y medir. |
| [PEGTL](https://github.com/taocpp/PEGTL) | Combinadores PEG en C++, sin generador externo obligatorio. | Alternativa si el prototipo con lexy no cumple; no instalar ambas. |
| Lexer + descenso recursivo/Pratt propios | Gramática pequeña, control de memoria y sin dependencia adicional. | Fallback viable; requiere mantener recuperación de errores, fuzzing y diagnósticos. No regex para todo. |
| [Tree-sitter](https://tree-sitter.github.io/tree-sitter/) | Árbol sintáctico incremental y útil con archivos incompletos. | Candidato posterior para resaltado/navegación del editor; no necesario en el runtime ni en el primer compilador. |
| [Runtime Compiled C++](https://github.com/RuntimeCompiledCPlusPlus/RuntimeCompiledCPlusPlus) | Referencia de recompilación y reconstrucción de objetos en ejecución. | Evaluar sus contratos en el prototipo nativo; no adoptarlo automáticamente ni importar sus ejemplos gráficos. |

La elección de parser se cerrará con un ADR tras probar un mismo corpus válido
e inválido, calidad de diagnósticos, allocations, tiempo de análisis y coste de
compilar **la herramienta**. Este último no es el tiempo de recompilar cada UI:
las plantillas de lexy/PEGTL no deben entrar en los `.cpp` generados.

No se propone LLVM/JIT, un parser C++ completo ni un motor CSS externo en la
primera versión. Añadirlos no elimina las obligaciones de estado, lifecycle y
compatibilidad del módulo; introduce nuevas dependencias y build/tooling.

### Dependencias y reproducibilidad

Clay es la única dependencia nueva decidida para la integración inicial.
La [release v0.14](https://github.com/nicbarker/clay/releases/tag/v0.14) está
publicada y es el tag candidato revisado; confirmar tag/SHA al implementarlo.
Su API difiere de ejemplos de `main`: los tests deben basarse en el tag elegido,
no mezclar versiones. No asumir APIs futuras de animación o imágenes.

Para cada librería realmente incorporada:

- Submódulo a tag revisado, gitlink exacto, licencia y procedencia verificadas.
- Actualizar [.scripts/libraries.csv](../.scripts/libraries.csv) con su proyecto
  consumidor real (`engine` o herramientas), adaptando los scripts si es preciso.
- Actualizar CMake, inputs/hash del flake y lock; builds locales y Nix aislado
  deben usar la misma revisión. No descarga implícita durante la compilación.
- Reutilizar FreeType, HarfBuzz y rapidhash ya registrados; no reinstalarlos.
- Tree-sitter, parser o librerías Unicode sólo se registran cuando se aprueba su
  uso y se incorporan, no como dependencias ficticias del plan.

## 7. Compilación rápida y recarga

### Mismos componentes, dos artefactos de desarrollo

| Cambio | Trabajo esperado | Condición |
| --- | --- | --- |
| Color, padding, reglas/import de estilos | Parse/style resolve + nuevo programa de datos. | Propiedad soportada y recursos resolubles. |
| Texto literal, estructura, instancias, bindings existentes | Recompilar datos UI y reconciliar instancias por clave. | Modelo, componentes y callbacks ya registrados en el módulo activo. |
| Nueva imagen/fuente | Datos + carga del recurso + upload asíncrono cuando proceda. | No prometer recarga inmediata si depende de I/O/rasterización. |
| Campo, handler, expresión que exige símbolo nativo nuevo, script o header C++ | Compilar/linkear módulo(s) afectados y migrar estado. | Toolchain y ABI compatibles; no toda edición de `<ui>` es sólo datos. |
| Cambio del ABI/runtime/compilador de paquetes | Rebuild compatible o reinicio controlado. | No hot reload arbitrario del engine completo. |

En desarrollo, `.nkuib` será un paquete versionado y validado de UI/estilos,
no código máquina. El motor no parsea CSS/SFC por frame. La evaluación de sus
bindings y nodos usa la misma semántica que la emisión C++ AOT.

En distribución, el generador emite código/tablas C++ enlazados al juego y
compila normalmente el script. No se incluyen watcher, compilador ni carga
arbitraria de módulos. La ruta de datos recargables es una optimización del
flujo de desarrollo, no un lenguaje de scripting nuevo.

### Incrementalidad

- Hash por bloque e import; invalidar sólo dependientes afectados. Un CSS
  compartido puede invalidar varias vistas sin tocar sus objetos C++.
- Generar un `.cpp` por componente o pequeño grupo justificado por medición;
  headers estables, includes mínimos, sin incluir toda la API del engine.
- Separar artefactos de script/bindings de layout/style en desarrollo. No
  reescribir C++ sin cambios ni forzar a Ninja a recompilar por timestamps.
- Caché identificada por contenido, transitive deps, versión de lenguaje/IR,
  target, compilador, ABI, defines y opciones. No sólo por nombre/mtime.
- `#line` y mapas de origen para que errores C++ y UI apunten al `.nkui` correcto;
  paths virtuales portables y mapeo de includes en diagnósticos.
- CMake/Ninja con dependencias explícitas y depfiles. `nk-uic` es herramienta
  **host**, separada de las librerías del target en builds cruzadas.
- Watcher nativo para Linux/Windows detrás de una interfaz; coalescer eventos,
  soportar guardado por rename, recovery tras overflow y borrados/renombrados.
- Worker de compilación separado del hilo de frame. Mantener opción one-shot
  para CI; demonio/caché caliente sólo si la medición lo justifica.
- Scripts Bash/PowerShell equivalentes y wrappers Nix. Watch y outputs escriben
  en el árbol de build, no en fuentes ni dentro del store inmutable de Nix.

### Publicación transaccional de datos

```text
guardar → detectar cambios → compilar candidato → validar dependencias/esquema
                                                    │
                               error ───────────────┤
                         conservar UI activa        │ éxito
                                                    ▼
                       publicar en frontera de frame + reconciliar estado
                                                    │
                          retirar programa/recursos viejos cuando sea seguro
```

Última versión válida permanece visible ante sintaxis incorrecta, CSS no
soportado, componente ausente, error de script, OOM o recurso no disponible.
Cada trabajo lleva generación; un resultado atrasado no sobreescribe uno más
nuevo. Cambios multiarquivo se publican como snapshot consistente, no mezclando
estilos nuevos y bindings antiguos incompatibles.

Conservar scroll, foco, selección y estado por claves compatibles; al eliminar
un control liberar captura/IME y aplicar fallback de foco. Rehacer referencias
de assets y cadenas. Un error se muestra en diagnóstico de desarrollo sin
romper la UI activa; esa vista de error no dependerá del componente fallido.

### Recarga de C++: límite y protocolo

La interfaz de uso sigue siendo C++ con métodos. **Sólo la frontera binaria**
de módulos recargables usa una entrada exportada versionada, tablas de funciones
y datos/handles de layout conocido. Ese pequeño ABI evita exponer el layout de
nuestros contenedores o una vtable del engine; no transforma toda la API en C.

- DLL/so con nombre único por generación y símbolos controlados. Rechazar
  incompatibilidad de versión, plataforma, arquitectura, compilador/runtime y
  opciones relevantes antes de activar.
- Modelo construido mediante factory con allocator explícito. No exigir
  constructor por defecto para un modelo con `str`; su allocator va primero.
- Crear instancia nueva y migrar campos registrados por identidad/tipo/versión;
  nunca `memcpy` de una clase cuyo layout cambió. Relaciones con el juego se
  reconstruyen usando handles, no direcciones antiguas.
- Validar migración en candidato antes del intercambio. Campos añadidos usan
  defaults; incompatibilidades requieren migración explícita o remontaje
  claramente notificado. Si falla la preparación, continúa el módulo anterior.
- Bloquear nuevas entradas al módulo saliente, terminar callbacks activos,
  drenar/cancelar sus trabajos y retirar sus suscripciones antes de descargar.
  Destruir objetos con código del módulo viejo **antes** de descargarlo.
- Estado persistente serializable y recursos administrados por el host. Nada de
  lambdas, vtables, TLS, destructores o punteros a strings del módulo antiguo
  escapando a jobs, GPU, event bus o cachés del host.
- En la primera versión, scripts UI sin threads propios ni efectos externos
  durante construcción/migración. Los efectos del juego pasan por comandos
  host; el render no retiene callbacks del módulo.
- Código C++ es confiable y nativo: no hay sandbox ni protección garantizada
  frente a un crash/UB del script. Compilar código de terceros requiere confianza;
  las últimas versiones válidas no permiten recuperarse de cualquier fallo nativo.
- Cambios de engine/ABI o módulos que no cumplen el contrato siguen un reinicio
  controlado. No anunciar recarga universal de cualquier clase del motor.

### Otros lenguajes en el futuro

`ScriptBackend` separa identificación de lenguaje, validación, compilación,
bindings, instancia, eventos, diagnóstico y migración. Implementar sólo
`CppBackend`; probar contratos con dobles de test, sin añadir Lua/JS/etc.

La UI y estilos pueden bajar a cualquier backend que implemente esos contratos.
**El C++ escrito por el usuario no se traducirá mágicamente a otro lenguaje.**
Un componente con `lang="futuro"` tendrá script escrito en ese lenguaje; compartirá
template, estilos y servicios. La composición entre lenguajes, si llega, usará
props/eventos/handles serializables, no objetos C++ cruzando sin contrato.

## 8. Fases de implementación y criterios de cierre

Cada fase termina con tests, evidencia en este documento y commits semánticos
por avance importante. El asunto describe el cambio, sin mencionar fase/capítulo.
Los checkboxes representan trabajo pendiente, no promesas ya implementadas.

### Fase 0 — Contratos, baseline y fixtures

- [ ] Registrar tiempos/allocations del overlay actual, CPU/GPU y frame pacing.
- [ ] Diseñar `UiSystem`, `UiSurface`, servicios de texto, lifecycle, límites y
  `UiDrawList`; fijar orden visual, unidades, input y uso de memoria.
- [ ] Crear fixtures objetivo: HUD, menú, panel de editor, superposición
  fondo/texto/imagen, clips anidados, DPI fraccional y texto complejo.
- [ ] Fijar presupuestos de compilación/recarga y registrar hardware/toolchain
  reales. Definir compatibilidad UI v0, lenguaje v1 y límites no soportados.

Cierre: ADRs pequeños y tests de contratos; baseline reproducible sin alterar
la escena ni la semántica de input existente.

### Fase 1 — Clay utilizable desde C++ y render correcto

- [ ] Incorporar Clay a tag mediante submódulo, CSV y Nix/CMake coherentes.
- [ ] Adaptador privado C++ con arena, errores `result`, contextos y capacidades.
- [ ] Extraer servicios comunes de fuentes y conectar medición/dibujo existentes.
- [ ] Implementar lista ordenada para rectángulos, bordes, imágenes, texto y
  clipping; batching adyacente y shaders Slang necesarios.
- [ ] Conservar temporalmente un adaptador de UI antigua y migrar overlay demo.

Cierre: una UI C++ con texto y controles visuales se dibuja correctamente sobre
la escena; moderna/legacy, resize, DPI, OOM y teardown validados. Nada del lenguaje
es necesario para ejecutar este hito.

### Fase 2 — Runtime interactivo y widgets esenciales

- [ ] Estado por identidad, foco, captura, navegación de teclado y eventos consumidos.
- [ ] Label, button, toggle/checkbox, slider, scroll, tooltip y modal básicos.
- [ ] Input monolínea: caret, selección, edición, undo limitado, clipboard e IME
  completo para el alcance anunciado, ampliando plataforma cuando corresponda.
- [ ] Resolver segmentación Unicode necesaria para edición: evaluar una librería
  especializada si los componentes actuales no bastan. No implementar Unicode
  completo a mano ni tratar HarfBuzz como algoritmo de bidi de párrafo.
- [ ] Fixtures de wrapping/shaping y contrato de idiomas inicial. Casos todavía
  no soportados quedan diagnosticados/documentados, no rotos silenciosamente.

Cierre: escribir, navegar, seleccionar y activar controles sin mover la cámara
accidentalmente; borrado correcto en casos Unicode cubiertos, pérdida de foco y
composición cancelada al desmontar. Foco y scissor coinciden con lo visible.

### Fase 3 — Uso real en juego y editor

- [ ] HUD/menú de juego y shell de editor sobre el mismo toolkit, con servicios
  y paquetes separados. Game build no arrastra herramientas de editor.
- [ ] Añadir target offscreen y asociación de viewport/cámara/tamaño por vista
  necesaria para mostrar la escena como imagen de UI; no asumir API ya existente.
- [ ] Sincronización render-to-texture → sampling, resize transaccional y
  retirement GPU tanto moderna como legacy.
- [ ] Mapear input/picking a viewport; toolbar, jerarquía simple e inspector
  mínimo de una propiedad real mediante comandos del host.

Cierre: escena visible dentro del editor con paneles, selección y menú de juego
aislados, sin GPU stalls nuevos. Docking, multiventana y editor completo no son
requisitos para cerrar esta fase.

### Fase 4 — Especificación y librería del lenguaje

- [ ] Fijar gramática v1 de bloques/imports/template/expresiones/estilos y formato
  de diagnóstico. Publicar corpus de ejemplos válidos e inválidos.
- [ ] Prototipo acotado de lexy; decidir con evidencia frente al fallback manual.
  Incorporar sólo la dependencia elegida y registrar pin/CSV/Nix si procede.
- [ ] Implementar separación fundacional mínima, librería sin engine/renderer,
  extractor C++, parser, AST con spans y resolución del grafo de imports.
- [ ] CLI `nk-uic check` con límites de tamaño/profundidad y recuperación de errores.
- [ ] Fuzzing del extractor/parser, ciclos, rutas, encoding y `</script>` en strings.

Cierre: parsea el ejemplo y componentes importados; rechaza `lang="js"`, bloques
duplicados y estilos inválidos con archivo/línea/columna. La librería puede
probarse headless sin inicializar memoria global, ventana o Vulkan.

### Fase 5 — Componentes, bindings e implementación C++ AOT

- [ ] IR común y API de registro C++20: props, estado, acciones, eventos y slots.
- [ ] Emitir C++ determinista, estáticas/tablas y verificaciones de tipos; maps
  de origen y depfiles. CMake construye las UIs como assets compilados del proyecto.
- [ ] Implementar interpolación, bindings, condiciones, listas con claves y slots.
- [ ] Lifecycle mount/update/unmount, límites de profundidad/expansión y errores
  runtime. Definir ownership para strings, slices y payloads de eventos.
- [ ] Convertir un widget compartido y una vista de juego/editor a `.nkui`.

Cierre: componente funcional compilado a C++ con estilo base, error de handler/prop
mal tipados en build, estado independiente en varias instancias y listas que
conservan estado al reordenarse. El ejemplo con estilos externos se completa
en fase 6.
No parser ni JIT en el frame; API C++ manual sigue funcionando.

### Fase 6 — CSS local y reutilizable

- [ ] Implementar el perfil inicial de la matriz, cascada, scope, herencia,
  imports de estilos, variables y estados interactivos.
- [ ] Reglas preindexadas por scope/tipo/clase; resolver estáticos al compilar y
  cachear combinaciones dinámicas con invalidación explícita.
- [ ] Validar unidades/rangos y casos no equivalentes a Clay; no ignorar
  propiedades desconocidas ni tratarlas como equivalentes CSS.
- [ ] Aplicar mismo theme base a HUD/editor con overrides locales y comprobar
  que clases iguales en componentes distintos no filtran reglas.

Cierre: hojas externas reutilizadas y `<style>` encapsulado, especificidad
correcta, tokens sin ciclos, fixtures de layout/color comparables y errores claros
al usar Grid/wrap/propiedades aún no soportadas.

### Fase 7 — Compilación incremental y recarga de UI/estilos

- [ ] Paquetes `.nkuib` versionados con validación de offsets, tipos, límites,
  referencias y compatibilidad; sin punteros crudos ni opcodes arbitrarios.
- [ ] Ruta de datos basada en la IR común, watcher multiplataforma, grafo/caché,
  worker y scripts Bash/PowerShell/Nix de desarrollo.
- [ ] Recarga transaccional con errores visibles, last-good y descarte de jobs
  obsoletos; mantener foco/scroll/estado compatible por clave.
- [ ] Diferenciar datos y contrato nativo: pedir recompilación si un binding
  nuevo no existe. Assets se preparan antes de publicar o usan fallback acordado.
- [ ] Comparar ejecución AOT y recargable de todos los fixtures con mismo input.

Cierre: editar layout/estilos sin reiniciar ni invocar el compilador C++ cuando
el cambio es sólo de datos. Errores y guardados parciales no rompen la UI activa;
latencias registradas, no estimadas.

### Fase 8 — Recarga nativa de scripts C++

- [ ] Prototipo aislado Linux/Win32 con ABI/versiones, compilación/link
  incrementales y módulos de nombre único, sin hot reload del engine completo.
- [ ] Factories, esquema de estado, migración, prepare/commit/retire y drenaje de
  callbacks/jobs; manejar fallos sin descargar prematuramente el módulo válido.
- [ ] Detectar cambios de ABI y ofrecer reinicio controlado cuando corresponda.
- [ ] Integrar diagnósticos nativos mapeados a `.nkui`, cache y toolchain Nix/Windows.

Cierre: cambiar un handler y agregar un campo conservando estado compatible;
fallos de compilación/migración mantienen la versión anterior. Ciclos repetidos
no acumulan módulos, objetos, callbacks ni GPU resources. Prueba real en ambas
plataformas; compilar Win32 desde Linux no sustituye verificar su loader.

### Fase 9 — Ampliación visual, CSS y herramientas de editor

- [ ] Priorizar desde pantallas reales: unidades relativas, `calc`, tamaños de
  superficie, object-fit, sombras/gradientes y transiciones acotadas.
- [ ] Implementar clipping redondeado y transforms con hit testing consistente;
  evaluar coste de máscaras/stencil/intermedios antes de fijar el backend.
- [ ] Añadir controles compuestos, listas virtualizadas, árbol e inspector con
  undo/redo del host; docking sólo tras definir persistencia y foco entre paneles.
- [ ] Inspector UI propio: árbol de instancias, bounds, clip, estilos computados,
  fuentes de reglas, memoria y coste. Preview usa el runtime real.
- [ ] Cada propiedad nueva entra en matriz con tests; flex completo/Grid/u otro
  solver se consideran una ampliación explícita, no requisito encubierto.

Cierre: escoger y documentar un conjunto de mejoras con utilidad demostrada;
rendimiento medido. La compatibilidad CSS no aumenta por declarar aliases.

### Fase 10 — Tooling del lenguaje, empaquetado y endurecimiento

- [ ] Resaltado, formato y navegación `.nkui`; evaluar Tree-sitter para edición
  incompleta y un LSP pequeño que reutilice diagnósticos/semántica del compilador.
- [ ] Completar autocompletado de bindings/componentes y mapas de código C++;
  no duplicar semántica en el editor ni implementar un IDE C++ desde cero.
- [ ] Documentar `ScriptBackend` y probar extensibilidad sin instalar otro lenguaje.
- [ ] Targets de distribución sin watcher/compiler/editor; toolchain host separada,
  assets/licencias y build reproducible desde checkout limpio con submódulos.
- [ ] Cerrar matriz de regresión, presupuesto de recursos y guía de migración
  del lenguaje. Extraer repositorio independiente sólo si aporta un beneficio real.

Cierre: runtime de juego reducido, demos de editor/game y pipeline CI completos;
especificación versionada y limitaciones visibles. No declarar acabado un editor
profesional ni un motor CSS completo por cerrar este plan.

## 9. Rendimiento, regresión y rollback

Objetivos iniciales para validar en la máquina de desarrollo, no resultados
medidos ni garantías universales:

- Registrar p50/p95 de guardar→visible. Objetivo orientativo p95 < 150 ms para
  estilos y templates pequeños calientes, incluyendo debounce; excluir/medir
  por separado cargas de assets fríos. Si no se alcanza, desglosar el coste.
- Para scripts pequeños, medir compile/link/migrate por separado; perseguir
  iteraciones cercanas a 1–2 s calientes, sin prometerlas antes del prototipo.
- Medir 100, 1.000 y 5.000 nodos, texto multilenguaje y scroll. Objetivo inicial
  CPU UI p95 ≤ 1 ms para 1.000 nodos simples calientes, sujeto a baseline real.
- Cero heap allocations en frames estables dentro de capacidades precalentadas;
  startup, glyphs nuevos y compilación tienen presupuestos propios.
- CPU de layout, styles, shaping, bindings y render prep separados; GPU,
  draw calls, bytes subidos, picos de memoria y p95/p99 de frame del juego.
- Una regresión > 10% sostenida en un escenario comparable exige investigación;
  comparar múltiples ejecuciones y dispersión, no un único FPS promedio.

Pruebas en cada fase, no sólo al final:

- Unitarias: identidad, lifetimes, OOM, parser, cascada, scopes, handlers,
  reconciliación, rollback y formatos malformados.
- Golden tests de AST/IR/C++ y screenshots con tolerancias fijadas por backend;
  equivalencia C++ manual/AOT/datos sobre fixtures, además de eventos iguales.
- Linux Wayland/niri, DPI entero/fraccional, resize/minimize y Win32 real.
  Vulkan moderno/legacy con validation layers, frames en vuelo y cierre limpio.
- Texto: tildes, combinantes, ligaduras, RTL explícito, emoji y composición;
  documentar cuándo no existe todavía bidi/fallback completo.
- Recarga: errores de sintaxis/tipo, OOM, import borrado, rename, eventos watcher
  perdidos, múltiples guardados y resultado viejo. Cien ciclos repetidos con
  memoria/módulos/referencias estables; pruebas prolongadas antes de release.
- Sanitizers y tracker propio; no dar por válida una plataforma no ejecutada.
- Build limpio local/Nix y juego empaquetado sin herramientas ni compilador.

Estrategia de regresión:

1. Mantener rutas C++ manual y AOT como referencia mientras se estabiliza la de
   datos; evitar dos algoritmos semánticos que diverjan.
2. Feature flags de desarrollo para hot reload de datos y nativo por separado.
   Si el nativo falla, sigue siendo posible recargar estilos y reiniciar scripts.
3. Toda recarga inválida conserva la última versión válida. Una feature visual
   no implementada produce error, no un fallback visual incorrecto silencioso.
4. Optimización de estilos/batching/caché entra aislada, con baseline y forma de
   desactivarla. Revertir el cambio específico si rompe corrección o empeora
   rendimiento reproduciblemente, no reescribir todo el sistema.
5. Recursos GPU se retiran por finalización; no usar un `wait idle` nuevo para
   ocultar UAF/races. Cambios de ABI requieren rebuild/reinicio documentados.
6. Registrar en cada cierre commits, comandos, resultados, plataformas realmente
   probadas y pendientes. No marcar el checkbox por haber escrito una API vacía.

Ejemplos de segmentación de commits: `feat(ui): add Clay layout adapter`,
`feat(renderer): preserve UI command ordering`,
`feat(ui-compiler): compile scoped component styles`,
`feat(ui): reload component programs transactionally`. Sin números de fase en
los asuntos y sin agrupar un cambio de renderer con todo el compilador.

## 10. Límites que requieren volver a decidir

Este plan delimita la extensibilidad, no amplía silenciosamente el alcance.
Antes de implementar cualquiera de estos cambios debe revisarse la decisión:

- Abandonar Clay como layout principal o mantener un fork sustancial para CSS.
- Integrar otro lenguaje, un JIT, runtime web o dependencia gráfica alternativa.
- Hacer hot reload arbitrario del engine/ABI, o ejecutar código C++ no confiable.
- Exigir nuevas features de GPU, eliminar legacy o añadir otra plataforma.
- Publicar la librería en un repositorio externo o introducir sincronización
  de dos repositorios antes de estabilizar su API.

Primer hito implementable: **fases 0–1**, con una demo C++ de Clay sobre nuestro
renderer y fuentes actuales. Primer hito del lenguaje: **fases 4–6**. Iteración
rápida de UI: **fase 7**, sin depender de resolver antes toda la recarga de C++.
