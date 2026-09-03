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
- `Vertex3D` almacena `position`, `normal` y `texcoord`, con offsets `0`, `12` y
  `24`, respectivamente, y stride total de 32 bytes.

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
  los mapas diffuse/specular y el cambio conjunto usado por la tecla `T` son
  transaccionales: el material y sus referencias anteriores no se modifican si
  falla cualquiera de las dos adquisiciones.

## Contrato CPU–GPU

- El UBO global world usa offsets: projection `0`, view `64`, ambient `128`,
  dirección `144`, color direccional `160` y posición de cámara `176`; su tamaño
  lógico es 188 bytes.
- El UBO instance world coloca `diffuse_color` en `0` y `shininess` en `16`;
  el descriptor reserva 64 bytes por instancia. El binding `1` contiene un
  array de dos samplers: diffuse en el elemento `0` y specular en el `1`.
- Los push constants world contienen model en `0` y normal matrix en `64`, con
  128 bytes totales.
- `Camera::set_view` recibe conjuntamente la matriz view y su posición world.
  `Renderer` las propaga al shader world; el pase UI no usa posición de cámara,
  luz ni mapa specular.
- La iluminación forma parte de `RenderPacket` mediante `SceneLighting`. El
  pase UI ignora estos datos y conserva su shader y layout anteriores.
