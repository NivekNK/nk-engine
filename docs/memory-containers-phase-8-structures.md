# Slices, hash y tabla asociativa

> Estado: completa
>
> Fecha: 2026-09-02

## Resultado

El engine dispone ahora de `nk::cl::slice<T>` como vista contigua general y de una única tabla `nk::cl::map<K,V>` con buckets planos y Robin Hood hashing. Todo `map` usa exclusivamente el wrapper `nk::hash64`; no existe un parámetro de plantilla que permita sustituir el algoritmo por tabla.

## Dependencia rapidhash

Rapidhash se incorporó siguiendo la misma política que las demás dependencias externas del repositorio:

| Campo | Valor fijado |
| --- | --- |
| Repositorio | `https://github.com/Nicoshev/rapidhash` |
| Ruta | `engine/vendor/rapidhash` |
| Tag | `rapidhash_v3` |
| Commit | `bc4b4baa48a15ff52ff4725e1ccdcda62815221c` |
| Licencia | MIT, copyright 2025 Nicolas De Carli |

La revisión se registra como gitlink en `.gitmodules`, no como una copia modificable del header. El flake contiene un input no-flake fijado al mismo commit y reconstruye el directorio del submódulo en builds aislados. Esto mantiene idéntica la fuente tanto para clones con submódulos como para el source filtrado que Nix obtiene desde Git.

Sólo `core/hash.cpp` incluye `rapidhash.h`. Los headers públicos no exponen sus macros, secrets ni nombres globales. La API propia se limita a `hash64_bytes` y overloads `hash64` para:

- `strview`, `str` y C strings;
- integrales y enums;
- punteros, normalizados a `uintptr_t`;
- adaptadores del engine encontrados mediante ADL.

Se definió un seed determinista general y seeds nombrados para tablas de metadata. `map_init` exige siempre un seed, de modo que la selección por tabla queda visible en el call site.

Tres vectores fijan el resultado exacto de rapidhash V3 con el seed determinista. Un cambio accidental de tag, secretos o algoritmo rompe inmediatamente la suite.

## `slice<T>`

`slice<T>` contiene sólo `T*` y `u64`. No almacena allocator, no destruye elementos y no libera memoria. Puede construirse desde puntero+longitud, arrays C y cualquier `arr` o `dyarr` compatible mediante sus APIs `data/length`.

La conversión de `slice<T>` a `slice<const T>` es válida; la conversión inversa se rechaza en compile time. Se implementaron acceso indexado, `at`, `first`, `last`, iteración y `subslice`. Un subslice fuera del rango produce una vista vacía y un count excesivo se recorta al storage disponible.

## `map<K,V>`

Cada bucket almacena inline:

- hash completo;
- distancia desde el bucket ideal;
- estado ocupado;
- storage alineado para una clave y un valor vivos sólo cuando el bucket está ocupado.

No existen nodos ni allocations por elemento. La tabla reserva un único bloque mediante el allocator prestado y mantiene una carga máxima del 80 %. Las capacidades son potencias de dos con un mínimo de ocho buckets.

### Inserción y búsqueda

La búsqueda termina al encontrar un bucket vacío o una distancia inferior a la recorrida. La inserción mantiene un elemento pendiente y lo intercambia con el bucket cuando su distancia es mayor, preservando la invariante Robin Hood. El intercambio usa move construction y destrucción explícita; no requiere default construction ni move assignment de claves o valores.

`insert` rechaza duplicados. `insert_or_assign` reconstruye el valor existente sin exigir assignment. `reserve` calcula la capacidad real considerando el límite de carga y `rehash` mueve cada objeto vivo una sola vez al nuevo bloque.

Una allocation fallida antes de crecer conserva bloque, capacidad, longitud y elementos originales. Las referencias a argumentos internos se materializan antes de rehash para no depender de direcciones que puedan invalidarse.

### Borrado e invalidación

`remove` destruye el bucket elegido y desplaza hacia atrás el resto del cluster hasta encontrar un bucket vacío o de distancia cero. No usa tombstones, por lo que no acumula degradación tras secuencias de borrado.

Las reglas públicas quedan documentadas en el header:

- reserve o rehash invalidan todas las referencias e iteradores;
- insert sin rehash conserva referencias;
- remove invalida el elemento y los buckets desplazados dentro de su cluster.

La iteración salta buckets vacíos y entrega referencias a clave constante y valor mutable o constante según la tabla.

## Verificación

Los tests cubren:

- construcción y conversión const de slices;
- vistas sobre arrays C, `arr` y `dyarr` sin alterar ownership;
- vectores exactos de rapidhash V3 y seeds distintos;
- adapters de strings, números y punteros;
- inserción, duplicados, reemplazo, lookup, iteración, clear y shutdown;
- cadenas largas con hash forzado y crecimiento Robin Hood;
- valores move-only sin constructor predeterminado ni move assignment;
- borrado al inicio, centro y final de un cluster;
- fallo transaccional de crecimiento;
- move construction/assignment de la tabla y liberación del destino.

Resultados reproducidos dentro del flake en Linux:

```text
Debug build:    correcto
Debug tests:    79/79
Release build:  correcto
Release tests:  73/73
```

## Trabajo deliberadamente diferido

- Sanitizers y mediciones P50/P95/P99 de lookup, insert y remove.
- Integración de `map` en las tablas de `MemorySystem`.
- Ajustar carga máxima o crecimiento sólo si los benchmarks con claves reales lo justifican.
