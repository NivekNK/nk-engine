# Texto y fuentes: adaptación de Kohi 73–75

Estado: en implementación. Clay se integrará más adelante; no es una
dependencia de este tramo. Sus futuros widgets serán responsabilidad de NK.

## Alcance y seguimiento

- [x] FreeType y HarfBuzz como submódulos a tags, CSV y Nix sincronizados.
- [x] UTF-8 validado sin cambiar la semántica en bytes de str/strview.
- [x] Fuentes, métricas, shaping y medición con la misma información que el dibujo.
- [ ] Atlas R8 paginado, posiciones estables y uploads agrupados.
- [ ] Geometría de texto agrupada y buffers por frame; shaders Slang.
- [ ] Texto dinámico, colores, tamaños, saltos de línea y clipping sobre Sponza.
- [ ] Base de entrada de texto y escala para Wayland/Win32.
- [ ] Tests CPU, fallos y lifecycle; builds y smokes Vulkan moderno/legacy.

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
libere sus cachés globales. FreeType utiliza hinting nativo, sin auto-hinter:
las pruebas de OOM detectaron un acceso nulo del auto-hinter con módulos
parcialmente inicializados. NK rechaza esa inicialización parcial y propaga
fallos de asignación incluso cuando la consulta de métricas atraviesa HarfBuzz.
Los layouts son transaccionales y sus arrays toman prestado el allocator del
FontSystem; ese allocator debe sobrevivirlos. Descargar una fuente invalida sus
handles, pero no mueve celdas de atlas de las demás fuentes.

Validación inicial: Debug compila; 12 pruebas de UTF-8, memoria, shaping,
dirección explícita, atlas, handles y rollback pasan.

## Referencias

- [73: bitmap y UTF-8](https://github.com/travisvroman/kohi/commit/b9b96eb81e850c2c74709da962c52de8b1de884a)
- [74: FontSystem y ui_text](https://github.com/travisvroman/kohi/commit/bd25659f7f6acece02536c45ddbe7d0921c4222f)
- [75: fuentes dinámicas](https://github.com/travisvroman/kohi/commit/9cf0b438746153a3223143ce938bd9b3e24a5b9b)
- [Fix de lectura](https://github.com/travisvroman/kohi/commit/7ac650cc8e79ba9bcaac4305acd18b025b617748)
- [Merge sin cambios adicionales](https://github.com/travisvroman/kohi/commit/11388336ae4ea142353df4170f3745a08067a925)
- [FreeType](https://freetype.org/freetype2/docs/index.html)
- [HarfBuzz](https://harfbuzz.github.io/)
- [NoGraphicsAPI](https://github.com/sebbbi/NoGraphicsAPI)
