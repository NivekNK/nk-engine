# Plan: Clay, UI compartida y lenguaje de componentes

Estado: **propuesto; ninguna fase implementada por este documento**.
Fecha de revisión: 2026-09-12. Revisión del plan: **5**.

## Cómo usar este plan

Esta guía mantiene arquitectura, decisiones y roadmap. El trabajo verificable
se detalla en tres documentos, sin renumerar las fases del plan anterior:

- [Fases 0–3: runtime, render, input y consumidores](clay-ui-phases-runtime.md).
- [Fases 4–6: compilador, componentes C++ y estilos](clay-ui-phases-language.md).
- [Fases 7–10: recarga, ampliaciones y distribución](clay-ui-phases-reload.md).
- [Contratos C01–C11](clay-ui-technical-contracts.md): ownership, secuencia de
  frame, IDs, tipos, CSS, artefactos, ABI, fallos y servicio de lenguaje.

La [sección 8](#8-fases-dependencias-y-seguimiento) contiene el índice de gates,
dependencias, trazabilidad y decisiones pendientes. Cada paquete tiene un ID
estable para registrar commits y pruebas. Todo sigue pendiente de implementar.
Las rutas/API/CLI nuevas son propuestas, no funcionalidades disponibles.

Cambios relevantes de esta revisión: se conserva GCC como frontend autoritativo
del build y se diseña `nk-ui-lsp` como servidor compuesto para VSCode y Neovim.
El servidor resuelve el lenguaje NK UI y delega la semántica del bloque C++ a un
proceso `clangd` opcional mediante documentos sombra y mapas de origen; no enlaza
las librerías internas de Clang ni convierte a Clang en compilador del proyecto.
Se especifican routing, diagnósticos, posiciones Unicode, modo degradado y pruebas.
Los anexos precisan los contratos de esta guía; cualquier modificación posterior
debe actualizar ambos, no crear dos versiones contradictorias del requisito.

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
- El contrato C++ se marca con atributos limpios `[[nkui::component]]`,
  `[[nkui::prop]]`, `[[nkui::state]]`, `[[nkui::computed]]`,
  `[[nkui::action]]` y `[[nkui::event]]`. GCC es el frontend semántico
  autoritativo mediante `nk-ui-gcc-plugin`; `nk-uic` no intenta parsear C++.
- `CppBackend` v1 soporta GCC nativo en Linux y GCC/MinGW en Windows. El resto
  del motor puede conservar compatibilidad incidental con otros compiladores,
  pero los componentes anotados no prometen otro frontend en este hito.
- `nk-ui-lsp` será el único servidor que registra `.nkui` en el editor. Resolverá
  `<import>`, `<ui>`, `<style>` y relaciones entre bloques; para `<script>` podrá
  ejecutar `clangd` como proceso hijo. `clangd` es tooling opcional, no frontend
  de build, fuente de `UiSchema` ni dependencia del runtime o del juego distribuido.
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
      script → GCC + plugin NK        paquete UI de desarrollo
      schema + C++ generado           validado, sin código nativo
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
- `tools/ui-gcc-plugin/`: plugin host `nk-ui-gcc-plugin`, registro de atributos,
  validación y extracción de árboles GCC a `UiSchema`. Se carga sólo durante
  build; ni runtime ni juego/editor distribuidos dependen del plugin.
- `tests/src/ui/`, `tests/ui-language/`, `benchmarks/ui/`: contratos y regresiones.
  Tests del lenguaje en target headless separado del target actual `tests`, que
  enlaza engine/Vulkan; tests UI siguen la estructura existente del engine.
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

Ejemplo de sintaxis objetivo; los atributos `nkui` aún no existen. El componente
importado `Button` declara el evento `click` y un slot por defecto:

```html
<import>
  component Button from "@ui/components/Button.nkui";
  style "@ui/styles/panel.nkcss";
</import>

<script lang="cpp">
namespace game::ui {
struct [[nkui::component("game.counter")]] Counter {
    [[nkui::prop]]
    nk::i32 step = 1;

    [[nkui::state("game.counter.count")]]
    nk::i32 count = 0;

    [[nkui::computed]]
    nk::i32 displayed_count() const { return count; }

    [[nkui::action]]
    void increment() { count += step; }
};
}
</script>

<ui>
  <box class="panel counter">
    <text>Contador: {{ displayed_count }}</text>
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

El script sigue siendo C++ normal y se conserva byte por byte en un artefacto
intermedio con mapa de origen. `nk-uic` ejecuta una primera pasada
`g++ -fsyntax-only` cargando `nk-ui-gcc-plugin`; el plugin registra los atributos
scoped `nkui`, inspecciona los árboles del frontend y emite un `UiSchema`
independiente del lenguaje. Tras validar template/schema, el C++ generado se
compila con el mismo GCC y el mismo plugin en modo de aceptación, sin neutralizar
atributos ni introducir un segundo compilador.

Debe existir exactamente un `struct` o `class` **definido dentro del bloque** y
marcado `[[nkui::component]]`; los tipos anotados que lleguen desde headers no
seleccionan accidentalmente el modelo. Sin `<script>` se genera un modelo vacío.
En v1 los miembros expuestos son públicos: generar acceso a privados requeriría
inyectar amistad o reescribir clases, complejidad que no se oculta bajo el
frontend. El cuerpo de métodos, includes y tipos no se traduce ni reescribe.

Semántica inicial de atributos:

| Atributo | Destino válido | Contrato v1 |
| --- | --- | --- |
| `[[nkui::component("id")]]` | Una definición de clase/struct | Marca el modelo; el ID estable es opcional pero recomendado para mover/renombrar. |
| `[[nkui::prop]]` | Campo público no estático | Entrada inmutable durante el frame; inicializador = default, sin él = required. |
| `[[nkui::state("id")]]` | Campo público no estático | Estado persistente y migrable; ID opcional, recomendado si sobrevivirá a renames. |
| `[[nkui::computed]]` | Método público `const` | Getter sin argumentos; la ausencia de efectos es un contrato validable sólo parcialmente, no una garantía automática de GCC. |
| `[[nkui::action]]` | Método público | Handler tipado ejecutado fuera de measure/layout. |
| `[[nkui::event]]` | Campo público del tipo de evento NK | Emisión tipada hacia el padre; el host controla ownership de payload. |
| `[[nkui::factory]]` | Método estático público | Construcción fallible/custom opcional; si falta se usa el contrato allocator-first soportado. |

El namespace de atributo es `nkui`, no `nk::ui`: la sintaxis C++ de atributos
scoped admite un namespace de atributo y un nombre. Como diagnóstico del plugin
se admite temporalmente un spelling GNU interno como
`__attribute__((nkui_state))`, pero no es sintaxis pública. No se usan macros para
ocultar anotaciones.

No dependemos de reflexión estándar ausente en C++20 ni de expresiones regulares.
El mismo GCC del proyecto resuelve declaraciones, aliases, templates y tipos. La
salida de sus árboles internos se reduce inmediatamente al schema versionado;
ningún `tree` ni header privado de GCC cruza al runtime o a `nk-ui-model`.

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
  adaptadores de tipo explícitos del SDK; definir copia/préstamo y conversiones
  comprobadas. Sólo se exponen declaraciones anotadas: recorrer el AST no
  convierte todos los miembros de una clase en API ni serializa punteros.
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
representa una política propia de NK, usar propiedades `nk-*` o una API NK
explícita, sin presentarla como CSS equivalente. `--token` almacena valores de
autor y no ejecuta instrucciones especiales por su nombre; C07 detalla el contrato.

La paridad visual de muchas UIs es alcanzable sin implementar toda la web.
La paridad CSS completa convertiría este trabajo en un proyecto de layout mucho
mayor. Clay seguirá como base; antes de sustituir su algoritmo o mantener un fork
grande, presentar coste, alternativas y solicitar una decisión de alcance.

## 6. Compilador y librerías candidatas

Pipeline:

1. Extraer bloques con offsets y spans del archivo original.
2. Tokenizar y parsear imports, template, expresiones y estilos a AST propios.
3. Escribir el fragmento C++ exacto y un wrapper temporal con `#line`; obtener
   del contexto de build target, includes, defines, estándar y flags semánticos.
4. Ejecutar el mismo `g++` del target con `-fsyntax-only` y
   `-fplugin=nk-ui-gcc-plugin`; emitir schema y depfile C++ con posiciones de origen.
5. Resolver el grafo de imports/símbolos y validar el template contra el schema;
   normalizar estilos y bajar a la IR común.
6. Emitir C++/paquete de datos, manifiesto, depfile agregado y mapa de origen.
7. Compilar el C++ generado con ese mismo GCC y el plugin en modo de aceptación;
   validar el programa contra capacidades del runtime antes de montarlo.

El extractor del bloque C++ debe reconocer comentarios, strings escapados, raw
strings y continuaciones léxicas: un `"</script>"` dentro de C++ no cierra el
bloque. El terminador real se define en la gramática. Las macros no se expanden
para decidir el cierre; el fragmento resultante se entrega intacto a GCC. No
se permite generar atributos `nkui` mediante macros: deben existir físicamente
en el source para que spans, diagnósticos e invalidación sean deterministas.

La IR puede contener referencias simbólicas hasta recibir el `UiSchema` de GCC.
El frontend verifica sujetos, visibilidad, firmas, tipos y argumentos de atributos;
el validador NK comprueba props/eventos entre componentes. Los contratos se
validan **antes de montar**, también en AOT y en reload de datos. La CI nativa
ejecuta el mismo pipeline headless. No se ejecuta un binario target durante
cross-compilation: el cross-`g++` se ejecuta en host, carga un plugin binario de
host construido contra sus headers exactos y analiza semántica del target.

`nk-ui-gcc-plugin` registra los atributos durante `PLUGIN_ATTRIBUTES`, recoge
declaraciones en eventos del frontend y publica el schema al finalizar la unidad.
Se ejecuta dentro de un proceso `g++` hijo de `nk-uic`: un crash no corrompe el
compilador SFC ni el motor activo. Los argumentos se pasan como argv/response
file desde CMake, nunca concatenando shell. La salida no será un dump de árboles
GCC —API interna inestable— sino `UiSchema` propio, pequeño y versionado.

La primera pasada semántica y la compilación final parsean el C++ dos veces, pero
con idéntico frontend, target y flags. Es el diseño inicial más simple y seguro;
se mide antes de intentar que el plugin inyecte thunks o código en GCC. Cachear
schema evita la primera pasada al editar sólo `<ui>`/`<style>`, pero cualquier
cambio C++ la invalida aunque finalmente conserve la misma interfaz.

| Alternativa | Utilidad real | Decisión propuesta |
| --- | --- | --- |
| [Plugins de GCC](https://gcc.gnu.org/onlinedocs/gccint/Plugin-API.html) + atributos custom | Árbol C++ autoritativo, atributos propios y exactamente el frontend usado por el motor. | **Decidido para `CppBackend`**; `nk-ui-gcc-plugin` fijado al GCC exacto y `UiSchema` como frontera. |
| [lexy](https://lexy.foonathan.net/) | DSL C++ de parsing, control explícito, diagnósticos y resultados en estructuras propias. | Candidato preferido para un prototipo acotado del parser; aislar en `.cpp` de herramientas y medir. |
| [PEGTL](https://github.com/taocpp/PEGTL) | Combinadores PEG en C++, sin generador externo obligatorio. | Alternativa si el prototipo con lexy no cumple; no instalar ambas. |
| Lexer + descenso recursivo/Pratt propios | Gramática pequeña, control de memoria y sin dependencia adicional. | Fallback viable; requiere mantener recuperación de errores, fuzzing y diagnósticos. No regex para todo. |
| [Tree-sitter](https://tree-sitter.github.io/tree-sitter/) | Árbol sintáctico incremental y útil con archivos incompletos. | Candidato posterior para resaltado/navegación del editor; no necesario en el runtime ni en el primer compilador. |
| [Runtime Compiled C++](https://github.com/RuntimeCompiledCPlusPlus/RuntimeCompiledCPlusPlus) | Referencia de recompilación y reconstrucción de objetos en ejecución. | Evaluar sus contratos en el prototipo nativo; no adoptarlo automáticamente ni importar sus ejemplos gráficos. |

La elección de parser se cerrará con un ADR tras probar un mismo corpus válido
e inválido, calidad de diagnósticos, allocations, tiempo de análisis y coste de
compilar **la herramienta**. Este último no es el tiempo de recompilar cada UI:
las plantillas de lexy/PEGTL no deben entrar en los `.cpp` generados.

No se propone Clang como frontend de build/reflexión, LLVM/JIT ni un motor CSS
externo en la primera versión. El plugin reutiliza el GCC ya requerido por el
proyecto y no replica su parser. `clangd` sólo se contempla como proceso LSP
opcional y aislado para reutilizar semántica C++ en el editor; nunca decide si un
componente compila con GCC. Añadir un JIT no eliminaría las obligaciones de
estado, lifecycle y compatibilidad.

### Dependencias y reproducibilidad

Clay es la única librería nueva decidida para la integración inicial del runtime.
La [release v0.14](https://github.com/nicbarker/clay/releases/tag/v0.14) está
publicada y es el tag candidato revisado; confirmar tag/SHA al implementarlo.
Su API difiere de ejemplos de `main`: los tests deben basarse en el tag elegido,
no mezclar versiones. No asumir APIs futuras de animación o imágenes.

GCC ya es el toolchain principal del proyecto, no una librería nueva. El plugin
requiere `gcc-plugin.h` y headers internos de la **misma build exacta** de `g++`.
Nix debe obtener compilador, headers de plugin y plugin desde el mismo paquete
bloqueado por `flake.lock`; Windows/MinGW debe fijar distribución, versión y SHA,
y comprobar que fue construida con soporte de plugins. `plugin_default_version_check`
rechaza ABI/versiones distintas antes de analizar una unidad.

No se añade GCC como submódulo ni a `libraries.csv`: es toolchain, no código
vendorizado dentro del engine. Su versión y procedencia se registran en el
manifiesto/toolchain y en evidencia de build. Las librerías nuevas siguen
exigiendo submódulo a tag y entrada CSV. Si una distribución MinGW no incluye
plugins o sus headers, esa configuración no puede cerrar G5 hasta reemplazarla
por una distribución GCC equivalente; no se reemplaza el frontend de build por
Clang silenciosamente. La disponibilidad de clangd no cambia este requisito.

Para cada librería realmente incorporada:

- Submódulo a tag revisado, gitlink exacto, licencia y procedencia verificadas.
- Actualizar [.scripts/libraries.csv](../.scripts/libraries.csv) con su proyecto
  consumidor real (`engine` o herramientas), adaptando los scripts si es preciso.
- Actualizar CMake, inputs/hash del flake y lock; builds locales y Nix aislado
  deben usar la misma revisión. No descarga implícita durante la compilación.
- Reutilizar FreeType, HarfBuzz y rapidhash ya registrados; no reinstalarlos.
- Tree-sitter, parser o librerías Unicode sólo se registran cuando se aprueba su
  uso y se incorporan, no como dependencias ficticias del plan.
- `clangd` se obtiene como herramienta opcional del entorno Nix/toolchain o de
  la instalación del usuario. No es código vendorizado, submódulo ni entrada de
  `libraries.csv`; se fija una versión soportada en el entorno de desarrollo y
  se permite configurar su ruta explícita.

## 7. Compilación rápida y recarga

### Mismos componentes, dos artefactos de desarrollo

| Cambio | Trabajo esperado | Condición |
| --- | --- | --- |
| Color, padding, reglas/import de estilos | Parse/style resolve + nuevo programa de datos. | Propiedad soportada y recursos resolubles. |
| Texto literal, estructura, instancias, bindings existentes | Recompilar datos UI y reconciliar instancias por clave. | Modelo, componentes y callbacks ya registrados en el módulo activo. |
| Nueva imagen/fuente | Datos + carga del recurso + upload asíncrono cuando proceda. | No prometer recarga inmediata si depende de I/O/rasterización. |
| Atributo, campo, handler, tipo, script o header C++ | Reejecutar GCC/plugin para schema, validar, compilar/linkear módulo(s) afectados y migrar estado. | Toolchain y ABI compatibles; el schema nuevo no se publica separado de su módulo. |
| Cambio del ABI/runtime/compilador de paquetes | Rebuild compatible o reinicio controlado. | No hot reload arbitrario del engine completo. |

En desarrollo, `.nkuib` será un paquete versionado y validado de UI/estilos,
no código máquina. El motor no parsea CSS/SFC por frame. La evaluación de sus
bindings y nodos usa la misma semántica que la emisión C++ AOT.

En distribución, el generador emite código/tablas C++ enlazados al juego y
compila el script y los adaptadores generados con el plugin GCC. El plugin se usa
en build, pero no se incluye en el producto. Tampoco se incluyen watcher ni carga
arbitraria de módulos. La ruta de datos recargables es una optimización del flujo
de desarrollo, no un lenguaje de scripting nuevo.

### Incrementalidad

- Hash por bloque e import; invalidar sólo dependientes afectados. Un CSS
  compartido puede invalidar varias vistas sin tocar sus objetos C++.
- Generar un `.cpp` por componente o pequeño grupo justificado por medición;
  headers estables, includes mínimos, sin incluir toda la API del engine.
- Separar artefactos de script/bindings de layout/style en desarrollo. No
  reescribir C++ sin cambios ni forzar a Ninja a recompilar por timestamps.
- Cachear `UiSchema` por hash de script + headers/depfile + argumentos semánticos
  + build exacta de GCC/plugin. Editar sólo `<ui>` o `<style>` no invoca GCC;
  editar una anotación siempre invalida schema y código nativo juntos.
- Caché identificada por contenido, transitive deps, versión de lenguaje/IR,
  target, compilador, ABI, defines y opciones. No sólo por nombre/mtime.
- `#line` y mapas de origen para que errores C++ y UI apunten al `.nkui` correcto;
  paths virtuales portables y mapeo de includes en diagnósticos.
- CMake/Ninja con dependencias explícitas y depfiles. `nk-uic` y el plugin se
  construyen para **host**; el cross-`g++` ejecutable en host analiza el target
  con sus flags/sysroot y carga un plugin construido contra esa misma build GCC.
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

`ScriptBackend` separa identificación de lenguaje, delimitación del bloque,
extracción semántica, compilación, schema, instancia, eventos, diagnóstico y
migración. Implementar sólo `CppBackend`, cuyo extractor es el plugin GCC; probar el
contrato con dobles de test sin añadir Lua/JS/etc.

La UI y estilos pueden bajar a cualquier backend que implemente esos contratos.
**El C++ escrito por el usuario no se traducirá mágicamente a otro lenguaje.**
Un componente con `lang="futuro"` tendrá script escrito en ese lenguaje; compartirá
template, estilos y servicios. La composición entre lenguajes, si llega, usará
props/eventos/handles serializables, no objetos C++ cruzando sin contrato.
Cada backend podrá usar las anotaciones/reflexión naturales de su lenguaje y
deberá producir el mismo `UiSchema`; no se impondrán atributos C++ ni GCC a
scripts futuros.

### Servicio de lenguaje compuesto

VSCode y Neovim registran sólo `nk-ui-lsp` para `.nkui`. El servidor mantiene el
AST tolerante del contenedor y atiende imports, UI, estilos, bindings, símbolos,
diagnósticos y navegación entre bloques. Para un cursor dentro de `<script>`, el
backend C++ puede delegar completion, hover, signature help, definición,
referencias, acciones y formato a un proceso
[`clangd`](https://clangd.llvm.org/design/) hijo por workspace/toolchain.
No se enlazan APIs internas de clangd: el aislamiento por proceso permite
cancelar, reiniciar y actualizar la herramienta sin fijar su ABI al motor.

Por cada documento abierto se crea bajo el directorio de build/cache un `.cpp`
sombra de ruta determinista. Contiene el preámbulo/wrapper requerido y una copia
del script; sólo los spans `[[nkui::...]]` se reemplazan por espacios de igual
longitud para que clangd no diagnostique atributos que pertenecen al plugin GCC.
Este filtro léxico acotado no interpreta C++ ni toca otros atributos. Un mapa de
origen bidireccional transforma posiciones, diagnósticos y edits entre el archivo
sombra y `.nkui`; los cambios sin guardar se transmiten con `didOpen/didChange`.

`nk-ui-lsp` genera para esos archivos una
[base de compilación](https://clangd.llvm.org/design/compile-commands) derivada del
target real: estándar, includes, defines, directorio y triple. Elimina flags
exclusivos del driver/plugin GCC que clangd no entiende. Consultar el GCC con `--query-driver`
será opt-in y sólo para rutas absolutas allowlisted, nunca un glob amplio, porque
clangd ejecuta el driver coincidente. GCC/plugin sigue ejecutándose por separado
para producir el `UiSchema` y sus diagnósticos son autoritativos para atributos y
build; clangd sólo ofrece feedback C++ rápido y puede diferir del resultado final.

El servidor negocia el encoding según
[LSP](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/)
y convierte explícitamente offsets UTF-8, UTF-16/UTF-32, CRLF y líneas añadidas
por el wrapper. Funcionalidades globales
fusionan resultados no solapados: diagnósticos etiquetados por origen, símbolos,
folding y semantic tokens. Rename cross-block combina de forma atómica el
`WorkspaceEdit` de clangd con usos `{{binding}}`, props y handlers resueltos por
NK; si cambió la versión o hay edits incompatibles, no aplica una mitad. El
formato de script tampoco puede editar delimitadores ni bloques vecinos.

Sin `clangd`, el mismo servidor conserva parse, UI/style/imports, bindings y la
validación autoritativa vía GCC/plugin, e informa que completion/navegación C++
está degradada. Las extensiones de VSCode y la configuración de Neovim serán
clientes delgados: arrancan el mismo binario y no duplican semántica. Un backend
de scripting futuro puede adjuntar su propio servidor hijo detrás de esta misma
frontera sin obligar a todos los lenguajes a usar clangd.

## 8. Fases, dependencias y seguimiento

El plan conserva las fases **0–10**. El detalle ejecutable está dividido por
tramo para no convertir esta guía en una lista inmanejable. Cada paquete tiene
ID estable, objetivo, archivos/targets afectados, pruebas, criterio de salida
y rollback. Los contratos transversales están en
[contratos C01–C11](clay-ui-technical-contracts.md).

### Índice y dependencias mínimas

| Fase | Entrega | Entrada mínima | Detalle / gate de cierre | Estado |
| --- | --- | --- | --- | --- |
| 0 | Contratos verificables, harness y baseline. | Estado actual auditado. | [P0.1–P0.5 / G0](clay-ui-phases-runtime.md#fase-0--contratos-ejecutables-y-baseline) | Pendiente |
| 1 | Clay C++ y render ordenado con texto compartido. | G0. | [P1.1–P1.10 / G1](clay-ui-phases-runtime.md#fase-1--clay-desde-c-y-rendering-ordenado) | Pendiente |
| 2 | Routing, widgets y edición nativa. | G1. | [P2.1–P2.8 / G2](clay-ui-phases-runtime.md#fase-2--input-widgets-y-edición) | Pendiente |
| 3 | Game/editor sobre el toolkit y viewport offscreen. | G1 + G2A; G2 para inspector editable. | [P3.1–P3.6 / G3](clay-ui-phases-runtime.md#fase-3--game-ui-y-editor-con-viewport) | Pendiente |
| 4 | Gramática, parser y compilador headless. | G0; no necesita viewport/editor. | [P4.1–P4.8 / G4](clay-ui-phases-language.md#fase-4--gramática-formal-y-compilador-headless) | Pendiente |
| 5 | Atributos GCC, schemas, componentes y C++ AOT. | G4 + G1 + G2A. | [P5.1–P5.11 / G5](clay-ui-phases-language.md#fase-5--modelo-de-componentes-y-salida-c-aot) | Pendiente |
| 6 | Perfil CSS, estilos locales y compartidos. | G5. | [P6.1–P6.10 / G6](clay-ui-phases-language.md#fase-6--css-local-imports-y-temas) | Pendiente |
| 7 | Incrementalidad y recarga de UI/estilos. | G6. | [P7.1–P7.10 / G7](clay-ui-phases-reload.md#fase-7--build-incremental-y-recarga-de-datos) | Pendiente |
| 8 | Módulos C++ y migración transaccional. | G7; loaders probados por plataforma. | [P8.1–P8.11 / G8](clay-ui-phases-reload.md#fase-8--recarga-nativa-de-c) | Pendiente |
| 9 | Extensiones visuales acotadas y herramientas editor. | G2 + G3 + G6 + G7; no depende de G8. | [P9.1–P9.7 / G9](clay-ui-phases-reload.md#fase-9--evolución-visual-y-herramientas-propias-del-editor) | Pendiente |
| 10 | Tooling del lenguaje, distribución y endurecimiento. | Gates obligatorios anteriores. | [P10.1–P10.6 / G10](clay-ui-phases-reload.md#fase-10--tooling-distribución-y-cierre) | Pendiente |

Los gates parciales G1A/G1B, G2A, G4A y G8A permiten comprobar un subsistema antes
de integrar el siguiente. No equivalen al cierre de su fase. G2A cubre botones,
foco y routing; G2 además exige edición/clipboard/IME del alcance definido.

Orden práctico: empezar 0–1; cerrar interacción básica de 2; continuar la
integración de juego/editor y el lenguaje según sus dependencias. El camino a
recarga rápida es 4 → 5 → 6 → 7; no esperar a docking, CSS completo o hot reload
nativo para validar esa experiencia.

### Qué significa terminar una fase

1. Sus entradas están disponibles y las decisiones técnicas necesarias se han
   cerrado con evidencia; no construir sobre supuestos de una fase incompleta.
2. Todos sus paquetes obligatorios tienen implementación y tests. Un prototipo,
   API vacía o screenshot aislado no cierra el trabajo.
3. Se han ejecutado las pruebas relevantes; registrar por separado Linux y
   Win32, moderna y legacy. Sin prueba Win32, indicar gate parcial de plataforma.
4. Correctitud, lifetimes y rollback pasan; rendimiento cumple el presupuesto
   acordado o existe una revisión explícita del presupuesto con evidencia.
5. Documentación/contratos reflejan lo implementado, ejemplos funcionan y quedan
   commits semánticos por avance importante, sin fase/capítulo en el asunto.
6. Actualizar checkboxes **en el documento del tramo**, estado de esta tabla y
   un informe de evidencia por fase. No duplicar listas de tareas en varios sitios.

Plantilla de evidencia a crear durante implementación:
`docs/evidence/clay-ui/phase-N.md`, con SHA inicial/final, IDs, comandos, entornos,
tests, capturas/métricas, limitaciones, decisiones y rollback. No se crean informes
vacíos que parezcan resultados obtenidos.

### Trazabilidad de los requisitos

| Requisito | Paquetes responsables | Prueba de cierre |
| --- | --- | --- |
| Clay compartido entre juego y editor. | P1.1–P1.10, P3.1–P3.6. | Dos consumidores, un toolkit; game no enlaza editor. |
| Allocators/containers/result propios y C++ con métodos. | P0.2, P1.2, P4.4, P5.1/P5.2. | OOM/lifetimes, API allocator-first, headers headless. |
| Bloques SFC, script opcional, sólo cpp y atributos `nkui`. | P4.1/P4.5/P4.8, P5.2/P5.8, P10.3. | Corpus acepta defaults, valida atributos con GCC y rechaza lang no soportado. |
| Imports, props, eventos, slots y componentes reutilizables. | P4.7, P5.5–P5.7. | Counter/lista, keys, aislamiento de instancias y errores de contrato. |
| Style local y estilos externos reutilizables. | P6.4/P6.5/P6.8. | Scopes, cascada y tokens sin filtración entre componentes. |
| CSS creciente sin falsas equivalencias. | P6.1–P6.10, P9.3–P9.6. | Matriz propiedad/valor → lowering → fixture o error. |
| Compilar a C++ sin toolchain pesado en el runtime. | P5.2/P5.8–P5.10, P10.4. | Schema GCC + AOT, no-op build, juego sin parser/watch/plugin. |
| Recarga rápida y estado preservado. | P7.4–P7.10, P8.5–P8.9. | Latencias medidas, snapshot actual, rechazo de candidatos inválidos. |
| Extensibilidad futura de lenguaje. | P5.1–P5.3, P10.2/P10.3. | `UiSchema` independiente del lenguaje como frontera; sólo CppBackend/GCC real. |
| `.nkui` verificable en VSCode y Neovim sin reimplementar C++. | P4.5, P5.2, P10.1/P10.2/P10.5/P10.6; C11. | Un `nk-ui-lsp` compuesto, `clangd` hijo opcional, mapas de origen y modo degradado probado. |
| Slang y Vulkan compatible inspirado en NoGraphicsAPI. | P1.8/P1.9, P3.4, P9.5. | Moderna/legacy, sin GPU mínima nueva ni stalls globales. |
| Submódulos a tags y CSV/Nix/toolchains coherentes. | P1.1, P4.3, P5.2/P5.9, P2.5, P10.1/P10.4. | Librerías con gitlink; GCC/plugin de la misma build; pins, licencias y build limpio reproducibles. |
| Herramientas editor propias sobre el runtime real. | P3.2, P9.1/P9.7, P10.1/P10.2. | Preview/inspector consumen las mismas instancias y diagnósticos. |

### Decisiones y experimentos

| Decisión | Estado | Cómo/cuándo se cierra |
| --- | --- | --- |
| Clay, C++ primero, style local y plataformas actuales. | Requisito del proyecto; no cambiar silenciosamente. | Invariantes en todas las fases. |
| GCC para descubrir bindings mediante atributos C++. | Decidido; spike temporal en GCC 16.2.1 aceptó `[[nkui::...]]` incluso con `-Werror=attributes`. No es implementación del engine. | P5.2 fija versión, plugin, corpus y soporte Linux/MinGW reproducible. |
| Nombres .nkui/.nkcss/nk-uic y gramática concreta. | Propuesta de diseño. | P4.1 con corpus versionado. |
| Propiedad de fuentes, secuencia de frame y color. | Contratos propuestos C02–C04. | P0.1/P0.2 y fixtures G1. |
| lexy frente a parser propio. | lexy preferido, sin instalar. | Spike P4.2/P4.3: tiempo, memoria, diagnósticos y build. |
| Segmentación Unicode adicional. | Biblioteca por determinar si hace falta. | P2.5 con corpus/version/licencia. |
| Recarga nativa propia o utilidad externa. | SDK mínimo propio propuesto. | P8.3; no imponer RCC++ sin comparar contratos. |
| Técnica de clipping redondeado. | Pendiente de medición. | P9.5, dos candidatos compatibles con moderna/legacy. |
| Tree-sitter para tooling. | Opcional, no requisito de runtime. | P10.1: utilidad incremental frente a coste/dependencias. |
| LSP de `.nkui` y semántica C++. | Servidor compuesto decidido; `clangd` opcional como proceso, GCC autoritativo. | P10.2/C11: spike de mapping, routing, latencia, restart y paridad VSCode/Neovim. |
| CSS completo, nuevo scripting, fork grande o nueva plataforma. | Fuera del alcance aprobado. | Consultar antes de ampliar; sección 10. |

Resolver detalles de implementación dentro de estos contratos no exige una nueva
decisión del usuario en cada paso. Sí detener la parte afectada si una alternativa
cambia alcance, plataformas, hardware mínimo, dependencia principal o confianza
del código. Documentar el bloqueo y qué otras tareas independientes siguen viables.

## 9. Rendimiento, regresión y rollback

Objetivos iniciales para validar en la máquina de desarrollo, no resultados
medidos ni garantías universales:

- Registrar p50/p95 de guardar→visible. Objetivo orientativo p95 < 150 ms para
  estilos y templates pequeños calientes, incluyendo debounce; excluir/medir
  por separado cargas de assets fríos. Si no se alcanza, desglosar el coste.
- Para scripts pequeños, medir extract, primera pasada GCC/schema, generación,
  segunda compilación, link y migrate por separado; perseguir iteraciones cercanas
  a 1–2 s calientes sin prometerlas ni esconder el doble parseo de C++.
- Medir 100, 1.000 y 5.000 nodos, texto multilenguaje y scroll. Objetivo inicial
  CPU UI p95 ≤ 1 ms para 1.000 nodos simples calientes, sujeto a baseline real.
- Cero heap allocations en frames estables dentro de capacidades precalentadas;
  startup, glyphs nuevos y compilación tienen presupuestos propios.
- CPU de layout, styles, shaping, bindings y render prep separados; GPU,
  draw calls, bytes subidos, picos de memoria y p95/p99 de frame del juego.
- Una regresión > 10% sostenida en un escenario comparable exige investigación;
  comparar múltiples ejecuciones y dispersión, no un único FPS promedio.

### Protocolo de medición

Fijarlo en P0.4/P0.5 y mantenerlo en los informes de cada fase:

1. Registrar CPU/GPU, driver, compilador/configuración, SHA, tag de librerías,
   resolución/escala, modo Vulkan y opciones de benchmark. No comparar Debug con
   Release ni medir sólo con la GPU ocupada por otro proceso sin advertirlo.
2. Para UI estable: calentamiento y precarga declarados, después al menos 1.200
   frames por ejecución y cinco repeticiones. Mantener misma escena e input.
   Separar UI aislada, escena sin UI y escena con UI para atribuir costes.
3. Para recarga: 100 cambios de datos y al menos 50 cambios nativos pequeños con
   caché caliente; reportar los ensayos fríos aparte. Muestras insuficientes se
   etiquetan como exploratorias, no conclusiones sobre p99.
4. Timestamps: save detectado, dependencias leídas, parse, GCC/schema,
   lower/codegen, compile, link, recurso listo, commit de candidato y frame
   presentado. Medir desde
   guardado real cuando el harness lo controle; no llamar «guardar→visible» a
   una métrica que sólo empieza tras el debounce.
5. A/B con misma secuencia y múltiples repeticiones. Correctitud/OOM/UAF/orden
   visual son gates duros; budgets temporales son objetivos que deben fijarse o
   revisarse explícitamente con datos, no cambiarse al final para declarar éxito.

La recarga de estado no se prueba sólo guardando cuando todo está quieto:
incrementar/editar mientras compila, mover foco, cambiar escala y eliminar un
componente con callbacks pendientes. Contar pasadas GCC de schema y compilación
para demostrar que una edición de CSS no las activa.

### Registro de riesgos y respuesta

| Riesgo | Detección concreta | Respuesta / responsable |
| --- | --- | --- |
| Texto por encima de paneles que deberían taparlo. | R01 y secuencia CPU de comandos. | Bloquear G1 hasta corregir orden; P1.7/P1.8. |
| Geometría de hits vieja o polling sin consumo. | Click tras resize y escritura con cámara activa. | Snapshot/versionado y vista de input enrutada; P2.1/P2.2. |
| Cache Clay devuelve medidas obsoletas. | Cambio de idioma/font/scale en dos superficies. | Invalidación de ambas caches; P1.5/P1.6. |
| Herramienta arrastra Vulkan/PCH/engine. | Configuración sólo-herramientas limpia. | Frontera C01 y headers propios; P4.4. |
| Plugin no coincide exactamente con GCC o MinGW no lo soporta. | Configure, version check y corpus Linux/Windows. | Rechazar temprano; misma distribución/headers y fallback GNU sólo diagnóstico; P5.2/P5.9. |
| Respuesta de clangd apunta a otro bloque, versión o columna Unicode. | Corpus UTF-8/UTF-16, CRLF, edits antes del script y respuestas atrasadas. | Source map versionado, negociar encoding, descartar resultados viejos/solapados; C11/P10.2. |
| clangd no está disponible, diverge de GCC o se cae. | Inicio sin binario, fixture con flags GCC y proceso terminado durante una request. | Modo degradado explícito, GCC/plugin autoritativo y reinicio aislado con backoff; C11/P10.2. |
| Falso soporte de CSS o tipos C++ mágicos. | Fixtures negativos, matrices y errores por nivel. | Limitar perfil y validar schema real; P5.10/P6.1. |
| Mezcla de dependencias o job atrasado. | Ediciones multiarquivo durante build. | Revalidación de snapshot y generación; P7.6/P7.9. |
| Estado retrocede tras recompilar. | Incrementar mientras compila. | Snapshot actual/revisión y migración; P8.6. |
| Unload con código todavía referenciado. | Jobs/callbacks pendientes, test de cierre. | Rechazar swap inseguro, drenar/destruir antes de unload; P8.8. |
| Ampliación CSS/editor sin final definido. | Tarea fuera de P9.1–P9.7. | Backlog explícito y nueva decisión de alcance. |

### Matriz de validación transversal

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
- LSP: clientes VSCode y Neovim contra el mismo servidor; fake clangd para
  mapping/cancelación determinista y smoke con clangd real. Cubrir Unicode, CRLF,
  archivos incompletos, rename atómico, semantic tokens fusionados y crash hijo.
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
