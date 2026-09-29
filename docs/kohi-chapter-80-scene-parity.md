# Escena de referencia de Kohi #80

Referencia: [commit 56bf83b](https://github.com/travisvroman/kohi/tree/56bf83b07d30dc7eb140c999bda20ea2c7256577).
Este documento compara la escena de prueba, no pretende igualar toda la
arquitectura ni los píxeles del motor original.

| Elemento | Kohi #80 | NK antes | Resolución |
| --- | --- | --- | --- |
| OBJ Falcon/Sponza | `vt` sin invertir; imágenes 2D invertidas al cargar | `V` invertida en el importador **y** filas de imagen invertidas | Conservar `vt` y subir la revisión del importador para regenerar `.nkmesh`. |
| Luz direccional | Color cálido `(0.4, 0.4, 0.2)` | Gris `(0.6, 0.6, 0.6)` | Usar el color de referencia en esta escena. |
| Cámara inicial | `(10.5, 5, 9.5)` | `(0, 0, 30)` | Usar la posición inicial de referencia. |
| UI de prueba | Cuadrilátero pequeño y transformado | Cuadrilátero naranja de `512 × 512` sobre Sponza | Ocultarlo por defecto; `NK_UI_DEMO=1` lo muestra y `NK_SAMPLER_DEMO=1` lo activa automáticamente. |

Los OBJ versionados de Falcon y Sponza son idénticos a los de la referencia.
Las texturas del coche son versiones reducidas de sus atlas originales, no
atlas diferentes. El sombreado Slang de NK conserva sus mejoras propias:
normalización segura, luz ambiente una sola vez, materiales recortados por alfa
y configuraciones de render/debug. Por eso no se espera una imagen idéntica al
GLSL de Kohi.

La doble inversión llevaba partes de Sponza a regiones negras de sus atlas y
desalineaba el color del coche. La prueba `StaticMeshCache.PreservesObjTextureCoordinatesAcrossCacheReload`
comprueba tanto la importación OBJ como la recarga del binario. Para verificar
la escena en Wayland/niri: `nix run .#build -- Debug` y luego
`nix run .#run -- Debug`. La primera ejecución después del cambio reimporta
los OBJ y actualiza las cachés; las siguientes usan los binarios.
