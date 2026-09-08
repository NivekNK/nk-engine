# Plan de implementación de Kohi 60–72 en NK Engine

- Estado: planificado
- Fecha de análisis: 2026-09-08
- Punto de partida: capítulos 41–59 adaptados en `feature/textures`
- Baseline del repositorio al preparar el plan: `2421da5`

## Objetivo

Ordenar la historia de Kohi desde el capítulo 60 hasta el 72, incluidos el
parche 70.1 y todos los commits auxiliares proporcionados, y convertirla en un
plan implementable sobre NK Engine. La referencia define el comportamiento que
queremos aprender y conservar; no define literalmente nuestra API, ownership ni
arquitectura C++.

Este tramo incorpora cinco bloques relacionados:

1. cámaras, vistas, transparencia y skybox;
2. toolchain de assets y shaders;
3. primitivas de threading, scheduler de jobs y carga asíncrona;
4. alineación, tracking multihilo y callbacks de memoria Vulkan;
5. buffers de render genéricos y subasignación orientada a rangos.

## Fuentes y criterio histórico

- Los títulos y el contenido funcional de los capítulos se contrastaron con la
  playlist indicada y con los parches de los commits de Kohi.
- Los enlaces proporcionados son commits; algunos representan el squash final
  de un PR y otros son merges de procedencia. Un merge no se tratará como una
  segunda implementación cuando no aporta estado funcional nuevo.
- Los fixes posteriores se asignan al sistema que corrigen, aunque su fecha esté
  entre dos vídeos.
- La referencia Vulkan moderna seguirá siendo
  [NoGraphicsAPI](https://github.com/sebbbi/NoGraphicsAPI), bajo la adaptación ya
  acordada en
  [`vulkan-performance-and-nographicsapi.md`](vulkan-performance-and-nographicsapi.md):
  backend propio, capacidades detectadas en runtime y rutas modernas sin exigir
  sus requisitos mínimos a todas las GPUs.
- Para callbacks de asignación Vulkan se seguirá el contrato oficial de
  [`VkAllocationCallbacks`](https://docs.vulkan.org/refpages/latest/refpages/source/VkAllocationCallbacks.html)
  y la sección de
  [host memory](https://docs.vulkan.org/spec/latest/chapters/memory.html#memory-host)
  de la especificación, no sólo el ejemplo de Kohi.

## Reglas de adaptación obligatorias

- Mantener C++ idiomático: tipos con invariantes y métodos, RAII donde el
  ownership lo permita, move semantics y handles generacionales. No copiar APIs
  C con tablas extensas de callbacks si una interfaz C++ expresa mejor el
  contrato.
- Usar los allocators, `arr`, `dyarr`, `map`, `slice`, `str` y `result` propios.
  No introducir contenedores STL con asignación dinámica en runtime.
- Toda operación recuperable nueva devuelve `nk::result<T, E>`. Un fallo de
  reserva, creación Vulkan o publicación asíncrona no debe dejar estado parcial.
- Todo shader nuevo será Slang. No se añadirán GLSL, `glslc` ni una herramienta
  que vuelva a introducir esa ruta.
- Linux usa Wayland/xdg-shell y debe funcionar bajo niri. No se implementarán
  macOS, GLFW ni una dependencia nueva de X11; el fallback Linux ya existente se
  conserva sin expandirlo.
- Windows conserva paridad funcional con Win32. No se considerará válido un
  diseño de jobs que sólo sea seguro en Linux.
- Las llamadas de presentación, creación de recursos GPU y publicación de
  resultados permanecerán en el hilo principal/render hasta definir de forma
  explícita colas Vulkan multihilo.
- No se usará espera activa. Los workers dormirán mediante condition variables y
  el loop suspendido por tamaño cero esperará eventos o aplicará backoff acotado.
- Ningún frame estable debe reservar memoria del heap general. Los paquetes de
  vista, listas de transparencia y comandos de upload usarán capacidad reservada,
  scratch por frame o rangos persistentes.
- Cada cambio importante se cerrará con un commit semántico cuyo asunto describa
  el resultado y no mencione el número del capítulo ni una fase.
- Una librería nueva sólo se acepta después de justificar que el código propio o
  una dependencia existente no cubren el caso. Debe agregarse como submódulo
  fijado a un tag y registrarse en `.scripts/libraries.csv` en el mismo cambio.
- No se prevé una librería nueva para este tramo. NoGraphicsAPI continúa como
  referencia de diseño, no como dependencia enlazada.

## Segmentación histórica resumida

| Capítulo | Tema | Referencia autoritativa | Commits auxiliares |
|---|---|---|---|
| 60 | Camera System | [`f43ae30`](https://github.com/travisvroman/kohi/commit/f43ae30483398f0d843383108580b19f1075d21a) | — |
| 61 | View System | [`6c2c2d9`](https://github.com/travisvroman/kohi/commit/6c2c2d9cb719210a534c7fb6bcdf61ffcf78e7e6) | — |
| 62 | Transparency | [`80a30b0`](https://github.com/travisvroman/kohi/commit/80a30b0b8da8334dd9912fbb69ca424810957e6e) | fix obligatorio [`a6280c6`](https://github.com/travisvroman/kohi/commit/a6280c6dda14c5d32a21926d7d95162877aa84af) |
| 63 | Cubemaps and Skybox | [`ab99e89`](https://github.com/travisvroman/kohi/commit/ab99e894b24a18598412c42c1545c892042e3dfa) | auditoría transversal [`1d500f6`](https://github.com/travisvroman/kohi/commit/1d500f65769276a00fc42117314ee5f554146371) |
| 64 | Toolchain | [`df2904e`](https://github.com/travisvroman/kohi/commit/df2904ec8abb184fd2916d2c4ad20c44f8968e65) | fix [`0b243c5`](https://github.com/travisvroman/kohi/commit/0b243c52a0aabbd7885424b2c5a65151235950b3) |
| 65 | Multithreading, Linux | [`76b9734`](https://github.com/travisvroman/kohi/commit/76b9734e6cbe360c1fd3b186b97b816b29ade57b) | — |
| 66 | Multithreading, Windows | [`fc0f503`](https://github.com/travisvroman/kohi/commit/fc0f503a8917382a643331692c78a911276a22aa) | — |
| 67 | Job System | [`56d2599`](https://github.com/travisvroman/kohi/commit/56d2599ea4a70718c7409372dbd845406f6850a9) | fix obligatorio [`97b4b5b`](https://github.com/travisvroman/kohi/commit/97b4b5bb02fafcc852fcaea9c73e771ed5e82542) |
| 68 | Jobified texture/mesh loading | [`d8936d2`](https://github.com/travisvroman/kohi/commit/d8936d24c305997c8e13f52ddb9ca13cd8775956) | — |
| 69 | Bug-fix pass | [`c8fd2cd`](https://github.com/travisvroman/kohi/commit/c8fd2cdbf986038f21f89392642b76807278bb12) | familia #80 y typo documental |
| 70 | Alignment and allocator updates | [`e5e044e`](https://github.com/travisvroman/kohi/commit/e5e044ebb4fd4b1e595cf50822f63cf81215c356) | merge [`e21b105`](https://github.com/travisvroman/kohi/commit/e21b105bf339cc2a8f58cb1ac8095f5a1c0648ec) |
| 70.1 | Alignment fixes | [`c732a24`](https://github.com/travisvroman/kohi/commit/c732a24fcb7f630ad1cca3bd6331468ac1c5ad86) | parche que sustituye parte de 70 |
| 71 | Custom Vulkan allocator | [`5755dc0`](https://github.com/travisvroman/kohi/commit/5755dc0e23877be5643874bdf398359a9bce220c) | merge [`775c646`](https://github.com/travisvroman/kohi/commit/775c64633bdf460217b862815ab5f48a6d0e445e) |
| 72 | Generic renderbuffers | [`5ea625c`](https://github.com/travisvroman/kohi/commit/5ea625c2de2a48bedb52cdb9e77b4b23954cde24) | — |

### Commits duplicados o de procedencia

La corrección de espera durante recreación de swapchain aparece repetida en
[`b0f0ddb`](https://github.com/travisvroman/kohi/commit/b0f0ddb5455a376fd833c9a0772cfc9a8dfe526d),
[`f326dc3`](https://github.com/travisvroman/kohi/commit/f326dc356c7c11f32ea436486fcfdf3161161e53),
[`8cca36a`](https://github.com/travisvroman/kohi/commit/8cca36a656eeb638f3938c2a1e17e50c2ad5e161)
y el merge
[`1669a4e`](https://github.com/travisvroman/kohi/commit/1669a4eed609d8be41092b9d269b542c67e15227).
El estado final se evaluará una vez dentro del capítulo 69; no se copiarán cuatro
variantes del mismo cambio. El commit
[`9c3dcd1`](https://github.com/travisvroman/kohi/commit/9c3dcd1c7cb0973c7d9f5e84e787f7c684266e8e)
sólo corrige un comentario en `event.h` y se conserva en el inventario como
procedencia, no como trabajo funcional.

De la misma forma, `e21b105` integra el estado de 70 y `775c646` integra 70.1 con
71. Se revisarán sus árboles finales para comprobar que no haya cambios únicos,
pero no generarán implementaciones duplicadas.

## Estado de NK Engine antes de comenzar

| Área | Estado actual | Brecha relevante |
|---|---|---|
| Cámara | `Camera` es un puente estático que sólo envía view/posición al renderer. | Falta un valor de cámara autónomo, cache de view y sistema de handles. |
| Vistas | `Renderer` conoce directamente world y UI y recibe un `RenderPacket` fijo. | Falta separar construcción de vistas de la ejecución del frame. |
| Transparencia | El pipeline habilita blending globalmente y ata depth-write a depth-test. | Faltan estados por material/pipeline y orden back-to-front. |
| Texturas | Existe sistema 2D con mipmaps, samplers y bindings. | Falta dimensión cube, seis layers y compatibilidad binding/view. |
| Shaders | CMake compila Slang con `slangc`. | El `.bat` antiguo aún invoca `glslc`; falta un flujo único y verificable. |
| Threading/jobs | No hay abstracción propia de thread, mutex, condition variable, ring ni scheduler. | Debe definirse ownership, parada cooperativa y seguridad de allocators. |
| Carga de recursos | Imagen y mesh se cargan de forma síncrona. | Falta separar trabajo CPU de publicación y upload GPU. |
| Memoria CPU | Alineación, `FreeList` y `FreeListAllocator` ya superan parte del estado de Kohi 70. | El tracking y los allocators no tienen aún contrato multihilo explícito. |
| Memoria Vulkan | Los objetos aceptan `VkAllocationCallbacks*`, actualmente nulo. | Falta callback opcional correcto y contabilidad separada de memoria GPU. |
| Buffers | `vk::Buffer`, `BufferSuballocator` y `vk::BufferView` existen. | Falta una fachada renderer-neutral y eliminar esperas globales del flujo de copia/resize. |

Los capítulos 70–72, por tanto, no reemplazarán ciegamente código existente:
cerrarán brechas concretas y conservarán las garantías ya probadas.

## Orden global y dependencias

```text
60 Camera ── 61 View System ──┬─ 62 Transparency
                              └─ 63 Cubemap/Skybox ── 64 Toolchain

65 Linux threads ──┬─ 67 Job System ── 68 Async resources ── 69 Regression gate
66 Win32 threads ──┘

67/68 multihilo ── 70 Alignment + allocator synchronization ── 70.1 hardening
                                                            │
                                                            └─ 71 Vulkan host callbacks

44/current BufferSuballocator + 58/59 render targets + 71 contracts
                                      └─ 72 Generic RenderBuffers
```

Los capítulos 62 y 63 dependen del modelo de vistas, pero son independientes
entre sí salvo por el orden de pass elegido. Los capítulos 65 y 66 deben cerrar
la misma interfaz antes de exponer el JobSystem. El capítulo 72 puede usar el
modelo de rangos existente, pero sólo se migrará después de estabilizar el
tracking multihilo y los contratos Vulkan.

---

## Capítulo 60 — Camera System

- Vídeo: [Kohi #060: Camera System](https://youtu.be/6UMJzh1RdOo?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj)
- Referencia: [`f43ae30`](https://github.com/travisvroman/kohi/commit/f43ae30483398f0d843383108580b19f1075d21a)

### Alcance histórico

Kohi convierte la cámara en un objeto con posición, Euler rotation, view
cacheada e invalidación por dirty bit. Añade movimientos y vectores de dirección,
limita pitch y construye un sistema de cámaras nombradas con referencias.

### Diseño NK

- [ ] Reemplazar el puente estático por un `Camera` movible o copiable como valor,
  con `glm::vec3` para posición/rotación y `glm::mat4` para la view cacheada.
- [ ] Invalidar la matriz sólo al cambiar posición u orientación. `view()` debe
  recalcular de forma lazy sin reservar memoria.
- [ ] Definir una convención única de handedness, ejes, orden Euler, yaw y pitch;
  limitar pitch a un margen seguro cercano a 90 grados sin introducir roll
  accidental.
- [ ] Añadir movimiento local (`forward`, `backward`, `left`, `right`, `up`,
  `down`) y vectores base normalizados usando GLM, que ya es una dependencia.
- [ ] Crear `CameraSystem` con capacidad explícita, `map<str, CameraHandle>` y
  slots generacionales. Un handle debe ser `{index, generation}`, no un puntero
  prestado que pueda sobrevivir a la reutilización del slot.
- [ ] Implementar cámara por defecto, adquisición/release con refcount y política
  explícita para recursos auto-release. Un fallo de nombre, capacidad u OOM debe
  devolverse mediante `result` y hacer rollback completo.
- [ ] Hacer que la aplicación seleccione su cámara activa; retirar gradualmente
  `Camera::set_view` cuando el sistema de vistas pueda consumirla.

### Validación y commits

- [ ] Unit tests de base ortonormal, movimiento, clamp de pitch, reset, dirty
  cache y estabilidad tras múltiples lecturas.
- [ ] Tests de cámara por defecto, nombre duplicado, refcount, stale handle,
  capacidad agotada y OOM durante inserción.
- [ ] Verificar que actualizar y leer la cámara no asigna memoria por frame.
- [ ] Commit sugerido: `feat(renderer): add managed camera values and handles`.
- [ ] Si la migración de la aplicación es suficientemente independiente:
  `refactor(editor): drive the world view through an active camera`.

---

## Capítulo 61 — View System

- Vídeo: [Kohi #061: View System](https://youtu.be/f8fx0HB6GgY?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj)
- Referencia: [`6c2c2d9`](https://github.com/travisvroman/kohi/commit/6c2c2d9cb719210a534c7fb6bcdf61ffcf78e7e6)

### Alcance histórico

Kohi crea un sistema de vistas nombradas y mueve fuera del renderer la lógica
específica de world/UI, sus matrices, resize, construcción de paquetes y
secuencia de render passes.

### Diseño NK

- [ ] Definir `RenderViewConfig`, `RenderViewHandle` y `RenderViewPacket` en la
  capa renderer-neutral. El packet presta datos por un frame y nunca toma
  ownership implícito.
- [ ] Introducir inicialmente vistas `World` y `UI`; reservar la extensión
  `Skybox` para el capítulo 63 sin exponer tipos Vulkan.
- [ ] Mantener en `Renderer` sólo begin/end frame, adquisición de targets y
  ejecución backend. Cada vista prepara sus datos y la secuencia lógica de
  passes.
- [ ] Preferir clases/structs C++ con métodos y dispatch tipado para vistas
  built-in. No copiar una tabla C de callbacks configurables antes de necesitar
  plugins de vistas.
- [ ] Hacer que el sistema posea nombres, handles y referencias a passes/targets.
  Destrucción, resize y fallo parcial deben respetar
  [`renderer-resource-lifetime-contracts.md`](renderer-resource-lifetime-contracts.md).
- [ ] Preparar paquetes en scratch preasignado o capacidad reutilizable. No crear
  dynamic arrays nuevas en cada frame como hace esta etapa de Kohi.
- [ ] Actualizar proyección al cambiar extent, tratar `0x0` como suspensión y
  publicar el nuevo estado sólo después de reconstruir todo correctamente.

### Validación y commits

- [ ] Tests de orden world/UI, lookup, stale handle, configuración inválida,
  resize, extent cero, rollback y capacidad agotada.
- [ ] Test con renderer falso que compruebe el protocolo begin/pass/end y que un
  fallo intermedio no ejecute operaciones posteriores.
- [ ] Comprobar cero allocations en construcción y ejecución de un frame estable.
- [ ] Commit sugerido: `feat(renderer): add configurable render views`.
- [ ] Commit de migración sugerido:
  `refactor(renderer): route frame packets through render views`.

---

## Capítulo 62 — Transparency

- Vídeo: [Kohi #062: Transparency](https://youtu.be/duQSUgdrBk0?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj)
- Referencia principal: [`80a30b0`](https://github.com/travisvroman/kohi/commit/80a30b0b8da8334dd9912fbb69ca424810957e6e)
- Fix obligatorio: [`a6280c6`](https://github.com/travisvroman/kohi/commit/a6280c6dda14c5d32a21926d7d95162877aa84af)

### Alcance histórico

La vista world separa geometría opaca y transparente, ordena esta última desde
el fondo hacia la cámara y corrige el cálculo de bounds máximos usado para sus
centros. El fix `a6280c6` pertenece funcionalmente aquí: el commit del vídeo no
actualizaba correctamente los máximos.

### Diseño NK

- [ ] Expresar `Opaque`, `Masked` y `Transparent` como modo de material/pipeline.
  No inferir transparencia exclusivamente al encontrar algún alpha en la imagen.
- [ ] Crear variantes de pipeline cacheables: opaco con depth-test/write y blend
  desactivado; transparente con depth-test activo, depth-write desactivado y
  mezcla `src alpha / one-minus-src-alpha`.
- [ ] Separar draw items opacos y transparentes en la vista world. Dibujar los
  opacos primero y los transparentes back-to-front.
- [ ] Ordenar por profundidad en view space o distancia cuadrada al centro
  transformado, evitando `sqrt`. Añadir un desempate determinista para reducir
  cambios de orden entre frames.
- [ ] Conservar centro y extents por submesh y transformar el centro mediante el
  model matrix. Validar mínimos y máximos con el fix posterior de Kohi.
- [ ] Reservar scratch para sort keys/draw items y manejar de forma segura listas
  vacías o de un elemento; no copiar el underflow potencial de un quicksort C.
- [ ] Mantener alpha correcto en el fragment shader Slang y validar la convención
  premultiplied/non-premultiplied elegida. La primera versión usará alpha no
  premultiplicado de forma consistente con los factores indicados.

### Validación y commits

- [ ] Tests de bounds negativos/positivos, centro transformado, orden vacío,
  unitario, distancias iguales y cámara en movimiento.
- [ ] Tests del estado de pipeline: blending, factores, depth test y depth write.
- [ ] Escena visual con material opaco delante/detrás de dos superficies alpha;
  revisar también Sponza por sus cortinas y vegetación.
- [ ] Validation Layers sin hazards ni incompatibilidades de pipeline/pass.
- [ ] Commit sugerido: `feat(renderer): sort and blend transparent geometry`.
- [ ] Fix independiente si corresponde:
  `fix(resources): preserve complete mesh bounds during import`.

---

## Capítulo 63 — Cubemaps and Skybox

- Vídeo: [Kohi #063: Cubemaps and Skybox](https://youtu.be/-6yPOAwbjSI?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj)
- Referencia principal: [`ab99e89`](https://github.com/travisvroman/kohi/commit/ab99e894b24a18598412c42c1545c892042e3dfa)
- Auditoría transversal: [`1d500f6`](https://github.com/travisvroman/kohi/commit/1d500f65769276a00fc42117314ee5f554146371)

### Alcance histórico

Kohi carga seis caras, crea una imagen compatible con cubemap, una image view
cube y un sampler clamp-to-edge. También añade geometría, shader, pass y vista
skybox. `1d500f6` agrega paréntesis a una macro de clamp; en NK se traducirá en
una auditoría de expresiones/templates, no en importar la macro.

### Diseño NK

- [ ] Añadir dimensión explícita a texturas y bindings: al menos `Texture2D` y
  `TextureCube`. El tipo de descriptor debe rechazar combinaciones incompatibles.
- [ ] Definir un `CubemapConfig` con orden de caras documentado y comprobado
  (`+X`, `-X`, `+Y`, `-Y`, `+Z`, `-Z`). Cargar las seis como una transacción:
  mismo width, height y formato, con rollback total ante cara ausente o distinta.
- [ ] Crear una imagen Vulkan con seis array layers, flag cube-compatible y view
  `VK_IMAGE_VIEW_TYPE_CUBE`. Las transiciones y copias deben cubrir rangos de
  layers explícitos, compatibles con sync2 y con el fallback legado.
- [ ] Añadir sampler clamp-to-edge y permitir culling frontal al dibujar el cubo
  desde dentro. El cull mode debe formar parte de la configuración/cache key del
  pipeline.
- [ ] Crear `Builtin.SkyboxShader.vertex.slang`, fragment Slang y `.shadercfg`.
  Eliminar traslación de la view y adaptar la profundidad a las convenciones
  Vulkan/GLM actuales; no copiar literalmente el truco GLSL sin comprobar clip
  space y reversed-Z si éste cambia en el futuro.
- [ ] Añadir `Skybox` al sistema de vistas. Orden inicial: skybox, world y UI. El
  contrato de load/clear de color/depth debe expresarse en la configuración de
  targets/passes existente, no mediante clears ad hoc.
- [ ] Importar el skybox de referencia de Kohi con atribución y licencia. Como el
  loader actual es PNG, convertir una vez las seis caras JPEG decodificadas a PNG
  lossless y versionar los resultados, hashes de origen y procedencia en
  `engine/assets/ATTRIBUTION.md`. Esto conserva los píxeles decodificados sin
  añadir un decoder JPEG sólo por seis assets fijos.
- [ ] Si se decide que JPEG será formato runtime soportado —una decisión distinta
  de este skybox—, abrir un cambio separado con `libjpeg-turbo` fijado a un tag,
  submódulo y fila CSV. No introducir `stb_image` de forma incidental.

### Validación y commits

- [ ] Tests de orden de caras, overflow de tamaño total, mismatch de dimensiones,
  cara ausente, rollback, binding cube/2D y rangos de seis layers.
- [ ] Smoke visual con orientación inequívoca de cada cara, cámara rotando y
  moviéndose; el skybox rota pero no se traslada.
- [ ] Validar rutas dynamic rendering/sync2 y render pass/barriers legados.
- [ ] Commit sugerido: `feat(renderer): add cubemap textures and skybox rendering`.
- [ ] Assets/licencia pueden cerrarse juntos si son inseparables del feature;
  nunca en un commit sin atribución.

---

## Capítulo 64 — Toolchain

- Vídeo: [Kohi #064: Toolchain](https://youtu.be/cCEmwjfHIkY?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj)
- Referencia principal: [`df2904e`](https://github.com/travisvroman/kohi/commit/df2904ec8abb184fd2916d2c4ad20c44f8968e65)
- Fix asociado: [`0b243c5`](https://github.com/travisvroman/kohi/commit/0b243c52a0aabbd7885424b2c5a65151235950b3)

### Alcance histórico

Kohi añade un ejecutable `tools` que envuelve `glslc`, scripts post-build y
configuración del editor. El fix posterior corrige que el script compilaba dos
veces UI y omitía skybox. El objetivo válido para NK es la reproducibilidad del
pipeline de assets, no copiar ese wrapper ni regresar a GLSL.

### Diseño NK

- [ ] Mantener CMake como fuente única del grafo de compilación Slang y assets.
  Enumerar/manifestar entradas para que nuevas vistas como skybox no se omitan.
- [ ] Corregir o retirar `.scripts/compile-shaders.bat`, que todavía referencia
  `glslc` y shaders deprecated. Tanto Bash como PowerShell deben invocar el mismo
  target CMake/Nix y aceptar rutas con espacios.
- [ ] Asegurar que un cambio en `.slang`, `.shadercfg` o un asset procesable
  reconstruya sólo su salida y que una build limpia no dependa de archivos
  generados localmente sin declarar.
- [ ] Validar nombres y stages soportados al configurar. Un shader mal nombrado
  debe fallar con un diagnóstico claro, no producir un SPIR-V ambiguo.
- [ ] No crear aún `nk-tools` si sólo ejecutaría otro proceso mediante strings.
  Introducirlo cuando exista una transformación propia —por ejemplo cache de
  mesh o manifest empaquetado— con inputs/outputs y errores tipados.
- [ ] Registrar versión de `slangc` en el diagnóstico de build y mantener el
  paquete correspondiente en `flake.nix`; no es un submódulo enlazado.

### Validación y commits

- [ ] Build limpia e incremental en Nix/Linux y generación equivalente en
  Windows; tocar un solo shader debe regenerar una sola salida.
- [ ] Test/script que compruebe que material, UI y skybox poseen binarios y
  configs en el directorio ejecutable.
- [ ] Verificar que no quedan `.glsl`, referencias a `glslc` ni artefactos del
  renderer simple.
- [ ] Commit sugerido:
  `build(shaders): unify Slang asset compilation across platforms`.

---

## Capítulo 65 — Multithreading Part 1, Linux

- Vídeo: [Kohi #065: Multithreading Part 1, Linux](https://youtu.be/NxtqrN6Jw-4?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj)
- Referencia: [`76b9734`](https://github.com/travisvroman/kohi/commit/76b9734e6cbe360c1fd3b186b97b816b29ade57b)

### Alcance histórico

Kohi introduce thread/mutex nativos y consulta de procesadores. También usa
cancelación forzada y serializa gran parte del MemorySystem con un mutex; ambas
decisiones deben revisarse antes de adaptarlas.

### Diseño NK

- [ ] Diseñar `Thread`, `Mutex`, `LockGuard` y `ConditionVariable` con métodos,
  ownership explícito y backend pthread en Linux. Evitar una asignación heap por
  objeto mediante almacenamiento nativo inline o implementación privada fija.
- [ ] Proveer start, join, detach sólo si existe un caso válido, thread id y
  consulta de logical CPU count mediante `result` donde la plataforma pueda
  fallar.
- [ ] Usar parada cooperativa observada por el worker. No usar `pthread_cancel`,
  señales asíncronas ni abandonar recursos owned por un thread.
- [ ] Garantizar que destruir un `Thread` joinable produzca una política segura y
  explícita; preferentemente exigir `join()` durante shutdown y comprobarlo en
  debug.
- [ ] Permitir configuración de número de workers y fallback síncrono con cero
  workers. Un equipo de un solo core no debe hacer fallar el engine.
- [ ] Declarar desde aquí el contrato de memoria: los allocators actuales son
  thread-compatible sólo bajo sincronización externa. No poner un mutex global
  alrededor de todo el sistema sin medición; jobs usarán allocator sincronizado
  dedicado y scratch por worker.
- [ ] Mantener toda interacción Wayland en el hilo de plataforma. No añadir X11,
  GLFW ni trabajo para macOS.
- [ ] Enlazar `Threads::Threads` desde CMake en Linux. Es una dependencia de
  sistema, no una librería vendorizada.

### Validación y commits

- [ ] Tests de start/join, identificación, mutex, lock guard, condition variable,
  wake-up espurio, stop cooperativo y errores de doble inicio/doble join.
- [ ] Smoke de shutdown repetido y suite opcional TSAN cuando el toolchain lo
  soporte sin falsos positivos conocidos de Wayland/Vulkan.
- [ ] Commit sugerido:
  `feat(platform): add cooperative Linux threading primitives`.

---

## Capítulo 66 — Multithreading Part 2, Windows

- Vídeo: [Kohi #066: Multithreading Part 2, Windows](https://youtu.be/jmt_BE6ny1g?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj)
- Referencia: [`fc0f503`](https://github.com/travisvroman/kohi/commit/fc0f503a8917382a643331692c78a911276a22aa)

### Alcance histórico

El commit aporta la contraparte Win32 y contiene además fixes de filesystem,
resize inicial y export de cámara. Esos cambios laterales deben evaluarse, no
copiarse automáticamente.

### Diseño NK

- [ ] Implementar la misma API pública mediante handles Win32,
  `SRWLOCK`/`CONDITION_VARIABLE` o primitivas equivalentes, conservando las
  mismas garantías de ownership y errores que Linux.
- [ ] Usar parada cooperativa y `WaitForSingleObject`/join. No usar
  `TerminateThread`, que rompe destructores, locks y tracking de memoria.
- [ ] Obtener logical CPU count considerando processor groups, con fallback
  seguro. El valor sirve como sugerencia; no fija de forma rígida el pool.
- [ ] Compilar tests de contrato comunes contra ambos backends. Sólo las pruebas
  de detalles nativos deben ser específicas por plataforma.
- [ ] Auditar por separado el `File::exists` de MSVC y el primer resize después de
  conocer el extent real de la superficie. Aplicar fixes sólo si NK reproduce la
  brecha.
- [ ] No alterar el backend Wayland salvo por consumir la interfaz común.

### Validación y commits

- [ ] Build y tests Windows Debug/Release, además de Linux para evitar que los
  headers Win32 contaminen interfaces públicas.
- [ ] Documentar como pendiente cualquier smoke runtime Windows que no pueda
  ejecutarse desde este equipo; una compilación cruzada no equivale a validarlo.
- [ ] Commit sugerido:
  `feat(platform): implement Windows threading parity`.
- [ ] Si aparecen brechas independientes:
  `fix(platform): preserve filesystem and initial extent contracts`.

---

## Capítulo 67 — Job System

- Vídeo: [Kohi #067: Job System](https://youtu.be/3JbLqoDubIY?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj)
- Referencia principal: [`56d2599`](https://github.com/travisvroman/kohi/commit/56d2599ea4a70718c7409372dbd845406f6850a9)
- Fix obligatorio: [`97b4b5b`](https://github.com/travisvroman/kohi/commit/97b4b5bb02fafcc852fcaea9c73e771ed5e82542)

### Alcance histórico

Kohi añade una ring queue, prioridades, máscaras de capacidades por worker,
payloads de job y callbacks de resultado. El fix `97b4b5b` corrige liberar el
resultado con el tamaño del parámetro. La versión histórica además hace polling,
consulta colas fuera del lock y puede perder resultados al saturarse.

### Diseño NK

- [ ] Añadir `nk::cl::ring<T>` como FIFO de capacidad fija, allocator-aware y
  apta para tipos move-only. El nombre sigue el patrón corto de `arr`, `dyarr`,
  `map` y `slice` sin forzar el sufijo `arr` a todos los contenedores.
- [ ] Definir semántica de full/empty, wrap-around, construcción/destrucción y
  rollback mediante `result`; ninguna operación fallida cambia head/tail/count.
- [ ] Crear `JobSystem` C++ con colas acotadas, prioridades high/normal/low y
  starvation control. Las capability masks se mantendrán sólo si hay trabajos
  que realmente necesiten afinidad.
- [ ] Representar cada envío con un handle generacional y estados explícitos.
  Copiar/mover el input al storage owned por el job; nunca guardar referencias a
  stack del llamador.
- [ ] Usar type erasure interna de tamaño conocido o payload inline pequeño con
  fallback a allocator dedicado. No exponer una API pública de punteros `void*`
  y tamaños susceptibles al bug de `97b4b5b`.
- [ ] Dormir workers con `ConditionVariable`; toda lectura/escritura de colas y
  estado de shutdown ocurre bajo el contrato de sincronización correspondiente.
- [ ] Ejecutar completions en el hilo principal. Una completion queue llena
  propaga backpressure o error; nunca descarta silenciosamente una publicación.
- [ ] Definir cancel, drain y shutdown: detener submissions, solicitar stop,
  despertar workers, resolver/cancelar jobs owned y hacer join.
- [ ] Usar allocator sincronizado dedicado para payloads/completions y scratch
  privado por worker. No compartir `FreeListAllocator` sin wrapper.
- [ ] Con cero workers, ejecutar por una ruta síncrona determinista útil para
  tests y equipos limitados.
- [ ] No marcar el renderer como multithreaded: ningún worker llamará Vulkan en
  este capítulo.

### Validación y commits

- [ ] Tests de `ring`: wrap, full, empty, move-only, destructor, OOM y secuencias
  aleatorias contra un modelo de referencia.
- [ ] Tests de jobs: prioridades, starvation, múltiples productores, completion
  en main, cancel, queue full, fallo del trabajo, OOM y shutdown con cola activa.
- [ ] Regresión explícita que compruebe lifetime/tamaño distintos para input y
  output, cubriendo `97b4b5b` sin depender de free-by-size inseguro.
- [ ] TSAN opcional y stress repetible con seed registrada.
- [ ] Commits sugeridos:
  `feat(collections): add fixed-capacity ring storage` y
  `feat(engine): add cooperative job scheduling`.

---

## Capítulo 68 — Jobifying Texture and Mesh Loading

- Vídeo: [Kohi #068: Jobifying Texture and Mesh Loading](https://youtu.be/dRxw-C-eBGU?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj)
- Referencia: [`d8936d2`](https://github.com/travisvroman/kohi/commit/d8936d24c305997c8e13f52ddb9ca13cd8775956)

### Alcance histórico

Kohi traslada lectura/decode/import de imágenes y meshes a workers y realiza
creación GPU/publicación en el callback principal. El parche histórico deja
riesgos de nombres prestados, generación modificada desde worker y cleanup
incompleto que NK no debe conservar.

### Diseño NK

- [ ] Separar cada carga en tres etapas: request inmutable, trabajo CPU owned por
  el worker y completion/publicación validada en main.
- [ ] Copiar en el request el nombre, configuración y token generacional. El
  worker no retiene `strview`, punteros a slots ni referencias a temporales.
- [ ] Modelar recursos como `Unloaded`, `Queued`, `Loading`, `Ready`, `Failed` y
  `Cancelling`. Sólo main cambia el slot publicado y su generación visible.
- [ ] Hacer que el worker produzca datos CPU completos y owned. La completion
  valida que handle/generation siga vigente, crea textura/geometría GPU y publica
  `Ready` únicamente tras éxito total.
- [ ] Agrupar adquisiciones simultáneas del mismo nombre. Release durante carga
  cancela la publicación o descarta el resultado de forma segura cuando termina.
- [ ] No acceder desde workers a maps/registries mutables del `ResourceSystem`.
  Darles loaders aislados, base path inmutable y allocators explícitos.
- [ ] Auditar `libspng`: mantener contexto/allocator de decoder por ejecución, sin
  compartir estado no sincronizado. Tinyobj debe producir buffers locales y el
  cache `.nkmesh` debe conservar escrituras atómicas con nombres temporales
  únicos.
- [ ] Mantener decode/import en CPU workers y uploads Vulkan en main/render. Una
  futura transfer queue asíncrona será otro feature con timeline y ownership de
  colas explícito.
- [ ] Dibujar placeholder o ignorar meshes no `Ready`, sin leer geometría a medio
  construir.
- [ ] Orden de shutdown: cerrar submissions, cancelar/drenar, consumir o descartar
  completions, unir workers y sólo entonces destruir sistemas/renderer.

### Validación y commits

- [ ] Usar executor falso/determinista en unit tests para no depender de sleeps.
- [ ] Casos: éxito, decode/import fallido, upload fallido, duplicado, release
  antes de completion, recarga, stale generation, OOM y shutdown con trabajos.
- [ ] Comparar bytes de imagen y digest de mesh entre rutas síncrona/asíncrona.
- [ ] Smoke Sponza/Falcon: la aplicación abre de inmediato, mantiene input y los
  modelos aparecen al publicarse; no debe haber hitch largo en el primer frame.
- [ ] Medir tiempo CPU de carga, frame-time y allocations antes/después.
- [ ] Commits sugeridos:
  `feat(resources): load texture and mesh payloads asynchronously` y
  `fix(resources): make in-flight publication cancellation-safe`.

---

## Capítulo 69 — Bug Fix Pass

- Vídeo: [Kohi #069: Bug Fix Pass](https://youtu.be/x8ao57nbMlY?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj)
- Referencia funcional: [`c8fd2cd`](https://github.com/travisvroman/kohi/commit/c8fd2cdbf986038f21f89392642b76807278bb12)
- Fix #80 final: [`8cca36a`](https://github.com/travisvroman/kohi/commit/8cca36a656eeb638f3938c2a1e17e50c2ad5e161)
- Variantes/procedencia #80:
  [`b0f0ddb`](https://github.com/travisvroman/kohi/commit/b0f0ddb5455a376fd833c9a0772cfc9a8dfe526d),
  [`f326dc3`](https://github.com/travisvroman/kohi/commit/f326dc356c7c11f32ea436486fcfdf3161161e53) y
  [`1669a4e`](https://github.com/travisvroman/kohi/commit/1669a4eed609d8be41092b9d269b542c67e15227)
- Cambio documental: [`9c3dcd1`](https://github.com/travisvroman/kohi/commit/9c3dcd1c7cb0973c7d9f5e84e787f7c684266e8e)

### Matriz de evaluación, no cherry-pick literal

| Cambio Kohi | Decisión en NK |
|---|---|
| Pausar 16 ms al omitir frame/recrear swapchain | Conservar la intención de no hacer busy-loop, pero usar espera de eventos o backoff sólo al estar suspendido/extent cero; no insertar una latencia fija en frames normales. |
| Tangent pasa de `vec4` a `vec3` | No aplicar. NK usa `vec4.w` para handedness y reconstruye bitangent, incluyendo escala negativa. Añadir test/layout assertion para no regresarlo. |
| Identidad de listener de evento | Corregir registro duplicado usando el par `(listener, callback)`, no sólo listener. |
| Mouse buttons como bool y wheel signed | Auditar; NK ya representa esos casos correctamente. Mantener tests. |
| Reloj monotónico Linux | NK usa `std::chrono::steady_clock`; mantener la abstracción común mientras sea monotónica. |
| Liberar arrays temporales de device/validation extensions | Auditar todos los exits y añadir failure injection. Aplicar cualquier cleanup faltante. |
| `memset` con tamaño de puntero en command buffer | NK usa value initialization; añadir regresión/inspección, no copiar el patrón C. |
| Requisitos físicos recreados por dispositivo | Verificar que configuración y extension storage tengan un owner único y cleanup completo al fallar cada candidato. |
| Argumento de graphics queue no usado al presentar | Mantener la firma mínima actual; eliminar parámetros muertos si existe alguno. |
| Lifetime de `VkDescriptorBufferInfo` | Garantizar que las estructuras sobrevivan hasta `vkUpdateDescriptorSets`; cubrirlo con validation. |
| Meshes binarios regenerados por cambio tangent | No importar: el formato NK ya conserva `vec4` y su propio versionado/cache. |
| Typo de comentario en `event.h` | Aplicar sólo si existe el texto equivalente; no requiere commit aislado. |

### Validación y commits

- [ ] Tests de listeners que comparten owner con callbacks distintos y rechazo de
  duplicados exactos.
- [ ] Tests de wheel negativo, tangent handedness, escala negativa y cache mesh.
- [ ] Failure injection en enumeración/selección de device, command buffers y
  descriptores; MemorySystem debe regresar al baseline.
- [ ] Resize/out-of-date/extent cero sin spin y sin penalizar frames renderizados.
- [ ] Validation Layers en startup, minimize/restore, resize repetido y shutdown.
- [ ] Dividir fixes no relacionados, por ejemplo
  `fix(events): distinguish callbacks owned by one listener` y
  `fix(vulkan): preserve temporary resource lifetimes`.

Este capítulo es una puerta de regresión: debe cerrarse antes de cambiar memoria
y buffers, para no atribuir bugs anteriores a esos refactors.

---

## Capítulo 70 — Memory Alignment and Allocator Updates

- Vídeo: [Kohi #070: Memory Alignment, Allocator Updates](https://youtu.be/Fo9ri9DMu74?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj)
- Referencia: [`e5e044e`](https://github.com/travisvroman/kohi/commit/e5e044ebb4fd4b1e595cf50822f63cf81215c356)
- Merge de procedencia: [`e21b105`](https://github.com/travisvroman/kohi/commit/e21b105bf339cc2a8f58cb1ac8095f5a1c0648ec)

### Alcance histórico y diferencia con NK

Kohi introduce asignaciones alineadas, headers con tamaño/alineación, estadísticas
y cambios a la free list/dynamic allocator. NK ya posee validación power-of-two,
alineación tipada, `posix_memalign`/`_aligned_malloc`, `FreeList::reserve` con
alignment/bias, `FreeListAllocator` y tests hasta alineamientos grandes. Copiar el
allocator de este commit perdería garantías actuales y limitaría metadata de
tamaño.

### Diseño NK

- [ ] Preparar una tabla de equivalencia de cada cambio upstream contra
  `Allocator`, `MallocAllocator`, `FreeList`, `FreeListAllocator` y
  `MemorySystem`; implementar únicamente brechas demostrables.
- [ ] Documentar formalmente tres niveles: allocator no thread-safe por defecto,
  allocator de uso exclusivo por worker y `SynchronizedAllocator` para recursos
  realmente compartidos.
- [ ] Añadir `SynchronizedAllocator` como wrapper, no un mutex dentro de cada
  implementación. Debe delegar allocation/free/reset y preservar source
  location, tipo, owner y tracking.
- [ ] Serializar el estado compartido de `MemorySystem`/allocation tracker sin
  sostener su lock al formatear logs, invocar código que asigne o entrar a Vulkan.
  Mantener un guard thread-local de reentrancia y fijar orden de locks.
- [ ] Revisar early-allocation replay frente a workers: al iniciar jobs,
  MemorySystem ya debe estar `Running`; cualquier evento tardío desde otro thread
  entra por la ruta sincronizada normal.
- [ ] Mantener aritmética checked en `align_up`, header+padding y range end. La
  alineación debe calcularse con la dirección real del backing store, no sólo con
  offsets relativos.
- [ ] No exigir `free(pointer, size)` cuando el owner ya conoce metadata; tampoco
  limitar silenciosamente allocation size a 32 bits.
- [ ] Exponer estadísticas total/used/free/peak con snapshots coherentes y sin
  construir grandes strings dentro del lock.

### Validación y criterio de rendimiento

- [ ] Tests de alignment desde 1 hasta 4096, backing intencionalmente desalineado,
  fragmentación/coalescing, overflow, OOM y metadata que representa tamaños
  mayores a 4 GiB sin reservarlos físicamente.
- [ ] Stress multihilo con allocator sincronizado y tracking; TSAN cuando sea
  viable y contador final exactamente en baseline.
- [ ] Benchmark A/B single-thread y contended. El wrapper sólo se usará en
  allocators compartidos; si degrada una ruta sin concurrencia, se revierte esa
  adopción y se mantiene ownership por thread.
- [ ] Commits sugeridos:
  `feat(memory): add explicitly synchronized allocator access` y
  `fix(memory): serialize tracked worker allocation events`.

---

## Capítulo 70.1 — Alignment Fixes Patch

- Vídeo: [Kohi #070.1: Alignment Fixes Patch](https://youtu.be/VQ6Q9B_WDlM?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj)
- Referencia: [`c732a24`](https://github.com/travisvroman/kohi/commit/c732a24fcb7f630ad1cca3bd6331468ac1c5ad86)

### Alcance

Este parche sustituye parte del enfoque alineado del capítulo 70 por metadata de
padding/header. Es autoritativo respecto al commit anterior de Kohi, pero no lo
es respecto a los invariantes ya mejores de NK. El parche contiene expresiones
de offset que no deben copiarse sin demostrar su corrección sobre una base
desalineada.

### Plan de hardening

- [ ] Convertir todos los casos corregidos por Kohi en tests black-box sobre los
  allocators NK antes de cambiar implementación.
- [ ] Verificar `base + offset + padding`, ubicación del header, rango realmente
  reservado y puntero devuelto. La misma base no puede sumarse dos veces.
- [ ] Validar round-trip allocate/free con alignments mayores que header y con
  tamaños pequeños, exactamente alineados y cercanos a overflow.
- [ ] Comprobar que un free inválido, double free o header corrupto falla de forma
  detectable en debug sin mutar la free list.
- [ ] Si todos los casos ya pasan, cerrar el punto con evidencia documental y sin
  un commit vacío. Si aparece una brecha, usar
  `fix(memory): harden aligned allocation metadata`.

---

## Capítulo 71 — Custom Vulkan Allocator

- Vídeo: [Kohi #071: Custom Vulkan Allocator](https://youtu.be/F9hN1PrxnHE?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj)
- Referencia principal: [`5755dc0`](https://github.com/travisvroman/kohi/commit/5755dc0e23877be5643874bdf398359a9bce220c)
- Merge de procedencia: [`775c646`](https://github.com/travisvroman/kohi/commit/775c64633bdf460217b862815ab5f48a6d0e445e)

### Decisión de arquitectura

`VkAllocationCallbacks` controla asignaciones CPU internas del driver. No es un
allocator de `VkDeviceMemory` ni, según la especificación Vulkan, una mejora de
rendimiento general. NoGraphicsAPI deja esos callbacks nulos y obtiene rendimiento
mediante heaps/rangos GPU y lifetimes explícitos. Por ello NK implementará los
callbacks sólo como herramienta opcional de diagnóstico/tracking, y mantendrá la
memoria GPU como subsistema separado.

### Diseño NK

- [ ] Crear `VulkanHostAllocator` cuyo estado y callbacks viven desde antes de
  `vkCreateInstance` hasta después de destruir el último objeto creado con ellos.
- [ ] Pasar el mismo puntero compatible a cada create/destroy. Mantener una ruta
  `nullptr` seleccionable, usada por defecto en Release salvo que mediciones
  justifiquen otra política.
- [ ] Implementar allocate/reallocate/free con alineación exacta y metadata de
  tamaño original. `reallocate` preserva `min(oldSize, newSize)`, mantiene el
  bloque anterior al fallar y trata tamaño cero como free.
- [ ] No copiar el overread potencial de Kohi al copiar `newSize` bytes desde un
  bloque menor.
- [ ] Hacer callbacks thread-safe: Vulkan puede invocarlos desde el mismo thread
  que provocó la llamada API, y distintas llamadas de aplicación pueden ocurrir
  simultáneamente. El callback no invoca Vulkan, logging con asignaciones ni
  ningún camino reentrante.
- [ ] Etiquetar scope/tipo de allocation sin asumir que todos los calls de driver
  poseen lifetime equivalente. Fallar con `nullptr` en OOM según contrato.
- [ ] Contabilizar `VkDeviceMemory` por separado y sólo después de un
  `vkAllocateMemory` exitoso; decrementar alrededor de `vkFreeMemory` con owner y
  tamaño conocidos. Distinguir heap device-local y host-visible.
- [ ] Exponer diagnóstico callbacks on/off para comparar estabilidad y coste. No
  venderlo como sustituto de subasignación GPU.

### Validación y commits

- [ ] Unit tests directos de callbacks: alignment, grow, shrink, realign, size
  cero, OOM, original intacto al fallar y concurrencia.
- [ ] Smoke Vulkan callbacks on/off con rutas modernas y fallback, validation
  layers y MemorySystem de vuelta al baseline tras shutdown.
- [ ] Inyección de fallo de `vkAllocateMemory`: no debe incrementar tracking ni
  publicar el recurso.
- [ ] Commits sugeridos:
  `feat(vulkan): add optional tracked host allocation callbacks` y
  `fix(vulkan): account device memory only after successful allocation`.

---

## Capítulo 72 — Generic Renderbuffers

- Vídeo: [Kohi #072: Generic Renderbuffers](https://youtu.be/7mNgC7dGkZs?list=PLv8Ddw9K0JPg1BEO-RS-0MYs423cvLVtj)
- Referencia: [`5ea625c`](https://github.com/travisvroman/kohi/commit/5ea625c2de2a48bedb52cdb9e77b4b23954cde24)

### Alcance histórico

Kohi crea una fachada genérica de renderbuffer y migra vertex, index, uniform y
staging buffers. NK ya posee `vk::Buffer`, `BufferSuballocator` y `vk::BufferView`,
por lo que el objetivo es completar la frontera renderer/backend y mejorar el
flujo de rangos, no duplicar clases.

### Diseño renderer-neutral

- [ ] Definir `RenderBuffer` o `RenderBufferHandle` move-only fuera del namespace
  Vulkan. No exponer `VkBuffer`, `VkDeviceMemory`, flags o punteros mapeados sin
  scope en headers públicos.
- [ ] Separar `BufferUsage` (`Vertex`, `Index`, `Uniform`, `Storage`,
  `TransferSource`, `TransferDestination`) de `MemoryUsage` (`DeviceLocal`,
  `Upload`, `Readback`). No usar un enum que mezcle intención y ubicación.
- [ ] Reutilizar `BufferSuballocator` para buffers que administren rangos. Una
  vista prestada contiene handle/generation, offset y size; cada operación valida
  owner, bounds, alignment y stale generation.
- [ ] Hacer que create, reserve, release, map, upload, copy y resize devuelvan
  `result`. Destrucción sigue siendo idempotente y no falla silenciosamente si el
  recurso aún está en vuelo.
- [ ] Ofrecer `MappedBufferRange` con scope/RAII o un par map/unmap de estado
  comprobado. Flush/invalidate debe alinear rangos a `nonCoherentAtomSize`.
- [ ] Mantener persistent mapping en upload/uniform cuando la memoria lo permita,
  evitando map/unmap y allocations por frame.
- [ ] Combinar restricciones de `vkGetBufferMemoryRequirements`,
  `minUniformBufferOffsetAlignment`, storage alignment, copy offsets y dirección
  real al reservar rangos.

### Adaptación de NoGraphicsAPI y optimización Vulkan

- [ ] Adoptar el principio de pocos bloques `VkDeviceMemory` y backing buffers
  grandes, con subasignación controlada por la aplicación para los usos
  existentes. Esto reduce presión sobre `maxMemoryAllocationCount` y llamadas al
  driver. No exigir buffer device address, descriptor heaps ni GPUs recientes
  sólo para completar este capítulo.
- [ ] Expresar comandos en términos de rangos prestados, con lifetime visible al
  submission. Preparar retiro diferido por fence/timeline para que un recurso no
  se reutilice mientras la GPU lo referencia.
- [ ] Mantener sync2/dynamic rendering cuando las capacidades están disponibles y
  traducir los mismos contratos a barriers/render pass legados en el fallback.
- [ ] Reemplazar `vkQueueWaitIdle`/`vkDeviceWaitIdle` de uploads y resize en
  steady-state por fences/timeline asociados al submission. Las esperas globales
  quedan para shutdown o recuperación excepcional.
- [ ] Batch de copias de staging por frame/lote, reduciendo command buffers,
  submissions y transiciones por recurso. Medir antes/después.
- [ ] Resize transaccional: crear nuevo backing, reservar/copy, publicar y retirar
  el anterior sólo tras completar GPU. Ante cualquier fallo, conservar handle,
  bytes, suballocator y vista originales.
- [ ] Actualizar metadata de `BufferSuballocator` sólo después del éxito backend o
  revertirla exactamente. Nunca publicar un rango que no tenga almacenamiento.

### Migración y validación

- [ ] Migrar por orden: staging/upload, global vertex, global index, uniform y
  cualquier readback. Tras cada paso eliminar sólo la exposición Vulkan que ya no
  tenga consumidores.
- [ ] Tests con backend falso: lifecycle, flags incompatibles, invalid ranges,
  double map, stale view, resize rollback, suballocation fragmentada y OOM.
- [ ] Validation Layers en startup, uploads, resize, swapchain recreation y
  shutdown para las rutas moderna y fallback.
- [ ] Benchmarks: submissions por carga, waits globales, bytes subasignados,
  fragmentación, CPU frame-time y allocations por frame.
- [ ] Smoke Wayland/niri con UI, skybox, Falcon y Sponza; comparar FPS y frametime
  contra el baseline del capítulo 59.
- [ ] Commits sugeridos:
  `refactor(renderer): expose backend-neutral render buffers`,
  `perf(vulkan): batch buffer uploads with explicit range lifetimes` y
  `fix(renderer): preserve buffer state across failed resizes`.

---

## Política transversal de rendimiento y regresión

### Métricas mínimas

Registrar antes del capítulo 60 y repetir en 62, 68 y 72:

- frame-time CPU promedio, p95 y p99 en Sponza;
- FPS sólo como dato secundario;
- allocations y bytes CPU por frame estable;
- tiempo de decode/import y tiempo hasta recurso `Ready`;
- número de submissions, command buffers temporales y waits globales;
- memoria GPU reservada/usada y fragmentación de buffers;
- tiempo de shutdown con jobs activos.

El benchmark debe fijar escena, resolución, cámara, build type, GPU/driver y ruta
Vulkan moderna/fallback. Una ganancia no se aceptará si rompe compatibilidad del
equipo actual o elimina la ruta segura para Windows/NVIDIA.

### Regla de rollback

- Cada optimización debe poder desactivarse detrás de una política o conservar el
  commit anterior como unidad revertible.
- Si una optimización empeora p95/p99 más allá del ruido medido, aumenta
  submissions/esperas o introduce errores de validation, se revierte antes de
  continuar.
- No se sacrifica corrección de lifetime por FPS. Primero se conserva el estado
  anterior ante fallo; después se optimiza el happy path.
- Los wrappers sincronizados se aplican sólo a allocators compartidos. Si la
  contención domina, se migra a ownership/scratch por worker en vez de sumar más
  locks.

## Matriz de verificación por plataforma

| Verificación | Linux Wayland/niri | Linux fallback | Windows 11/NVIDIA |
|---|---:|---:|---:|
| Build Debug/Release | Obligatoria | Obligatoria si está disponible | Obligatoria |
| Unit/integration tests | Obligatoria | Comparte suite | Obligatoria |
| Thread/job stress | Obligatoria | Comparte backend Linux | Obligatoria |
| TSAN | Cuando toolchain/driver lo permita | Igual | No requerido inicialmente |
| Vulkan Validation Layers | Obligatoria | Obligatoria | Obligatoria |
| Dynamic rendering + sync2 | Si el dispositivo lo ofrece | Si lo ofrece | Si lo ofrece |
| Render pass/barriers fallback | Obligatoria mediante override/test | Obligatoria | Obligatoria mediante override/test |
| Smoke UI/skybox/Falcon/Sponza | Obligatoria | Secundaria | Obligatoria |

## Dependencias y assets

### Dependencias previstas

No se necesita una biblioteca nueva para implementar cámaras, vistas,
transparencia, threading, jobs, allocators o buffers. Se reutilizan:

- GLM `1.0.1` para matemáticas;
- rapidhash `rapidhash_v3` mediante `map` propio;
- libspng `v0.7.4` y zlib `v1.3.2` para PNG;
- tinyobjloader `v2.0.0rc13` para OBJ;
- Vulkan SDK y Slang provistos por el toolchain/Nix.

Antes de aceptar cualquier dependencia inesperada:

- [ ] documentar el hueco funcional y alternativas evaluadas;
- [ ] escoger un release/tag estable, nunca una rama flotante si existe tag;
- [ ] añadir submódulo apuntando exactamente al tag;
- [ ] integrar build offline/reproducible;
- [ ] añadir nombre, versión, URL y proyecto a `.scripts/libraries.csv`;
- [ ] registrar licencia y atribución;
- [ ] cerrar todo en un commit semántico propio.

### Assets del skybox

- [ ] Obtener exactamente las seis imágenes del commit `ab99e89` y registrar sus
  hashes originales.
- [ ] Conservar autor/licencia de Emil Persson/Humus indicada por Kohi.
- [ ] Versionar las caras PNG lossless listas para el loader actual y documentar
  el mapping de nombres a ejes.
- [ ] Verificar que el pipeline de build copie las seis y no repita/omita una,
  absorbiendo la intención de `0b243c5`.

## Criterio de cierre de cada capítulo

Un capítulo queda completado sólo cuando:

1. los commits principal, fixes y merges asociados fueron evaluados;
2. la API respeta ownership, allocators y `result` de NK;
3. tests nuevos y suite completa pasan;
4. `git diff --check` queda limpio;
5. si toca Vulkan, Validation Layers y fallback correspondiente fueron probados;
6. si toca render visual, existe smoke Wayland/niri y se registra cualquier
   validación Windows pendiente;
7. las métricas relevantes no muestran regresión no explicada;
8. documentación/CSV/licencias se actualizaron cuando corresponde;
9. el cambio importante termina en un commit semántico sin número de capítulo.

## Inventario completo de commits proporcionados

| Commit | Clasificación | Destino del plan |
|---|---|---|
| [`f43ae30`](https://github.com/travisvroman/kohi/commit/f43ae30483398f0d843383108580b19f1075d21a) | implementación principal | 60 |
| [`6c2c2d9`](https://github.com/travisvroman/kohi/commit/6c2c2d9cb719210a534c7fb6bcdf61ffcf78e7e6) | implementación principal | 61 |
| [`80a30b0`](https://github.com/travisvroman/kohi/commit/80a30b0b8da8334dd9912fbb69ca424810957e6e) | implementación principal | 62 |
| [`ab99e89`](https://github.com/travisvroman/kohi/commit/ab99e894b24a18598412c42c1545c892042e3dfa) | implementación principal | 63 |
| [`a6280c6`](https://github.com/travisvroman/kohi/commit/a6280c6dda14c5d32a21926d7d95162877aa84af) | fix de bounds | 62 |
| [`1d500f6`](https://github.com/travisvroman/kohi/commit/1d500f65769276a00fc42117314ee5f554146371) | hardening de macro clamp | 63, auditoría sin copiar macro |
| [`df2904e`](https://github.com/travisvroman/kohi/commit/df2904ec8abb184fd2916d2c4ad20c44f8968e65) | implementación principal | 64 |
| [`0b243c5`](https://github.com/travisvroman/kohi/commit/0b243c52a0aabbd7885424b2c5a65151235950b3) | fix del script skybox | 64 |
| [`76b9734`](https://github.com/travisvroman/kohi/commit/76b9734e6cbe360c1fd3b186b97b816b29ade57b) | implementación Linux | 65 |
| [`fc0f503`](https://github.com/travisvroman/kohi/commit/fc0f503a8917382a643331692c78a911276a22aa) | implementación Win32 y fixes laterales | 66 |
| [`56d2599`](https://github.com/travisvroman/kohi/commit/56d2599ea4a70718c7409372dbd845406f6850a9) | implementación principal | 67 |
| [`97b4b5b`](https://github.com/travisvroman/kohi/commit/97b4b5bb02fafcc852fcaea9c73e771ed5e82542) | fix tamaño de result | 67 |
| [`d8936d2`](https://github.com/travisvroman/kohi/commit/d8936d24c305997c8e13f52ddb9ca13cd8775956) | implementación principal | 68 |
| [`b0f0ddb`](https://github.com/travisvroman/kohi/commit/b0f0ddb5455a376fd833c9a0772cfc9a8dfe526d) | variante inicial fix #80 | 69, procedencia |
| [`f326dc3`](https://github.com/travisvroman/kohi/commit/f326dc356c7c11f32ea436486fcfdf3161161e53) | merge/variante fix #80 | 69, procedencia |
| [`8cca36a`](https://github.com/travisvroman/kohi/commit/8cca36a656eeb638f3938c2a1e17e50c2ad5e161) | squash/fix #80 autoritativo | 69 |
| [`c8fd2cd`](https://github.com/travisvroman/kohi/commit/c8fd2cdbf986038f21f89392642b76807278bb12) | bug-fix pass principal | 69 |
| [`9c3dcd1`](https://github.com/travisvroman/kohi/commit/9c3dcd1c7cb0973c7d9f5e84e787f7c684266e8e) | typo documental | 69, sólo si aplica |
| [`1669a4e`](https://github.com/travisvroman/kohi/commit/1669a4eed609d8be41092b9d269b542c67e15227) | merge de procedencia #80 | 69, no duplicar |
| [`e5e044e`](https://github.com/travisvroman/kohi/commit/e5e044ebb4fd4b1e595cf50822f63cf81215c356) | implementación principal | 70 |
| [`e21b105`](https://github.com/travisvroman/kohi/commit/e21b105bf339cc2a8f58cb1ac8095f5a1c0648ec) | merge de integración | 70, no duplicar |
| [`c732a24`](https://github.com/travisvroman/kohi/commit/c732a24fcb7f630ad1cca3bd6331468ac1c5ad86) | parche correctivo autoritativo | 70.1 |
| [`5755dc0`](https://github.com/travisvroman/kohi/commit/5755dc0e23877be5643874bdf398359a9bce220c) | implementación principal | 71 |
| [`775c646`](https://github.com/travisvroman/kohi/commit/775c64633bdf460217b862815ab5f48a6d0e445e) | merge de integración | 71, no duplicar |
| [`5ea625c`](https://github.com/travisvroman/kohi/commit/5ea625c2de2a48bedb52cdb9e77b4b23954cde24) | implementación principal | 72 |

Todos los commits proporcionados quedan asignados. Ninguno se ignora: los que no
producen código se conservan explícitamente como fixes ya absorbidos, auditorías
o merges de procedencia.

## Orden recomendado de ejecución y segmentación de commits

1. Camera value/system y migración de cámara activa.
2. Render views y migración del frame packet.
3. Transparencia/bounds.
4. Cubemap, skybox Slang y assets con licencia.
5. Toolchain Slang común.
6. Threading Linux.
7. Paridad threading Windows.
8. `cl::ring`.
9. JobSystem.
10. Carga asíncrona y publicación segura.
11. Puerta de bug fixes.
12. Contrato multihilo de allocators/tracking.
13. Hardening 70.1 basado en tests.
14. Callbacks host Vulkan opcionales y contabilidad device memory.
15. RenderBuffer renderer-neutral.
16. Batch de uploads, retiro diferido y migración de buffers.

No se combinarán estos puntos en un único commit masivo. Los commits de tests
pueden acompañar el comportamiento que protegen; fixes descubiertos en una capa
anterior se separarán del feature que los reveló.
