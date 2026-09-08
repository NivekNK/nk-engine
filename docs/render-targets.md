# Render targets and configurable render passes

El capítulo 59 separa la descripción de un pass y sus attachments de los objetos
nativos que usa Vulkan. La API pública describe qué recurso se usa, cómo se
carga, cómo se conserva y qué se limpia; no expone `VkRenderPass`,
`VkFramebuffer`, layouts ni stages.

## Modelo renderer-neutral

`RenderPassConfig` contiene nombre, tipo de pass, área, valores de clear,
attachments y continuidad con el pass anterior/siguiente. Cada
`RenderAttachmentConfig` declara:

| Campo | Contrato |
| --- | --- |
| `role` | Color o depth. Actualmente sólo se permite uno de cada tipo. |
| `source` | Color del window, depth del window o una textura interna. |
| `format` | Formato neutral que debe coincidir exactamente con la textura. |
| `sample_count` | Número de muestras común a todos los attachments del pass. |
| `load` | Descartar, cargar o limpiar el contenido anterior. |
| `store` | Descartar o conservar el resultado. |

`RenderTargetConfig` une esa descripción con un tamaño y una lista ordenada de
`Texture*`. El límite actual es de dos attachments porque el renderer sólo usa
un color y un depth. MRT, resolve attachments y subpasses adicionales quedan
fuera del alcance actual; ampliar el límite sin definir esas relaciones no
crearía una API coherente.

Antes de publicar un target se comprueba:

- nombre, área y cantidad de attachments;
- rol único y formato color/depth compatible;
- sample count uniforme en todo el pass;
- dimensiones idénticas al target;
- textura válida y `writable`;
- ownership compatible: window color debe ser external, mientras window depth
  y texturas internas pertenecen al renderer.

El target toma un snapshot de puntero, generación, formato, sample count, rol,
fuente y dimensiones. `current()` invalida el uso si cualquiera de esos datos o
el ownership cambia. `init()` primero valida un candidato completo y sólo
después reemplaza el estado publicado; un rebuild inválido conserva el target
anterior.

## Passes incorporados

El flujo continúa siendo deliberadamente lineal:

```text
world: window color (clear/store) + per-image depth (clear/discard)
  ↓ barrera/dependencia de color
UI:    window color (load/store)
  ↓ transición final
present
```

Cada imagen del swapchain posee su propio wrapper de color y su propia textura
depth. Compartir un único depth entre imágenes presentables permitiría que
frames simultáneos escribieran el mismo recurso, por lo que NK se desvía aquí de
la implementación histórica del tutorial.

El frontend acepta también `RenderAttachmentSource::texture`, preparado para
targets offscreen futuros. Este capítulo no publica aún una API de render graph
ni crea un pass adicional que la use.

## Traducción Vulkan

| Camino | Implementación |
| --- | --- |
| Dynamic rendering | Conserva formatos/config neutral, resuelve views desde el target actual y pasa load/store/clear a `vkCmdBeginRendering`. |
| Compatibilidad | Genera `VkRenderPass` desde la misma config y crea un `VkFramebuffer` por target. |

Los formatos y sample counts se convierten en un único adaptador del backend.
El frontend y los sistemas de recursos no incluyen tipos Vulkan. La relación
world → UI se expresa como barrera explícita en dynamic rendering y como
dependencia/load operation en el camino clásico.

Esto sigue la separación de descripción, recurso y comandos de
[NoGraphicsAPI](https://github.com/sebbbi/NoGraphicsAPI), pero conserva la
selección por capacidades de NK. Dynamic rendering y synchronization2 se usan
cuando existen; `NK_VULKAN_LEGACY=1` mantiene el camino para hardware/drivers
anteriores. No se incorporan descriptor heaps, device addresses ni otro mínimo
de GPU nuevo.

## Reconstrucción y ownership

Los targets y framebuffers nuevos se construyen en arrays candidatos. Sólo se
mueven al renderer después de completar todos los elementos. Durante resize:

1. se espera el trabajo pendiente según el contrato actual del swapchain;
2. se destruyen framebuffers, que todavía referencian views;
3. se descartan snapshots renderer-neutral;
4. se reemplazan depth images, views de color y swapchain;
5. se actualizan área, dimensiones, generación e image count;
6. se publican targets y, en legacy, framebuffers completos;
7. se regeneran command buffers y sincronización por imagen.

En shutdown se conserva el mismo orden: framebuffers y targets antes de render
passes, views/images y dispositivo. Los wrappers de color no destruyen el
`VkImage` external del WSI; las imágenes depth sí pertenecen al swapchain de NK.

## Referencias históricas auditadas

El commit de Kohi `4ba9e70` y su PR #61 aportan el modelo configurable y la
regeneración de targets. NK adapta esas ideas a C++, `Result`, allocators y
contenedores propios, y a sus caminos dynamic/legacy; no copia el frontend C ni
sus handles globales.

El fix tardío `5f910b6` reserva un mínimo mágico de 20 nodos para la FreeList de
Kohi. No se trasladó: la FreeList de NK recibe capacidad de metadata explícita,
no asigna después de inicializarse y reporta `metadata_exhausted` sin mutar su
estado. Sus regresiones cubren split, búsqueda de un rango posterior, release
separado y resize sin metadata suficiente.

No hicieron falta dependencias, assets ni shaders nuevos; los shaders existentes
continúan compilándose desde Slang.

## Validación reproducible

```bash
nix develop --offline --command bash -c \
  '.scripts/build.sh Debug --parallel 4 && \
   ctest --test-dir out/build/Linux-Debug --output-on-failure'

NK_ENABLE_SANITIZERS=ON nix develop --offline --command bash -c \
  '.scripts/build.sh Debug --target tests --parallel 4 && \
   ctest --test-dir out/build/Linux-Debug-Sanitized --output-on-failure'

nix develop --offline --command bash -c \
  '.scripts/build.sh Release --parallel 4 && \
   ctest --test-dir out/build/Linux-Release --output-on-failure'

NK_PLATFORM_BACKEND=wayland NK_SMOKE_TEST_FRAMES=120 \
VK_LAYER_VALIDATE_SYNC=1 nix run --offline .#run -- Debug

NK_PLATFORM_BACKEND=wayland NK_VULKAN_LEGACY=1 NK_VULKAN_ANISOTROPY=0 \
NK_SMOKE_TEST_FRAMES=120 VK_LAYER_VALIDATE_SYNC=1 \
nix run --offline .#run -- Debug
```

Cierre local: 265 tests Debug, 265 con ASan/UBSan y 256 Release. Ambos smokes
Vulkan completaron 120 frames, sin VUID, sin allocations en 118 frames estables
y sin fugas. En niri se aplicaron seis cambios de ancho/alto entre 640×720,
1280×900 y 936×999, se restauró el tamaño inicial y se cerró normalmente.
niri no ofrece una acción universal de minimizar/restaurar, por lo que esa parte
se validó mediante configure/resize/restore, que es el ciclo soportado por el
backend xdg-shell. Windows comparte config y backend Vulkan, pero no fue
ejecutado en esta máquina.
