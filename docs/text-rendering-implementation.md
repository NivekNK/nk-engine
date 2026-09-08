# Texto y fuentes: adaptación de Kohi 73–75

Estado: implementado y validado. Clay se integrará más adelante; no es una
dependencia de este tramo. Sus futuros widgets serán responsabilidad de NK.

## Alcance y seguimiento

- [x] FreeType y HarfBuzz como submódulos a tags, CSV y Nix sincronizados.
- [x] UTF-8 validado sin cambiar la semántica en bytes de str/strview.
- [x] Fuentes, métricas, shaping y medición con la misma información que el dibujo.
- [x] Atlas R8 paginado, posiciones estables y uploads agrupados.
- [x] Geometría de texto agrupada y buffers por frame; shaders Slang.
- [x] Texto dinámico, colores, tamaños, saltos de línea y clipping en la escena.
- [x] Base de entrada de texto y escala para Wayland/Win32.
- [x] Tests CPU, fallos y lifecycle; builds y smokes Vulkan moderno/legacy.

No se incorporan todavía Clay, widgets de editor, MSDF ni un sistema completo
de párrafos bidireccionales. El contrato recibe runs con dirección/idioma;
el layout de párrafos y los componentes de UI podrán usarlo posteriormente.

## Dependencias y memoria

FreeType VER-2-14-3 (`0a0221a`) y HarfBuzz 14.4.0 (`36cb489`) se
compilan desde los submódulos. FreeType recibe el allocator del FontSystem.
HarfBuzz utiliza hooks de compilación hacia un MallocAllocator propio protegido
por mutex. Su dominio dura todo el proceso: los idiomas internados y las tablas
de funciones globales de la biblioteca se destruyen mediante atexit, después
del MemorySystem del motor. Sus contadores separados se exponen mediante
`harfbuzz_memory_statistics()`; no se atribuyen esas cachés a un FontSystem
ni se conserva un puntero a un allocator destruido. Las cachés y los recursos
de NK usan los contenedores y allocators del motor.

La compilación embebida habilita explícitamente `HAVE_ATEXIT` para que HarfBuzz
libere sus cachés globales. El shaping usa las funciones OpenType nativas de
HarfBuzz sobre los bytes de la fuente; no usa `hb-ft` ni hace que una consulta
dinámica atraviese FreeType. FreeType utiliza hinting nativo, sin auto-hinter:
las pruebas de OOM detectaron un acceso nulo del auto-hinter con módulos
parcialmente inicializados. NK rechaza esa inicialización parcial y propaga
fallos de asignación incluso cuando la consulta de métricas atraviesa HarfBuzz.
Los layouts son transaccionales y sus arrays toman prestado el allocator del
FontSystem; ese allocator debe sobrevivirlos. Descargar una fuente invalida sus
handles, pero no mueve celdas de atlas de las demás fuentes.

Validación CPU final: Debug compila y las 331 pruebas del repositorio pasan,
incluyendo UTF-8, memoria, shaping, dirección explícita, atlas, handles,
rollback, entrada de texto y reutilización de memoria tras el calentamiento.
Las mismas 331 pruebas pasan en una build Debug con ASan/UBSan y detección de
fugas activa.

El overlay reutiliza layouts estáticos y actualiza estadísticas a 4 Hz. Las
métricas de contorno se cachean por glyph/tamaño: no se recargan contornos de
FreeType para cada actualización del contador. `TextDrawList` genera seis
vértices por quad y une sólo batches adyacentes con atlas/scissor iguales, aun
con colores distintos. No hay índice ni buffer por texto; es un primer formato
sencillo para streaming. La reducción a cuatro vértices indexados/instancias
queda como optimización medible, no un requisito de GPU más moderna.

Inspección visual realizada en Niri/RADV Renoir; texto orientado correctamente,
acentos, tamaños y estadísticas visibles. Dos smokes Debug de 600 frames con
validación Vulkan cubrieron tanto dynamic rendering/synchronization2 como el
fallback legacy. Revisaron respectivamente 588 y 594 frames estables: 0 eventos
de allocation, 0 fugas y ningún VUID. Se midieron 3.179/2.014 ms CPU/GPU en la
ruta moderna y 3.133/2.000 ms en legacy; son datos diagnósticos de esta máquina,
no benchmarks comparables entre versiones. La build Release también ejecutó y
cerró limpiamente tras 300 frames, y `nix flake check` pasó para x86_64-linux.

## Entrada, IME y escala

`Input::set_text_input_enabled(true)` activa de forma explícita la captura de
texto. `Input::text_input()` devuelve vistas sin ownership de texto confirmado,
preedit, selección del preedit y solicitudes de borrado. Todos los offsets son
bytes UTF-8 y cada actualización confirmada vive un frame; el preedit persiste
hasta que la plataforma lo sustituye o finaliza. Los buffers son fijos, nunca
parten un scalar UTF-8 y exponen overflow para que el futuro widget decida cómo
recuperarse.

Wayland usa xkbcommon para teclas, compose y repeat, `text-input-v3` para la
base IME, y `fractional-scale-v1` junto a `viewporter` para separar unidades
lógicas de píxeles físicos. Esta es la ruta probada bajo Niri; la sesión usada
reportó escala 1.0, mientras que la conversión fraccional se cubrió con tests
CPU. Win32 convierte WM_CHAR/WM_UNICHAR e IMM32 desde UTF-16, distingue
preedit/commit y usa DPI awareness per-monitor v2. El código Win32 no pudo
compilarse ni ejecutarse en esta máquina Linux y debe validarse en el host
Windows 11. El fallback XCB conserva los controles de teclado, pero no promete
IME ni escala fraccional.

Todavía no existe edición de un documento ni contexto de surrounding text:
el consumidor aplica `delete_before/delete_after` sobre su propio modelo. Al
integrar Clay, un adapter deberá medir mediante `FontSystem::measure`, traducir
sus comandos de texto a `TextDrawList`, habilitar input sólo para el widget con
foco y mantener selección/caret/surrounding text fuera de Clay. Nada de esta
capa obliga a que Clay sea el dueño del texto, las fuentes o recursos Vulkan.

## Referencias

- [73: bitmap y UTF-8](https://github.com/travisvroman/kohi/commit/b9b96eb81e850c2c74709da962c52de8b1de884a)
- [74: FontSystem y ui_text](https://github.com/travisvroman/kohi/commit/bd25659f7f6acece02536c45ddbe7d0921c4222f)
- [75: fuentes dinámicas](https://github.com/travisvroman/kohi/commit/9cf0b438746153a3223143ce938bd9b3e24a5b9b)
- [Fix de lectura](https://github.com/travisvroman/kohi/commit/7ac650cc8e79ba9bcaac4305acd18b025b617748)
- [Merge sin cambios adicionales](https://github.com/travisvroman/kohi/commit/11388336ae4ea142353df4170f3745a08067a925)
- [FreeType](https://freetype.org/freetype2/docs/index.html)
- [HarfBuzz](https://harfbuzz.github.io/)
- [NoGraphicsAPI](https://github.com/sebbbi/NoGraphicsAPI)
