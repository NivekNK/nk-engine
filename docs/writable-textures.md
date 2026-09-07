# Writable textures

El capítulo 58 incorpora texturas creadas en runtime, escritura de regiones y
texturas externas para imágenes cuyo almacenamiento no pertenece al motor.
La API usa deliberadamente la grafía `writable`.

## Modelo de propiedad

| Clase de textura | Registro | Propietario de imagen/memoria | Propietario de view | Auto-release |
| --- | --- | --- | --- | --- |
| Archivo | `TextureSystem` | renderer | renderer | configurable |
| Runtime writable | `TextureSystem` | renderer | renderer | configurable |
| External/swapchain | `Swapchain` | WSI/swapchain | wrapper `Image` | no aplica |

`TextureFlag::writable` indica que la textura puede ser destino de escritura o
render. `TextureFlag::external` indica que NK no puede destruir ni reemplazar su
imagen o memoria. Una imagen de swapchain posee ambos flags: es un attachment
escribible, pero no admite la ruta de upload/resize del `TextureSystem`.

El swapchain mantiene un `Texture` y un `TextureData` estable por imagen durante
cada una de sus generaciones. `Image::shutdown()` destruye la view creada por NK,
pero no llama `vkDestroyImage` para recursos externos. Las views desaparecen
antes de `vkDestroySwapchainKHR`.

## API de runtime

```cpp
auto created = textures.acquire_writable(
    "runtime_color", 512, 512, 4, true);
if (!created) {
    // Inspeccionar texture_error.
}

Texture* texture = *created;
auto uploaded = textures.write(
    *texture,
    TextureRegion{32, 16, 128, 64},
    rgba_pixels);
auto resized = textures.resize(*texture, 1024, 512);

textures.release("runtime_color");
```

Una adquisición repetida sólo comparte una textura runtime cuando dimensiones,
canales y transparencia coinciden. Los nombres de texturas de archivo, runtime y
defaults no pueden colisionar silenciosamente.

Una región es válida cuando cabe completamente en la imagen y el slice contiene
exactamente `width * height * channel_count` bytes. La carga Vulkan usa un staging
buffer del tamaño exacto de la región y un `VkBufferImageCopy` con offset 2D real;
no interpreta el offset como un byte ignorado.

## Generaciones y rollback

La generación cambia únicamente después de que la operación del backend termina:

- creación válida: generación `0`;
- escritura válida: siguiente generación;
- resize válido: siguiente generación;
- error de validación, staging, imagen, comandos o submit: sin cambio;
- resize al mismo tamaño: éxito sin cambio de generación.

El resize crea y prepara una imagen sustituta antes de publicarla. El submit de
inicialización se completa, se cambia `m_internal_data` y recién entonces se
destruye la imagen anterior. Un error conserva puntero, dimensiones y generación.
Esto permite que la cache de descriptores detecte el cambio desde el mismo
`Texture*`.

## Sincronización actual

Uploads y resize se encolan en la misma graphics queue usada para render. El orden
FIFO cubre usos anteriores del recurso; se espera la finalización del submit de
transferencia antes de liberar staging o la imagen reemplazada. No se usa
`vkDeviceWaitIdle` en estas mutaciones.

La ruta de compatibilidad aún espera el dispositivo al destruir explícitamente
una textura y al recrear el swapchain. Eliminar esas esperas requiere retiro
diferido por fence/timeline y queda fuera de este capítulo. Las mutaciones deben
ocurrir entre frames; actualmente los callbacks de aplicación se ejecutan antes
de comenzar a grabar el frame.

Las texturas writable usan un mip, tiling óptimo y usos `TRANSFER_DST`, `SAMPLED`
y `COLOR_ATTACHMENT`. Esto deja preparado el recurso para los render targets del
capítulo siguiente sin elevar el mínimo de Vulkan ni exigir synchronization2 o
dynamic rendering: la capa de comandos conserva sus rutas moderna y legacy.

## Validación reproducible

La ruta opt-in ejercita create, upload completo, resize y upload de subregión
contra el backend Vulkan real:

```bash
NK_PLATFORM_BACKEND=wayland \
NK_SMOKE_TEST_WRITABLE_TEXTURE=1 \
NK_SMOKE_TEST_FRAMES=120 \
VK_LAYER_VALIDATE_SYNC=1 \
nix run --offline .#run -- Debug
```

En niri se validó también la recreación del swapchain cambiando ancho y alto de
la ventana durante un run de 600 frames. Resultado local: cuatro recreaciones,
cero errores/VUID de Validation Layers, cero eventos de allocation en 595 frames
estables y cero fugas reportadas. Un segundo run de 120 frames con
`NK_VULKAN_LEGACY=1 NK_VULKAN_ANISOTROPY=0` obtuvo el mismo resultado y ejercitó
las barreras y render passes de compatibilidad. La suite cerró con 259 tests
Debug, 250 Release y 259 Debug con ASan/UBSan. Windows comparte las APIs y el
backend Vulkan, pero no fue ejecutado en esta máquina.
