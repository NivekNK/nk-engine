# Evidencia de errores tipados en File y shaders

> Estado: aceptada
>
> Fecha: 2026-09-02
>
> Baseline: `863f61b`

## Contratos resultantes

`File` conserva `exists` como predicado y reemplaza los `bool` ambiguos por:

| Operación | Contrato |
| --- | --- |
| `open` / `close` | `result<void, file_error>` |
| `read_line(str&)` | `result<read_line_outcome, file_error>` |
| `read(slice<u8>)` | `result<u64, file_error>` |
| `write(slice<const u8>)` | `result<u64, file_error>` |
| `read_all_bytes()` | `result<dyarr<u8>, file_error>` |

EOF de línea es `read_line_outcome::end_of_file`, no un fallo. Una lectura raw
corta también es éxito y retorna su número de bytes; sólo `ferror` produce
`read_failed`. Esto hace posible consumir streams sin confundir su final con
un error del sistema.

La carga completa devuelve un `dyarr<u8>` move-only. La medición previa mostró
que este retorno permanece dentro del ruido frente a `bool + out`, y los tests
confirman que su storage vuelve al allocator al destruir el `result`. El path
interno conserva capacidad mientras vive `File`, de acuerdo con la semántica
actual de `str`, y se libera al destruirlo.

`File` ya no es copiable ni movible, evitando duplicar un `FILE*` y cerrarlo dos
veces. El destructor ejecuta un close best-effort; el cierre explícito conserva
`close_failed` y siempre deja el objeto en estado cerrado porque un stream no
se puede reutilizar de forma segura después de fallar `fclose`.

## Límite de shaders

`create_shader_module` devuelve `result<void, shader_error>`. `shader_error`
conserva:

- la capa (`path_format_failed`, `file_failed`, `invalid_binary` o
  `module_creation_failed`);
- el `file_error` inferior cuando aplica;
- el `VkResult` nativo de `vkCreateShaderModule`.

La función inferior ya no registra el mismo error ni usa `VulkanCheck` para
convertir una creación fallida en fatal. `ObjectShader`, que sí conoce nombre y
stage, genera un único log contextual. El bytecode se valida como no vacío,
múltiplo de cuatro y alineado para `u32`; su storage se libera por RAII y el
`pCode` temporal se limpia después de la llamada Vulkan.

## Fallos cubiertos

Se añadieron seis tests Linux deterministas:

- path inexistente, modo inválido, apertura repetida y close idempotente;
- dos líneas y EOF normal;
- lectura parcial y lectura de cero bytes al alcanzar EOF;
- transferencia y liberación de ownership de `read_all_bytes`;
- OOM con allocator inyectado y seek fallido sobre un pipe real;
- escritura por slices, modo no permitido y error real usando `/dev/full`.

`close` exitoso e idempotente se prueban directamente. El branch de
`close_failed` consume el retorno de libc y queda cubierto por sanitizadores;
no se añadió un backend virtual permanente únicamente para forzar un `fclose`
fallido, ya que eso aumentaría la superficie de producción por un detalle de
test no reproducible de forma portable.

## Validación

- Debug: 97/97 tests.
- Release: 89/89 tests.
- ASan/UBSan: 97/97 tests desde una configuración CMake limpia en `/tmp`, sin
  hallazgos ni fugas.
- Editor Debug: compila y enlaza con las firmas nuevas.
- Smoke Wayland/Niri: tres frames, shaders vertex/fragment creados, shutdown
  completo y cero fugas.
- Frames estables: 0 eventos de allocation en los dos frames comprobados.

No quedan adapters `bool + out` de File ni liberación manual del buffer shader.
