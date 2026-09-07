# Sponza: rendimiento y base Vulkan inspirada en NoGraphicsAPI

Fecha de evaluación: 2026-09-06. Estado: cambios implementados y probados en
Linux/Wayland/niri. Punto de partida: `460e7f5`.

## Resultado y decisión

El problema dominante era la selección del driver: el flake forzaba Lavapipe,
por lo que Sponza se rasterizaba en CPU. Corregirlo produjo una mejora mucho
mayor que cambiar la API de comandos. Después se eliminaron esperas globales
por frame, se aislaron recursos en vuelo y se añadieron mipmaps.

Para este proyecto conviene **mantener una implementación propia y compatible,
usando NoGraphicsAPI como referencia arquitectónica**, no instalar el prototipo
completo como backend obligatorio. La nueva `vk::GraphicsCommands` materializa
esa decisión; no es un alias de la librería ni una implementación completa de
sus características. El contrato para continuar está en
[renderer-resource-lifetime-contracts.md](renderer-resource-lifetime-contracts.md).

No se añadió ninguna dependencia externa. `.gitmodules`, los pins del flake y
`.scripts/libraries.csv` permanecen sin cambios por esta decisión.

## Equipo y compatibilidad comprobada

El equipo local tiene Ryzen 5 5500U, 6 núcleos/12 hilos, y GPU integrada AMD
Lucienne/Vega (`1002:164c`, identificada por Vulkan como `RADV RENOIR`). Sesión
nativa Wayland con niri, CachyOS. La consulta al driver del host da Mesa
26.2.2-arch1.1, loader Vulkan 1.4.357 y dispositivo Vulkan 1.4.354. Las mediciones
del motor usan Mesa 26.2.1 de la clausura Nix, no mezclan su libc con Mesa del host.

Capacidades del dispositivo local, consultadas con `vulkaninfo`:

| Capacidad | Equipo local | NK después del cambio |
|---|---|---|
| Dynamic rendering | Sí | Se usa si está disponible |
| Synchronization2 | Sí | Se usa si está disponible |
| Buffer device address / timeline semaphores | Sí | No exigidos ni usados todavía |
| `VK_EXT_descriptor_heap` | Anunciada por RADV del host | No exigida ni usada |
| `VK_KHR_device_address_commands` | Anunciada por RADV del host | No exigida ni usada |
| `VK_KHR_shader_untyped_pointers` | Anunciada por RADV del host | No exigida ni usada |
| `VK_EXT_mesh_shader` | **No anunciada** | No exigida |
| Presentación Wayland | Sí | Se conserva nuestra plataforma nativa |

Anunciar una extensión no certifica todas sus features; aquí la ausencia de
mesh shaders basta para descartar NoGraphicsAPI **tal como está**. No se intentó
presentar una ejecución de esa librería como prueba exitosa.

La revisión de referencia es
[`284cc360c769874c3db29bd92ca6b00d4645e69e`](https://github.com/sebbbi/NoGraphicsAPI/tree/284cc360c769874c3db29bd92ca6b00d4645e69e).
Su [README](https://github.com/sebbbi/NoGraphicsAPI/blob/284cc360c769874c3db29bd92ca6b00d4645e69e/README.md)
identifica un prototipo Vulkan 1.4, single-thread/single-queue, con presentación
Win32 y compilación headless en Linux. La matriz publicada registra RTX 20/30/40/50
con NVIDIA 616.64 para Windows. Esto es evidencia del proyecto upstream, **no una
prueba en el segundo computador**: faltan el modelo NVIDIA y su driver exactos.
Windows 11 por sí solo no determina la compatibilidad. Un driver reciente no
convierte cualquier GPU antigua en una GPU con todas las features necesarias.

La [especificación del backend](https://github.com/sebbbi/NoGraphicsAPI/blob/main/docs/vulkan-support.md)
exige descriptor heaps, device-address commands, untyped pointers y mesh shaders,
además de memoria device-local/host-visible/coherente para heaps mapeados. No tiene
fallback host-only o no coherente. ReBAR no es la única posibilidad: UMA o un BAR
limitado también pueden satisfacer ese requisito; importa el tipo y la capacidad
real de memoria. `VK_KHR_unified_image_layouts` es opcional, no obligatorio.

## Librería completa frente a adaptación propia

| Opción | Beneficio | Coste para nuestro caso | Decisión |
|---|---|---|---|
| Submódulo fijado a commit | Reutilizar implementación y evolución upstream | Excluye la GPU local; falta WSI Wayland; obliga a migrar shaders/bindings y asumir la API experimental | No como dependencia obligatoria |
| Fork de la librería con fallbacks | Mantener una superficie muy próxima a upstream | Implementar igualmente WSI, descriptores y memoria compatibles, más mantener divergencias | No aporta ahorro claro hoy |
| Backend NK inspirado en su diseño | Preserva allocators, `result`, Slang, GLM, ventanas y compatibilidad actuales | Nosotros mantenemos y validamos el backend | Implementado, limitado a lo usado |

Es una decisión de ingeniería a partir del código y las capacidades observadas,
no una afirmación de que un wrapper propio sea siempre más rápido. NoGraphicsAPI
puede ser conveniente para un producto con un hardware mínimo deliberadamente
moderno y exclusivamente Win32; no coincide con nuestros dos objetivos actuales.
Si en el futuro se incorpora como backend opcional, se fijará a un SHA revisado,
se registrará ese SHA en el CSV de librerías y se mantendrá la ruta compatible.
La falta de tags no impide fijarlo reproduciblemente; no justifica seguir `main`
sin pin.

## Qué tomamos de NoGraphicsAPI

Se revisaron su
[superficie C++](https://github.com/sebbbi/NoGraphicsAPI/blob/main/include/NoGraphicsAPI/NoGraphicsAPI.hpp),
[backend](https://github.com/sebbbi/NoGraphicsAPI/blob/main/src/NoGraphicsAPI.cpp) y
[comparación de diseño](https://github.com/sebbbi/NoGraphicsAPI/blob/main/docs/no-graphics-api-comparison.md).
La inspiración central es separar ownership/suballocación de los comandos,
expresar dependencias de memoria y enviar pocos datos por draw. La correspondencia
implementada en NK es deliberadamente parcial:

| Idea | Implementación NK actual | Diferencia deliberada |
|---|---|---|
| Rangos prestados de memoria | `vk::BufferView {buffer, offset, size}` sobre nuestros buffers subasignados | Handles/offsets, no direcciones GPU obligatorias |
| Root data pequeño | `GraphicsCommands::push_root`, usado por los uniforms locales | Push constants validados y layouts existentes, no `vkCmdPushDataEXT` |
| Rendering con attachments | `RenderingTarget`, begin/end dinámicos, pipelines por formatos | Se conserva el fallback `VkRenderPass`/framebuffer |
| Dependencias productor/consumidor | `AccessScope`, barreras globales y `ImageUse` para transiciones internas | Layouts óptimos por uso en lugar de imponer `GENERAL` |
| Envío asíncrono y lifetime explícito | `Device::submit`, dos slots, fences, memoria por slot | Todavía sin timelines públicas ni cola de borrado diferido |
| Memoria controlada por aplicación | Suballocators propios y UBOs persistentemente mapeados | No se impone ReBAR; siguen existiendo staging y descriptores compatibles |

Los sistemas públicos siguen sin conocer tipos Vulkan. No se añadió una API
general para compute, meshlets o ray tracing que aún no necesitamos. Los shaders
siguen siendo Slang, con las convenciones GLM/matrices del motor; no se introdujo
GLSL ni se cambió a los tipos matemáticos de la librería.

Dynamic rendering elimina objetos render-pass/framebuffer en dispositivos que
lo soportan; synchronization2 simplifica dependencias y envío. Son capacidades
independientes, detectadas por features y extensiones, con entry points core 1.3
o KHR. La ruta compatible mantiene el mínimo Vulkan 1.2 y las features que ya
usábamos. Referencias técnicas: [Khronos: dynamic rendering](https://docs.vulkan.org/samples/latest/samples/extensions/dynamic_rendering/README.html)
y [synchronization2](https://docs.vulkan.org/guide/latest/extensions/VK_KHR_synchronization2.html).

## Cambios de rendimiento y seguridad

1. **GPU real por defecto.** Los wrappers/devshell agregan los ICD de Mesa Nix
   mediante `VK_ADD_DRIVER_FILES`; no fuerzan Lavapipe. Respetan overrides Vulkan
   explícitos y permiten `auto`, `software` y `system`. El selector elige una GPU
   discreta/integrada adecuada antes que CPU. NVIDIA propietario en Linux depende
   de que sus librerías del host sean utilizables en ese entorno; no se promete
   resolver automáticamente cualquier mezcla de drivers/Nix. Windows no utiliza
   estos wrappers Linux.
2. **CPU y GPU se solapan.** Se eliminó `vkDeviceWaitIdle` del frame normal, después
   de resolver las dependencias que ocultaba. Cada slot tiene acquire semaphore,
   fence, descriptores y rangos UBO independientes. Cada imagen del swapchain tiene
   su depth, command buffer y semáforo de presentación. Su fence se espera **antes**
   de volver a grabar. El criterio de semáforos sigue la
   [guía de reutilización de Khronos](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html).
3. **Menos trabajo de uniforms.** Mapping persistente; descriptor global escrito
   una vez por slot; revisión de contenido por instancia/slot para no subir
   materiales sin cambios. El tracking de memoria y los contenedores propios se
   conservan. No se cambió el allocator para obtener estas mejoras.
4. **Mipmaps al cargar.** Se generan todos los niveles en la GPU, en una sola
   submission de upload, si el formato permite ambos blits y filtrado lineal.
   El sampler expone los niveles inicializados y limita anisotropía al máximo
   soportado, hasta 16. No se baja resolución de pantalla, geometría ni iluminación.
   El fallback es mip 0. Se validan dimensiones, niveles y transiciones, siguiendo
   el modelo de [Khronos: mipmaps](https://docs.vulkan.org/tutorial/latest/09_Generating_Mipmaps.html).
5. **Medición reproducible.** `NK_BENCHMARK=1` congela la rotación de los cubos y
   activa timestamps GPU. Solo se leen queries ya retiradas por fence, sin
   `WAIT_BIT`; se respeta `timestampValidBits` y `timestampPeriod`. Si no existen
   timestamps, se informa `gpu_ms=-1`. No hay queries en una ejecución normal.

Resize, cargas, destrucción de recursos y shutdown conservan algunas esperas
globales: no forman parte del frame estable. Se corrigieron también lifetime de
views/imágenes/memoria y el array de queue families usado al crear el swapchain.

Los mipmaps agregan memoria y trabajo al cargar: para una textura cuadrada grande,
la cadena ocupa aproximadamente un tercio adicional. El filtrado conserva el
formato UNORM actual. Un pipeline de assets con sRGB, mipmaps especializados para
normales/alpha y compresión de texturas sería otro trabajo, no está implícito aquí.

## Mediciones

Release, vista inicial fija de Sponza + Falcon + cubos + UI, 936×999, sin mover la
cámara ni compilar simultáneamente. Presentación mailbox. Se descartan 60 frames;
las cantidades efectivamente medidas están en
[vulkan-performance-sponza.csv](vulkan-performance-sponza.csv).

| Variante | ms/frame medios | FPS de render | ms GPU medios |
|---|---:|---:|---:|
| Original con Lavapipe forzado | 43,600 | 22,94 | 43,365 |
| Original seleccionando explícitamente RADV | 2,935 | 340,72 | 1,801 |
| Selección automática de GPU | 2,927 | 341,63 | 1,817 |
| Frames solapados, memoria aislada | 2,077 | 481,43 | 1,753 |
| Capa moderna, todavía sin mipmaps | 2,062 | 484,88 | 1,753 |
| Mipmaps, primera ejecución A/B | 1,829 | 546,70 | 1,542 |
| Mismo código con mipmaps desactivados | 1,994 | 501,50 | 1,725 |
| Mipmaps, repetición | 1,845 | 541,87 | 1,526 |

Son corridas cortas, no percentiles ni una garantía para cualquier posición de
cámara. El iGPU comparte memoria, potencia y condiciones térmicas con la CPU.
Los FPS describen throughput de render, **no** imágenes distintas visibles por
segundo en el monitor. El frame interval incluye trabajo CPU, GPU y presentación;
no es una medición aislada de tiempo CPU ni de latencia de entrada.

La mayor ganancia es corregir el renderer software (~15× al elegir hardware).
El solapamiento mejora otro ~41% sobre hardware automático. La migración de
comandos por sí sola no muestra una mejora significativa: 1,753 ms GPU en ambos
casos. En el A/B de mipmaps, la GPU baja aproximadamente 11% y los FPS suben
8–9%. No se atribuyen las mejoras completas a NoGraphicsAPI.

Los SHAs del CSV identifican los cambios correspondientes; las mediciones se
tomaron durante su desarrollo antes del commit. Los logs completos de esta
sesión quedaron en `/tmp/nk-vulkan-analysis.TT2b1I/`; no se requiere ese directorio
para construir, ejecutar ni repetir las pruebas.

## Uso y regresión

```bash
nix run .#build -- Release --parallel 4
nix run .#run -- Release
nix develop --command .scripts/benchmark.sh Release 1260
```

Comparación sobre **el mismo binario**, manteniendo tamaño, cámara y alimentación:

```bash
nix develop --command .scripts/benchmark.sh Release 1260
NK_VULKAN_MIPMAPS=0 nix develop --command .scripts/benchmark.sh Release 1260
NK_VULKAN_LEGACY=1 nix develop --command .scripts/benchmark.sh Release 1260
```

Driver de diagnóstico o selección externa:

```bash
NK_VULKAN_DRIVER=software nix run .#run -- Release
NK_VULKAN_DRIVER=system nix run .#run -- Release
# VK_DRIVER_FILES=/ruta/al/icd.json nix run .#run -- Release
```

`NK_VULKAN_DRIVER` se evalúa al entrar al devshell/wrapper Nix; cambiarlo después
de entrar a un shell ya configurado no reescribe sus variables Vulkan. Eliminar
la variable de diagnóstico restaura el comportamiento normal. Los overrides
Vulkan explícitos tienen prioridad, incluso sobre `NK_VULKAN_DRIVER`.

Pruebas ejecutadas:

- Debug: **228/228**; Release: **219/219**; ASan/UBSan: **228/228**.
- Sponza/Falcon, tres modos de renderizado y cierre normal bajo validación Vulkan
  y de sincronización, con mipmaps, tanto en ruta moderna como fallback.
- Resize nativo en niri: 936×999 → 700×700 → 1100×800 → 936×999;
  4000 iteraciones por ruta, **3992 frames estables sin allocations**, sin errores
  de validación y sin fugas del tracking del motor.
- Selección explícita de Lavapipe: smoke de 120 frames, sin errores de validación.
- Paquete `nix build .#nk-engine` y ejecución de su `nk-editor`: GPU RADV seleccionada,
  Falcon/Sponza cargados y cierre normal.
- Windows/NVIDIA: **no ejecutado en esta sesión**. Antes de darlo por validado se
  necesitan GPU/driver exactos y repetir Debug/Release, resize, cierre y ambos
  caminos en ese equipo. Tampoco se probó físicamente una GPU limitada a Vulkan
  1.2; forzar legacy prueba el camino de código en la GPU disponible.

Ejemplo para repetir validación (requiere las validation layers del devshell):

```bash
nix run .#build -- Debug --parallel 4
NK_SMOKE_TEST_FRAMES=360 NK_SMOKE_TEST_CYCLE_RENDER_MODES=1 \
  VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT \
  nix run .#run -- Debug
```

`VK_LAYER_ENABLES` funciona en la versión probada, aunque avisa que está deprecada.
Comprobar los mensajes de validación, no solo el código de salida: el callback
actual registra errores sin convertirlos automáticamente en exit no-cero.
No comparar FPS con validation layers o sanitizadores activados. Las builds
Debug normal y sanitizada comparten actualmente el directorio de ejecutables;
relinkar los ejecutables normales después de usar ASan evita mezclar variantes.

Para revertir: mipmaps y ruta moderna tienen los interruptores anteriores.
La sincronización/aislamiento de recursos forma un cambio atómico: no eliminar
sus esperas/fences o volver a compartir UBO/depth aisladamente. Si fuera necesario
revertirlo, hay que revertir el commit completo y sus dependientes mediante un
commit de reversión, conservando el historial y repitiendo validación.

## Próximos candidatos, aún no implementados

- Culling espacial y de frustum: el importador agrupa Sponza en 25 grupos globales
  por material; muchos abarcan gran parte del edificio. Para ganar de verdad hay
  que evaluar partición espacial/bounds, no asumir que ocultar el mesh completo
  o añadir un test por material elimina su coste.
- Caché de matrices normales/modelo: `MaterialSystem::apply_local` sigue calculando
  la normal matrix por subgeometría. Puede reducir CPU, pero no explica el cuello
  dominante observado ni se cuantificó como mejora aquí.
- Compresión y mips offline: reducir memoria/bandwidth del iGPU; requiere definir
  formatos, semántica de color y fallback. No alterar alpha/blending indiscriminadamente.
- Uploads por lotes y destrucción diferida por timeline: útiles para streaming;
  las esperas de carga actuales no explican los FPS estables.
- BDA/vertex pulling, descriptor indexing o heaps e indirect draws: añadir como
  capacidades opcionales solo con A/B frente a vertex fetch/descriptores actuales.
  Una API más reciente no garantiza menor tiempo GPU. Mesh shaders no deben ser
  un requisito para dibujar las mallas que ya soporta el motor.

Los cambios futuros deben mantener la frontera pequeña de comandos y ownership
explícito inspirada en NoGraphicsAPI, sin implementar toda su superficie por
anticipado ni cambiar el mínimo de hardware sin una decisión explícita.
