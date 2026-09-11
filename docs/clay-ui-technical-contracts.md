# NK UI: contratos técnicos propuestos

Revisión: 2026-09-11. Estado: diseño, no API implementada.
Documento complementario al [plan principal](clay-ui-language-implementation-plan.md).
Las firmas y formatos se concretan en los paquetes de trabajo que los citan;
los invariantes de este documento son sus criterios de diseño y prueba.

## C01 — Targets y dirección de dependencias

```text
nk-foundation       tipos/allocators/result mínimos, sin engine ni GPU
      ↑
nk-ui-model         IDs, valores, esquema de bindings y formato lógico de programa
      ↑                                ↑
nk-ui-runtime                    nk-ui-language → nk-uic [host]
                                          ↑
                               GCC + nk-ui-gcc-plugin [host]
      ↑
adaptador Clay + renderer NK
      ↑
game / editor [consumidores diferentes]
```

Las flechas indican «utilizado por». Son targets propuestos, no un traslado
masivo de directorios. `nk-ui-model` no incluye AST de herramientas, Clay,
Vulkan, ventanas ni el editor. El compilador no enlaza `nk-ui-runtime` para
resolver layout: emite descripciones, no posiciones dependientes de una pantalla.
`nk-ui-gcc-plugin` incluye headers internos del GCC exacto y reduce sus árboles
a `UiSchema`; nunca se enlaza al runtime, juego ni editor distribuido.
Se carga mediante `-fplugin=<ruta>` dentro de un proceso GCC separado. Declara
licencia compatible como exige la API, ejecuta `plugin_default_version_check` y
rechaza basever/datestamp/devphase/revision/configuración diferentes. En cross
build el plugin es binario host construido contra los headers del cross-GCC que
lo cargará, no contra los del GCC target inexistente en la máquina de build.

La auditoría debe atender tres dependencias actuales: PCH público de
`engine/CMakeLists.txt`, inicialización de logging/memoria y configuración raíz
que añade engine/editor/tests/Vulkan incondicionalmente. Un target aparentemente
headless no lo es si CMake exige Vulkan o necesita `nkpch.h` para sus tipos.

Entregable: configuración sólo-herramientas, headers autocontenidos y tests del
lenguaje sin inicializar `Engine`. Reutilizar nuestros contenedores y allocators,
no copiarlos a otra librería ni crear una segunda implementación fundacional.
GCC usa su propio garbage collector y árboles internamente; toda salida que
cruza a NK se serializa y valida con límites y tipos fundacionales propios.

## C02 — Ownership y API C++

| Tipo propuesto | Dueño / función | Qué no posee |
| --- | --- | --- |
| `TextServices` | Servicios de texto compartidos con lifecycle del engine: CPU `FontSystem` y preparación GPU. | No widgets ni contexto Clay. |
| `UiSystem` | Registro de superficies, instancias, foco y configuración. | No estado autoritativo del juego. |
| `UiSurface` | Contexto Clay, arena, root, snapshot de hits y métricas de una región. | No ventana nativa obligatoria ni atlas duplicado por superficie. |
| `UiBuilder` | Construcción temporal C++ con scopes balanceados. | No almacena callbacks capturando stack de `build_frame`. |
| `UiProgram` | Programa inmutable validado y referencias a recursos/contratos. | No punteros dentro de la memoria del parser ni código nativo en el paquete. |
| `UiInstance` | Modelo/estado y claves de una instancia montada. | No memoria raw de una clase de otro módulo sin destructor válido. |
| `UiFrame` | Slices de comandos/vertices/texto para `draw_frame`. | No vida más allá del frame salvo copia explícita del backend. |

La API pública usa métodos y `result<T, ui_error>`; factories reciben allocator
primero. Scopes RAII para balancear nodos/clips y restaurar el contexto Clay,
incluyendo early return. Son scopes de construcción, no allocator ambiental.

`TextOverlay` se convierte en consumidor de servicios compartidos. Acordar un
solo propietario de cada atlas/font handle y un solo acknowledge por upload
aceptado. Al preparar varias superficies, acumular todos sus glyphs antes del
snapshot final de uploads; no perder un dirty rect descubierto por la segunda.

`UiSystem::shutdown()` cancela input/subscripciones/modelos antes de texto,
recursos y renderer. Herramientas arrancan su memoria explícita, no el engine.
Todos los objetos describen quién los destruye y con qué allocator.

## C03 — Secuencia de frame e input

Orden lógico a introducir en el lifecycle actual, con nombres ilustrativos:

1. Pump nativo y snapshot de input crudo con orden/serial, sin perder release,
   composición o múltiples clicks ocurridos entre dos frames.
2. En frontera segura, aceptar candidatos de recarga listos y gestionar
   cancelaciones. No consumir todavía acciones del nuevo layout invisible.
3. Enrutar input contra el último snapshot **presentado** y aún válido. Cada
   target se valida contra la instancia/generación activa; un snapshot invalidado
   por resize/reload no permite activar una geometría vieja bajo otro significado.
4. Actualizar estado UI y encolar comandos del host. `App::update` recibe input
   filtrado para gameplay y consume comandos del host exactamente una vez.
5. `App::build_frame`: leer modelo/props del frame, evaluar bindings, construir
   layout y listas visuales. Ninguna acción se ejecuta durante measure/layout.
6. Preparar uploads/geometry de todas las superficies y entregar `UiFrame` al
   renderer dentro del contrato prestado de `RenderPacket`.
7. Tras resultado de frame, publicar snapshot de hits sólo para contenido
   presentado. No confirmar atlas/uploads de un frame descartado.
8. Retirar recursos completados y limpiar input/frame temporales.

Un hover puede actualizarse con layout actual para visualización sin reemitir
clicks ni crear bucles de relayout. No introducir un segundo layout obligatorio
por frame; documentar la latencia de hit testing y probar animación/resize.

Consumo no significa borrar estado crudo. Mantener una vista enrutada de polling
para cámara/gameplay: marcar un evento «consumido» no sirve si después el juego
consulta `Input::is_key_down` sin filtro. No fabricar releases que dejan teclas
atascadas al cambiar foco.

Prioridad: modal superior → superficie focalizada/hit más alto → otras superficies
elegibles → gameplay. Captura conserva target mientras se arrastra; desmontaje,
focus-out, suspensión o cancelación nativa la liberan. Un control deshabilitado
no se activa ni recibe foco por navegación normal.

Texto: transacciones ordenadas de edición y una revisión del documento. Una
respuesta asíncrona de clipboard/IME lleva foco, instancia y revisión para no
pegar sobre un campo distinto después de recargar o mover foco.

## C04 — Render, geometría y recursos

`UiDrawList` es una secuencia de primitivas tipadas; textos referencian rangos de
glyphs, no otro pass al final. Batch key incluye destino, pipeline, textura/sampler,
blend, clip y recursos pertinentes. Sólo se combinan vecinos compatibles.

Controles de precisión:

- Layout f32 en píxeles lógicos; físicos = origen físico + lógico × escala.
- Intersectar clips lógicos y limitar al target antes de convertir a scissor.
  Fijar floor/ceil de bordes y comportamiento de dimensiones cero/negativas.
- No convertir coordenadas de cursor a `i16` en la nueva frontera UI; ampliar
  el tramo nativo necesario, preservando adaptadores antiguos mientras se migra.
- Rechazar NaN, infinito y overflow. No truncar silenciosamente una propiedad
  float a un campo entero/rango menor del tag de Clay elegido.
- Acordar color y alpha con un fixture sobre fondo claro/oscuro. Propuesta:
  colores de autor sRGB, cálculo definido en linear y salida premultiplicada en
  shaders UI, incluyendo cobertura de glyphs; si se adopta, migrar texto e imágenes
  juntos. No mezclar pipelines straight/premultiplied sin conversión explícita.

Buffers por slot retirado, flush de memoria no coherente cuando corresponda y
uploads con barreras apropiadas. Descriptores de una textura no se reescriben
mientras un frame en vuelo los usa. Recursos que cambian de generación se
retienen hasta terminar sus referencias CPU y uso GPU, sin espera global nueva.

El viewport del editor necesita color muestreable, depth privado y ruta de
transición attachment → sampled, tamaño/generación/cámara propios y su región
de picking. Separar extents físicos del target y rect lógico de UI. Reemplazar
target tras resize de forma transaccional; descartar readbacks de la generación
anterior. Ningún target puede realimentarse mientras se escribe en ese pass.

## C05 — Identidad, estado y binding

Tres identidades diferentes: definición de componente, instancia y recurso GPU.
La definición deriva de package/ruta canónica; la instancia añade padre y key;
los handles usan generación. Un hash ayuda al lookup, pero las colisiones se
detectan comparando la identidad completa. No asumir unicidad absoluta de 32 bits.

Sin key explícita, cambios estructurales pueden remontar un nodo: emitir aviso
de desarrollo cuando se pierde estado. `v-for` exige key estable y única entre
hermanos; no se preserva estado por índice. Renombrar/mover archivo es cambio de
identidad en v1 salvo migración explícita; no prometer preservación por heurística.

Bindings iniciales:

| Clase | Semántica |
| --- | --- |
| Prop | Sólo lectura para el hijo; default explícito o required. |
| State | Miembro registrado; mutación en acciones/update, no en el template. |
| Computed | Método `const` anotado para valores derivados; contrato sin efectos. |
| Action | Método registrado, firma tipada; ejecutado fuera del layout. |
| Event | Payload tipado hacia el padre; encolado y copiado al lifetime necesario. |
| Slot | Árbol declarado en scope del padre; montado en punto del hijo. |

Tipos v1: escalares NK, enum registrado, texto, valores visuales y handles del
host. Records/listas necesitan descriptores; préstamos valen sólo durante el
snapshot. No usar `std::any`, boxing con heap por valor ni reflexión C++ implícita.
Conversiones numéricas comprobadas; bool no acepta truthiness de un string.

Sólo las declaraciones marcadas con atributos `[[nkui::...]]` forman la interfaz.
`nk-ui-gcc-plugin` registra esos atributos en GCC, valida su destino y firma, y
reduce tipos canónicos a un `UiSchema` NK versionado. No existe una tabla manual
`bindings()`, no se expanden macros para descubrir miembros y no se refleja la
clase completa por aparecer en el AST. Campos/métodos v1 deben ser públicos;
privados/protected son error hasta tener un mecanismo explícito que no dependa
de inyección frágil de `friend`.

El parser SFC ve nombres simbólicos y delega C++ a GCC. AOT comprueba el schema
antes de emitir adaptadores; una segunda pasada del mismo GCC compila el script
intacto. Contratos restantes entre componentes se validan con tablas compiladas
antes del mount, igual que datos recargados. La CI nativa ejecuta validación
headless adicional. No se obtiene información ejecutando un binario target
durante cross-build: el cross-`g++` corre en host y analiza ese target.
Enumerar/validar schemas no instancia modelos ni llama getters, factories o
servicios del juego: debe poder hacerlo antes de crear ventana/renderer.
Cambiar un schema exige compilación nativa aunque el error se descubra en `<ui>`.

Callbacks persistentes se guardan como target/acción/generación y tablas del
módulo, no lambdas temporales ni `void*` sin ownership. Cada evento se limita en
cantidad/profundidad para impedir ciclos entre componentes en un solo frame.

## C06 — Sintaxis v1 que debe fijar el compilador

El [ejemplo principal](clay-ui-language-implementation-plan.md#5-lenguaje-propuesto-formato-y-ejemplo)
se mantiene. El parser formalizará estas reglas, no inventará semántica al parsear:

- Exactamente un `<ui>`; script/import/style opcionales y únicos. Atributos de
  bloque desconocidos o repetidos son error. `lang` sólo en script y sólo `cpp`.
- Si existe `<script>`, exactamente una definición ubicada físicamente en ese
  bloque lleva `[[nkui::component]]`. `component`, `prop`, `state`, `computed`,
  `action`, `event` y `factory` son los únicos atributos v1; spelling, número de
  argumentos, sujeto, acceso y duplicados incorrectos son errores de GCC/NK.
- Los IDs opcionales de component/state son literales UTF-8 no vacíos y únicos
  dentro de su dominio. Omitirlos deriva identidad de package/ruta/nombre y hace
  explícito que un rename puede perder migración. Macros que materialicen u
  oculten atributos `nkui` se rechazan para conservar spans deterministas.
- Delimitadores de bloques reservados fuera de strings/comentarios; el extractor
  C++ entiende raw strings y continuaciones léxicas, no evalúa `#if`/macros. Texto
  reservado sin comillas dentro de un `#if 0` sigue sujeto a la gramática externa.
- Tags case-sensitive y balanceados; atributos entre comillas, entidades
  `amp`, `lt`, `gt`, `quot`, `apos` y referencias numéricas válidas. Sin DTD/XML
  entities externas. Indentación entre elementos no crea texto; dentro de `text`
  el perfil de whitespace es explícito. No colapsar arbitrariamente texto UTF-8.
- Texto/interpolación y slots de texto se normalizan a nodos `text`. `class`/`id`
  en primitivas son locales al scope. En un componente importado se diagnostican
  salvo props/host contract explícitos; no inventar fallthrough de atributos.
- `v-if`/`v-else` entre hermanos adyacentes. No `v-if` y `v-for` en el mismo
  elemento en v1: requerir un contenedor para evitar prioridades ambiguas.
- `v-for="item in items"`, `:key="item.id"`; `@click="increment"` referencia una
  acción, no evalúa C++ dentro del atributo. Slots nombrados tienen nombres
  estáticos; slot props/scoped slots complejos quedan fuera de v1.
- Expresiones: literales, campos, índices, operadores numéricos/comparación,
  `!`, `&&`, `||` y ternario con precedencia/short-circuit definidos. No assignment,
  punteros, casts C++, funciones libres arbitrarias ni creación de objetos.
- Index fuera de rango, división por cero y overflow producen diagnóstico
  tipado y abortan esa evaluación sin efectos; no UB distinto entre AOT y datos.
- Imports de componentes forman DAG en v1: recursión import/self-component se
  rechaza. Widgets árbol se implementan con listas/estructura host, no recursión
  ilimitada del lenguaje. Estilos pueden compartir dependencias sin duplicarlas.

`UiDiagnostic` contendrá código estable, severidad, file ID, span UTF-8, mensaje
y referencias relacionadas (por ejemplo, cadena de imports). Columnas de bytes
para el core, conversión a protocolo/editor en la frontera. Límite de diagnósticos
con uno final de truncamiento; nunca un mensaje distinto por dirección de memoria.

## C07 — CSS: perfil verificable, no traducción aproximada

Cada entrada de la tabla de soporte tendrá: propiedad/valor, fase, default NK,
heredable, dominio/rango, dependencia de tamaño, lowering, error y fixture.
Referencia de comportamiento: [Flexbox](https://www.w3.org/TR/css-flexbox-1/) y
[cascada](https://www.w3.org/TR/css-cascade-5/); los defaults de nuestras primitivas
se documentan como estilos NK, no como defaults HTML del navegador.

Perfil de fase 6: cajas de fila/columna, dimensiones px y porcentajes con eje
contenedor definido; padding/gap/border válidos; colores; tipografía; clip/scroll;
selectores simples y compuestos; variables y pseudoestados ya implementados.
No declarar `auto`, `flex`, shrink/grow/basis, wrap o `min-content` equivalentes
a CSS por usar un macro Clay parecido. Si una combinación exige un algoritmo
no implementado, debe fallar con el caso y su limitación, no dibujar otro layout.

Esto exige escribir el lowering de propiedades y sus condiciones **antes** de
aprobar ejemplos como compatibles. La matriz del plan principal es una meta,
no una lista de capacidades actuales del tag de Clay.

Valores de `--token` son tokens de autor; no ejecutar una propiedad por empezar
con `--nk-`. Para extensiones de layout usar propiedades `nk-*` documentadas
fuera de CSS estándar, distinguiéndolas de custom properties que sólo almacenan
valores. `var()` se resuelve por dependencia; ciclos, faltantes sin fallback y
valor inválido para la propiedad se diagnostican.

Un scope es de definición, no de instancia. La cadena de selectores no cruza
internos de otro componente. Slots conservan autoría de estilo; sólo tipografía
y tokens explícitamente heredables cruzan la frontera. `display:none` desmonta
su participación visual/input, pero no equivale a `v-if` que desmonta instancia.

Imports preceden a reglas locales en orden de fuente, conservando especificidad.
Deduplicar por archivo **no debe cambiar** precedencia: reutilizar AST, representar
cada aparición en la cascada, y resolver su orden de forma determinista. Reglas
idénticas de distintos scopes no comparten estado dinámico.

## C08 — Artefactos, build e invalidación

Manifiesto v1 de proyecto UI, formato a fijar en fase 4: versión, raíces,
aliases, entrypoints, assets y perfil de capacidades. Paths relativos; no rutas
personales, URLs ejecutables ni postinstall. Su parser no justifica incorporar
otra librería de configuración si bastan las herramientas ya elegidas.

Artefactos por componente: fragmento C++ exacto, wrapper temporal, `UiSchema`,
adaptadores C++, programa de layout/estilos, depfile agregado y mapa de origen.
Nombres/versiones estables; writes atómicos y sólo si cambió contenido. Temporales
viven en build y no se instalan. No unity build para scripts privados.

| Entrada cambiada | Invalidación |
| --- | --- |
| Style compartido | Programas de scopes importadores; no GCC, schema ni compilación C++. |
| UI que usa bindings/componentes registrados | Datos + validación contra schema cacheado; no GCC ni compilación C++. |
| UI que requiere un binding o tipo nativo nuevo | Error de contrato hasta anotar/cambiar script y hacer rebuild nativo. |
| Script/header/dependencia C++ | Reejecutar GCC/plugin, schema/adaptadores y objetos/módulos afectados; depfiles de ambas pasadas. |
| Fuente/imagen | Recurso/generación, cachés de medida/layout pertinentes y programas dependientes. |
| GCC/plugin/schema, toolchain target, ABI o paquete incompatible | Invalidación completa correspondiente, rebuild o reinicio; no reutilizar caché obsoleta. |

La clave de reflexión incluye bytes del script, dependencias C++ transitivas,
target triple, estándar, defines/includes semánticos, versión exacta de
GCC/plugin y versión de `UiSchema`. No incluye mtime como autoridad. Los depfiles
de la pasada de schema y compilación se unen sin perder dependencias exclusivas.

AOT primero emite tablas y adaptadores C++ que usan el evaluador común; no
mantener dos intérpretes con reglas distintas. Especializar nodos/expresiones en
C++ optimizado es una optimización posterior con pruebas diferenciales.

Paquete `.nkuib`: magic/versiones, endian definido, longitudes, tabla de secciones,
IDs de capacidades/schema y payloads con offsets relativos. Serializar campo a
campo; nunca volcar structs C++ con padding/punteros. Comprobar sumas/multiplicaciones,
rangos, enum/opcode válido, integridad y presupuestos antes de reservar memoria.
Un hash de integridad no autentica código ni vuelve confiable un módulo nativo.

## C09 — Transacción de recarga y ABI

Máquina de estados de datos:
`dirty → building → validating → ready → active → retired`;
cualquier fallo previo a `active` termina en `rejected` y conserva el activo.
El request posee generación, snapshot de deps y configuración. Si cambia alguna
dependencia durante el build, revalidar hashes/reintentar: debounce por sí solo
no crea una transacción multiarquivo atómica del editor externo.

Preparar antes de commit: imports, assets, schema, memoria, reconciliación y
acciones del nuevo programa. Commit sólo intercambia estado ya preparado; no
hace I/O, compila o intenta reservar por primera vez. Retirar tras desaparición
de referencias host y finalización GPU, no tras un número mágico de frames.

Módulo C++ de desarrollo: entrada exportada/versionada, host API pequeña con
funciones POD/handles y tamaño de tabla negociado. Ni excepciones, RTTI, allocators
virtuales, containers ni `std::` cruzan el ABI. La factory usa un adaptador local
del módulo a funciones de allocation del host; free vuelve al mismo host. Los
wrappers de conveniencia internos siguen siendo C++ con métodos.

Revisar manifest/fingerprint antes de cargar y confirmar tabla exportada después.
El loader nativo puede ejecutar inicializadores durante la carga: scripts
recargables no pueden tener inicialización estática con efectos, allocations
globales, TLS con destructor o registros `atexit` que escapen. Llevar ese trabajo
a factories/unmount. El checker de árboles rechaza patrones estructurales evidentes,
pero GCC no puede demostrar ausencia total de efectos, aliasing peligroso o UB;
sigue siendo un contrato de código confiable con revisión y tests.

El formato de `.nkuib`, versión de schema, versión de atributos/plugin GCC y ABI
del módulo son independientes. C++ compilado debe compartir target/compiler/
runtime/flags relevantes; comprobar también macros de tracking que hoy cambian
layouts NK. Un cambio sólo en GCC/plugin invalida metadata aunque no cambie ABI.

Reload nativo:

1. Compilar/linkear candidato sin alterar el activo; cargar generación única.
2. Verificar ABI/contratos y preparar instancias con side effects deshabilitados.
3. En frontera del hilo propietario, capturar **estado actual**, no el que había
   cuando comenzó la compilación. Preparar migración; si es costosa, usar revisión
   optimista y revalidación, nunca aplicar snapshot viejo sin comprobar.
4. Migración por ID/tipo: default para campo nuevo, política para removido,
   adaptador explícito para tipo renombrado/cambiado. Fallo mantiene todo el activo.
5. Commit de tablas/instancias/programa compatibles como una unidad; revalidar
   foco/input en cola y generaciones de acciones.
6. Drenar trabajo permitido, destruir instancias con su código original y
   desregistrar callbacks antes de descargar el módulo saliente.

Una migración no puede llamar servicios con efectos irreversibles. Una vez
ejecutado C++ nativo arbitrario con UB no hay rollback garantizado. Si no puede
demostrarse descarga segura, rechazar la recarga y mantener la versión válida;
no «resolverlo» acumulando DLL/so por tiempo indefinido.

## C10 — Errores, límites y texto

Familias de error propuestas: lifecycle/capacidad/OOM, input/edición, layout/caps,
parse/import/tipo/estilo, formato/revisión, compilación/ABI/migración/recurso.
Un error del callback de Clay se registra en el builder actual y lo invalida;
no excepción, abort del proceso ni continuar con una lista parcial como válida.

Un **fallo de candidato** conserva el programa activo. Un **fallo durante un frame**
no permite reusar slices temporales del frame anterior: omitir esa superficie o
mostrar un fallback residente válido, sin volver a ejecutar sus acciones.

Presupuestos configurables, con límites de seguridad además de reservas:
superficies, nodos por superficie, profundidad, comandos/clips, instancias,
glyphs/texto, reglas/expresiones, eventos, archivos/imports, tamaño de candidato
y generaciones simultáneas. Registrar pico y solicitudes rechazadas. Dos arenas
no garantizan por sí solas una memoria acotada si se acumulan candidatos.

Editar texto se basa en límites de grapheme según perfil/versionado de
[UAX #29](https://www.unicode.org/reports/tr29/), no bytes ni sólo clusters del
shaper. Line breaking y bidi de párrafo son contratos separados; seleccionar
su solución por pruebas antes de declararlos soportados. Fuentes de fallback,
glyph missing y fuentes corruptas tienen política explícita.

Permisos nativos de clipboard/IME y seriales Wayland se resuelven en plataforma.
No bloquear el frame esperando clipboard. Preedit no se inserta como committed;
cancelar/reemplazar composición no debe duplicar texto.
