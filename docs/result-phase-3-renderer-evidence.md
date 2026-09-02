# Evidencia de outcomes y errores del renderer

> Estado: aceptada
>
> Fecha: 2026-09-02
>
> Baseline: `67051b8`

## Contratos resultantes

La API pública no expone tipos Vulkan. `Renderer::create`, `draw_frame` y
`create_texture` devuelven `result` con un `renderer_error` trivial de 8 bytes:

```cpp
struct renderer_error {
    renderer_error_code code;
    i32 native_code;
};
```

El código de dominio identifica la operación y `native_code` conserva el
`VkResult` cuando existe. Errores inferiores de File se traducen a
`shader_file_failed` y conservan el `file_error` en `native_code`; el caller no
necesita incluir Vulkan ni conocer la implementación del renderer.

Los estados `swapchain_outcome::{ready,out_of_date,suboptimal}` son éxito. En
el límite de frame se traducen a `frame_outcome::{rendered,
skipped_swapchain_recreation}`. Un resize o `VK_ERROR_OUT_OF_DATE_KHR` ya no se
confunde con un fallo y `VK_SUBOPTIMAL_KHR` agenda la recreación siguiente.

`Engine` consume el resultado y registra una sola vez el error que provoca el
cierre. Los wrappers inferiores de fence, swapchain, comandos, buffers,
imágenes, pipeline, descriptores, render pass y framebuffer propagan el código
nativo sin emitir el mismo error nuevamente.

## Inicialización y ownership

La factory sólo publica el puntero después de completar renderer, recursos de
shader, buffers y textura por defecto. Ante un error ejecuta shutdown antes de
destruir el allocator interno. Los handles y punteros Vulkan tienen estado
inicial nulo y los shutdown relevantes son idempotentes, de modo que un objeto
parcial puede limpiarse de forma determinista.

La textura se construye en estado local y sólo se copia a `out_texture` tras
crear imagen, upload y sampler. OOM y cualquier error nativo dejan el output
sin modificar. Los argumentos nulos y dimensiones inválidas son contratos y
abortar es consistente en Debug y Release.

También se corrigió el contador de frame: antes se incrementaba en
`VulkanRenderer::end_frame` y nuevamente en `Renderer`; ahora avanza una sola
vez y únicamente tras un outcome `rendered`.

## Pruebas deterministas

Se añadieron tests que cubren:

- success, out-of-date y suboptimal como estados normales del swapchain;
- `VK_ERROR_DEVICE_LOST` conservado como código nativo;
- fallo de fence/begin y submit/end sin avanzar el frame;
- frame omitido sin ejecutar updates ni presentación;
- violación de contrato en dependencias nulas de la factory;
- OOM inyectado en la allocation inicial de textura, comprobando que el output
  conserva byte por byte su estado observable;
- destrucción de un `VulkanRenderer` no inicializado bajo ASan/UBSan.

Los fallos Vulkan se prueban en el límite de clasificación y propagación. No se
añadió una tabla global de function pointers ni ramas de fault injection al hot
path de producción; las llamadas migradas comprueban directamente su
`VkResult`, y los estados parciales se cubren mediante sus destructores seguros
y sanitizadores. Operaciones aún infalibles en la API pública, como updates de
uniforms, quedan fuera de esta migración.

## Validación

- Debug: 103/103 tests.
- Release: 95/95 tests.
- ASan/UBSan: 103/103 tests desde `/tmp/nk-engine-renderer-sanitized`, sin
  hallazgos.
- Smoke Wayland/Niri: tres frames con xdg-shell nativo, Vulkan y shaders
  inicializados, seguido por shutdown completo.
- Frames estables: 0 eventos de allocation en los dos frames comprobados.
- Shutdown final: 0 allocations activas y 0 fugas en App y Renderer.

El success path añade únicamente discriminantes triviales ya medidos en la
baseline de `result`; el smoke conserva cero allocations por frame. No se usa
frame pacing de llvmpipe como microbenchmark porque su variación no mide el
coste de la propagación de errores.
