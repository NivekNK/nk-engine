# Infraestructura de texto propia

> Estado: completa
>
> Fecha: 2026-09-02

## Resultado

El nombre `nk::str` ya representa un texto propietario controlado por un allocator del engine. Se añadieron `nk::strview` para vistas no propietarias, `nk::strbuf<N>` para texto fijo inline y `nk::format_to` para producir texto sin allocations dinámicas.

Los consumidores antiguos no se migraron todavía. Para evitar que el alias anterior colisionara con el nuevo tipo definitivo, `using str = std::string` pasó a llamarse temporalmente `legacy_str`. Los call sites existentes sólo recibieron ese cambio mecánico de nombre y conservan exactamente su semántica anterior. La migración por dominio de `ApplicationConfig`, renderer, archivos y temporales Vulkan sigue programada para la migración global.

## `strview`: vista, no C string

`strview` ocupa un puntero y una longitud. Puede construirse desde:

- literales y `cstr`, calculando su longitud inicial;
- `str` y `strbuf<N>`;
- un puntero junto con una longitud explícita.

La API expone `data`, no `cstr`: una vista no promete que exista un byte nulo después de su rango. En cambio, `str` y `strbuf` sí exponen `cstr()` y mantienen esa terminación como invariante. Esta diferencia permite representar subslices y bloques que no terminan en nulo sin lectura fuera de rango ni copias ocultas.

Las operaciones implementadas son comparación lexicográfica, búsqueda de carácter o texto, `starts_with`, `ends_with` y `substr`. La búsqueda usa `strview::npos` como resultado ausente y los subslices recortan su longitud al rango disponible.

## `str`: ownership y dominio explícito

Las construcciones canónicas son:

```cpp
nk::str empty{allocator};
nk::str name{allocator, "NK Engine"};
nk::str local_copy{allocator, source};
```

No existe constructor predeterminado, constructor propietario sólo desde texto, allocator global, estado ambiental por hilo ni scope macro. Incluso un texto corto queda enlazado al allocator recibido para que un crecimiento posterior conserve el dominio elegido.

En las plataformas de 64 bits soportadas, el objeto ocupa actualmente 48 bytes y reserva 23 caracteres inline más el terminador. El layout contiene `Allocator*`, longitud, capacidad y una unión entre el buffer inline de 24 bytes y el puntero al bloque dinámico. Un texto que cabe en 23 caracteres no asigna memoria.

La longitud excluye el terminador y la capacidad describe caracteres utilizables. `data()[length()]` y `cstr()[length()]` siempre contienen `\0`, incluso después de `assign`, `append`, `clear`, copy o move.

El crecimiento comprueba overflow antes de sumar longitud, duplicar capacidad o reservar el terminador. Primero reserva y copia en un bloque nuevo; sólo libera y publica el cambio cuando la operación puede completarse. Una allocation fallida devuelve `false` y conserva texto, longitud, capacidad y bloque originales. Los constructores y operadores que no pueden devolver error aplican el contrato fail-fast ya definido para owners.

Las operaciones sobre vistas del propio texto conservan el offset antes de crecer, por lo que `append(text.view().substr(...))` y `assign` con rangos internos siguen siendo válidos tras una realloc.

### Reglas de copy y move

| Operación | Regla del allocator |
| --- | --- |
| `str copy{source}` | Hereda el allocator prestado del origen |
| `str copy{allocator, source}` | Copia al dominio indicado |
| `source.clone(allocator)` | Devuelve una copia en el dominio indicado |
| Copy assignment | Conserva el allocator del destino |
| Move construction | Transfiere bloque y allocator; el origen queda moved-from |
| Move assignment, mismo allocator | Puede robar el bloque |
| Move assignment, allocators distintos | Mueve el contenido al storage del destino y conserva ambos dominios |

## `strbuf<N>` y truncación

`strbuf<N>` almacena hasta `N` caracteres y un terminador adicional dentro del propio objeto. No conoce allocators y nunca hace spill a memoria dinámica. `append` devuelve `false` cuando no cabe todo el texto, copia solamente el prefijo válido y activa una señal persistente `truncated()`. `clear()` vacía el buffer y reinicia esa señal.

De esta forma, el caller puede distinguir un mensaje completo de uno recortado sin inspeccionar contenido ni perder terminación nula.

## Formatting sin allocations

`format_to(strbuf&, format, args...)` limpia el destino, escribe directamente en su capacidad fija y retorna `FormatResult` con uno de tres estados: `Success`, `Truncated` o `InvalidFormat`. No construye `std::string`, no usa `std::format` y no consulta ningún allocator.

El subconjunto inicial cubre:

- strings propias, vistas, buffers, C strings y strings de transición;
- caracteres y booleanos;
- enteros decimales, hexadecimales, binarios y octales;
- floats con formato general, fijo o científico y precisión decimal;
- punteros hexadecimales;
- ancho, alineación izquierda/derecha/centrada, fill y zero-padding;
- escapes `{{` y `}}`.

Los enteros, floats y punteros usan `std::to_chars`, que no asigna. Un `format_string<Args...>` construido desde literal valida en compile time balance de llaves, cantidad de argumentos y gramática del spec. Las combinaciones de tipo/spec que sólo pueden resolverse al despachar el argumento producen un diagnóstico directo y assertion en Debug; en Release retornan `InvalidFormat`.

## Verificación

Los tests focalizados cubren:

- vistas desde puntero+longitud, `str`, `strbuf` y literales;
- comparación, búsqueda, prefijos, sufijos y subslices;
- ausencia de constructor propietario ambiental;
- SSO sin allocations y terminación nula;
- crecimiento, aliasing y fallo transaccional de allocation;
- copia heredada y explícita, clone y movimientos entre dominios;
- capacidad fija, truncación persistente y clear de `strbuf`;
- strings, chars, bools, enteros, bases, floats, punteros, padding y escapes;
- propagación de truncación desde formatting.

Resultados reproducidos dentro del flake en Linux:

```text
Debug build:    correcto
Debug tests:    71/71
Release build:  correcto
Release tests:  65/65
```

## Trabajo deliberadamente diferido

- Migrar logging y assertions a `strview`, `strbuf` y `format_to`.
- Retirar `legacy_str` al asignar un dominio explícito o una vista a cada consumidor existente.
- Ejecutar sanitizers y ampliar pruebas exhaustivas junto con el resto de estructuras propias.
- Medir si 23 caracteres inline y el factor de crecimiento requieren ajustes con cargas reales del engine.
