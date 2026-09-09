# Convenciones de coordenadas e iluminación

Este documento fija el contrato matemático del renderer a partir de la
iluminación direccional. Los shaders y generadores de geometría deben conservar
estas reglas; cualquier cambio futuro requiere actualizar las regresiones
matemáticas y de layout.

## Espacios y handedness

- El espacio world es right-handed, que es el comportamiento por defecto de
  GLM en este proyecto. La cámara mira hacia `-Z`.
- Los triángulos front-facing usan winding counter-clockwise. Las normales se
  generan con `cross(b - a, c - a)` y apuntan hacia el exterior de la malla.
- Vulkan mantiene `VK_FRONT_FACE_COUNTER_CLOCKWISE`; el viewport de altura
  negativa adapta el eje vertical al framebuffer.
- `Vertex3D` almacena `position`, `normal`, `texcoord` y `tangent`, con offsets
  `0`, `12`, `24` y `32`, respectivamente, y stride total de 48 bytes. La parte
  xyz de la tangente vive en object space y `w` conserva el handedness
  bitangent (`+1` o `-1`).
- Las tangentes se acumulan por vértice, se ortogonalizan contra la normal con
  Gram-Schmidt y usan un eje fallback determinista cuando la parametrización UV
  es degenerada. Topología inválida u OOM no dejan el buffer parcialmente
  modificado.

## Matrices y ángulos

- Se usan vectores columna y el orden `projection * view * model * position`.
  En Slang esto se expresa con `mul(matrix, vector)`.
- Las normales viven inicialmente en object space. Por cada draw, CPU calcula
  `transpose(inverse(mat3(model)))`, la expande a `mat4` y la envía como push
  constant. Esto evita calcular una inversa por vértice y conserva
  perpendicularidad bajo escalas no uniformes.
- Una transformación singular o no finita produce una normal nula. El shader
  mantiene entonces sólo la contribución ambiental, sin propagar NaN.
- Toda API de rotación recibe radianes. Los valores expresados en grados deben
  convertirse explícitamente con `glm::radians`.

## Luz direccional y color

- `DirectionalLight::direction` indica la dirección en que viajan los rayos de
  luz, en world space. El vector desde la superficie hacia la luz es
  `-direction`.
- Dirección y normal se normalizan de forma segura. Una dirección nula o no
  finita desactiva la contribución diffuse.
- El término diffuse es `max(dot(normal, -direction), 0)`.
- `ambient_color.rgb` y `DirectionalLight::color.rgb` modulan el RGB resultante
  de `material.diffuse_color * texture_sample`.
- El alfa final continúa siendo
  `material.diffuse_color.a * texture_sample.a`. El alfa de ambient y de la luz
  queda reservado y no altera transparencia.
- Los valores por defecto son ambient `(0.25, 0.25, 0.25)`, dirección
  normalizada `(-1, -1, -1)` y color direccional `(0.8, 0.8, 0.8)`.

## Reflexión especular

- El modelo world usa Blinn-Phong. Si `D` es la dirección normalizada en que
  viajan los rayos, entonces el vector hacia la luz es `L = -D`, el vector
  hacia la cámara es `V = normalize(view_position - world_position)` y el
  half-vector es `H = normalize(V + L)`, equivalente a `normalize(V - D)`.
- La contribución especular sólo se evalúa cuando `dot(N, L) > 0`. Su RGB es
  `light.rgb * specular_map.rgb * pow(max(dot(N, H), 0), max(shininess, 1))`.
- El reflejo especular no se tiñe con el color diffuse. Tampoco modifica el
  alfa, que continúa perteneciendo exclusivamente al color y mapa diffuse.
- `shininess` debe ser finito y mayor que cero; el valor por defecto es `32`.
  Un material que omite `specular_map_name` recibe la textura specular negra de
  1×1, por lo que conserva el resultado ambient + diffuse sin un branch ni una
  variante de shader.
- Declarar un mapa inexistente es un error de carga tipado. La adquisición de
  los mapas diffuse/specular/normal y el cambio conjunto usado por la tecla `T`
  son transaccionales: el material y sus referencias anteriores no se modifican
  si falla cualquiera de las tres adquisiciones.

## Normal mapping

- Un normal map se interpreta en tangent space como `rgb * 2 - 1`. El default
  opaco de 1×1 es `(128, 128, 255, 255)`, equivalente de forma discreta a una
  normal plana orientada hacia `+Z`, y no tiene ownership por material.
- El shader vuelve a ortogonalizar la tangente interpolada contra la normal
  geométrica. La bitangente se obtiene con `cross(N, T) * tangent.w` y el TBN
  transforma la muestra al mismo world space usado por la iluminación.
- La normal usa inverse-transpose y la tangente la parte lineal de `model`. El
  signo de la tangente también incorpora la orientación de `model`, por lo que
  una transformación reflejada no invierte incorrectamente el normal map.
- Normal, tangente, bitangente y resultado TBN se normalizan de forma segura;
  entradas degeneradas o no finitas no propagan NaN.

## Contrato CPU–GPU

- El UBO global world usa offsets: projection `0`, view `64`, ambient `128`,
  dirección `144`, color direccional `160` y posición de cámara `176`; su tamaño
  lógico es 188 bytes.
- El UBO instance world coloca `diffuse_color` en `0` y `shininess` en `16`;
  el descriptor reserva 64 bytes por instancia. El binding `1` contiene un
  array de tres samplers: diffuse en el elemento `0`, specular en el `1` y
  normal en el `2`.
- Los push constants world contienen model en `0` y normal matrix en `64`, con
  128 bytes totales.
- `Camera::set_view` recibe conjuntamente la matriz view y su posición world.
  `Renderer` las propaga al shader world; el pase UI no usa posición de cámara,
  luz ni mapas specular/normal.
- La iluminación forma parte de `RenderPacket` mediante `SceneLighting`. El
  pase UI ignora estos datos y conserva su shader y layout anteriores.
## Frustum y clip space

Las matrices de proyección actuales de las vistas se crean con GLM sin
`GLM_FORCE_DEPTH_ZERO_TO_ONE`. Por ello el culling extrae el volumen homogéneo
`-w <= x,y,z <= w` de la misma matriz `projection * view` que consume Vulkan.
La inversión vertical se resuelve en el viewport y no cambia estos planos.

Las AABB locales se llevan a mundo mediante centro y extents conservadores:
`worldCenter = model * localCenter` y
`worldExtents = abs(mat3(model)) * localExtents`. Esto preserva rotación, escala
no uniforme y escala negativa. La tangencia cuenta como visible; matrices o
bounds no finitos también se conservan visibles para fallar sin ocultar objetos.

El renderer calcula el frustum de cada vista world y filtra sólo su lista de
geometría. Skybox, UI y texto quedan fuera de este culling. Primero se preserva
el orden de envío de los objetos opacos y después se ordenan únicamente los
transparentes visibles de atrás hacia delante para la posición de esa vista.

`FrameMetrics` expone candidatos, visibles, descartados y draws finales. Para
comparar la imagen o aislar una regresión se puede desactivar temporalmente el
culling sin cambiar la build:

```bash
NK_FRUSTUM_CULLING=0 nix run .#run -- Release
```

El valor exacto `0` lo desactiva; si la variable no existe o contiene otro
valor, el culling permanece activo.
