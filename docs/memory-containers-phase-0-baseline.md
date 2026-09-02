# Fase 0 — Contratos y línea base

> Estado: completa
>
> Fecha: 2026-09-01
>
> Commit de referencia: `f7da9df0376d20a97b153f67758f3b5eb866813b`
>
> Rama: `feature/textures`

## Alcance

Esta fase registra el comportamiento anterior al refactor de memoria y contenedores. Los resultados que fallan también forman parte de la línea base: documentarlos no los convierte en comportamiento compatible.

El inventario cubre `engine` y `editor`, excluyendo `engine/vendor`, artefactos generados y tests de terceros. La única modificación ejecutable de esta fase es el target aislado `phase0_baseline`; no cambia el runtime ni se construye como parte del target predeterminado.

Al iniciar la fase, los archivos de `engine` y `editor` coincidían con el commit de referencia y el único cambio local era el directorio documental `docs/` sin añadir a Git.

## Entorno reproducible

| Componente | Valor registrado |
| --- | --- |
| Sistema | Linux `7.2.2-1-cachyos-bore`, x86_64 |
| CPU de la medición | AMD Ryzen 5 5500U, 6 cores/12 threads |
| Nix | 2.34.7 |
| nixpkgs fijado | `34ab99075ac4f7e40cf037eef32cb1c360bb85e9` |
| Compilador | GCC 15.3.0 |
| CMake | 4.3.4 |
| Ninja | 1.13.2 |
| Slang | `v2026.16-nixpkgs` |
| Estándar | C++20, excepciones deshabilitadas |

Las dependencias GLM y GoogleTest coinciden con los gitlinks y entradas fijadas en `flake.lock`.

## Comandos de reproducción

```bash
nix develop --command .scripts/build.sh Debug --clean-first --parallel 4
nix develop --command ctest --test-dir out/build/Linux-Debug/tests --output-on-failure

nix develop --command .scripts/build.sh Release --clean-first --parallel 4
nix develop --command .scripts/build.sh Release --target editor --parallel 4
nix build --no-link path:.#nk-engine

nix develop --command .scripts/build.sh Release --target phase0_baseline --parallel 4
nix develop --command bin/Linux-Release/benchmarks/phase0_baseline
```

CTest no encuentra tests desde la raíz del build. Hasta corregir P0-D11 debe ejecutarse desde el subdirectorio `tests` mostrado arriba.

## Resultado de builds

| Configuración/target | Resultado | Observaciones |
| --- | --- | --- |
| Debug completo, limpio | aprobado | 67 pasos; genera shaders, assets, engine, editor y tests |
| Release completo, limpio | falla | el target de tests no compila; P0-D10 |
| Release `editor` | aprobado | el producto enlaza correctamente aunque falle el target de tests |
| Paquete Nix Release aislado | aprobado | el flake construye únicamente el producto `editor` |
| Release `phase0_baseline` | aprobado | target aislado, tracking deshabilitado |

Ambas configuraciones emiten warnings ya existentes, principalmente parámetros no usados, inicializadores Vulkan incompletos y orden de inicialización de miembros de `Allocator`. No se corrigieron en esta fase.

### Fallo del build Release completo

`tests/src/memory/linear_allocator.cpp:27` llama directamente:

```cpp
allocator._free_linear_allocator(__FILE__, __LINE__);
```

Ese overload sólo existe cuando el tracking está activo. En Release únicamente existe `_free_linear_allocator()` y la compilación termina con `no matching function`.

## Resultado de tests

La suite Debug descubre seis tests:

| Test | Resultado |
| --- | --- |
| `Arr.ArrInit` | segfault |
| `Arr.DyarrInit` | aprobado |
| `LinearAllocator.LinearAllocatorInit` | aprobado |
| `MallocAllocator.MallocAllocatorInit` | aprobado |
| `LoggingSystem.LoggingSystemInit` | aprobado |
| `MemorySystem.MemorySystemInit` | aprobado, pero su cuerpo está comentado |

Resultado agregado: 5/6 aprobados, 1/6 abortado por `SIGSEGV`; no hubo fallos normales de expectativas.

GDB confirma que `Arr.ArrInit` entra en `MemorySystem::update_allocator`, intenta leer `MemorySystemInfo::allocations` a través de `this == nullptr` y cae en `std::vector::size`. La llamada se origina en `arr::_arr_init` porque los macros del header convierten internamente `allocate_lot_t` en la variante instrumentada aunque el test haya llamado el overload no instrumentado. Es P0-D07/P0-D27.

La suite Release no puede ejecutarse porque su binario no termina de compilar.

## Línea base de rendimiento

### Metodología

`phase0_baseline` se compila optimizado en Release, con GCC 15.3.0, siete muestras por métrica y mediana. El tracking de `MemorySystem` se deshabilita deliberadamente para medir las primitivas actuales y no las tablas STL del tracker. El helper de assertions permanece habilitado dentro de este ejecutable para poder contar su formatting; no se ejecuta ningún `debug_break`.

Las operaciones medidas son:

- Par `MallocAllocator::allocate/free` de 64 bytes.
- Inicialización desde capacidad cero y crecimiento mediante `dyarr_push_copy<u64>`.
- Inserción repetida en el centro de un `dyarr<u64>` con capacidad preasignada.
- Borrado repetido en el centro de un `dyarr<u64>`.

La inserción se mantiene dentro de capacidad para no activar el defecto conocido de inserción lejana. Estos números sirven para comparar el mismo hardware y toolchain; no son objetivos absolutos ni pruebas de corrección.

| Métrica | Operaciones | Mediana total | ns/op |
| --- | ---: | ---: | ---: |
| `malloc.allocate_free_64B` | 200000 | 1686547 ns | 8.433 |
| `dyarr.grow_push_u64` | 100000 | 808392 ns | 8.084 |
| `dyarr.insert_middle_u64` | 2000 | 1174552 ns | 587.276 |
| `dyarr.remove_middle_u64` | 2000 | 869537 ns | 434.769 |

### Layout actual en Release

| Tipo | `sizeof` | `alignof` cuando se registró |
| --- | ---: | ---: |
| `Allocator` | 40 | 8 |
| `MallocAllocator` | 40 | 8 |
| `LinearAllocator` | 48 | 8 |
| `arr<u64>` | 32 | 8 |
| `dyarr<u64>` | 40 | 8 |

Estos tamaños no son ABI pública. Se registran para detectar crecimiento accidental durante el refactor.

## Asignaciones de logging, formatting y assertions

El benchmark reemplaza `operator new/delete` únicamente dentro de su ejecutable y cuenta llamadas globales realizadas durante cada operación. No cuenta asignaciones internas de libc, drivers ni APIs externas; por tanto los valores son un límite inferior dependiente de libstdc++.

| Operación | Allocations | Bytes solicitados | Frees durante la operación |
| --- | ---: | ---: | ---: |
| `LoggingSystem::init` | 7 | 222 | 0 |
| Log sin placeholders, mensaje largo | 1 | 129 | 1 |
| Log con formatting | 4 | 342 | 4 |
| Reporte de assertion con formatting | 4 | 401 | 4 |

Las siete asignaciones de inicialización quedan retenidas por `m_style`. Un log formateado construye primero el `std::string` de `vformat_to` y después otro buffer dentro del overload final. Assertions también construyen un mensaje intermedio y el buffer final. `MemorySystem` añade más `std::string` y `std::format` mientras genera reportes.

## Inventario de memoria cruda

### `new`, `delete`, `malloc`, `calloc` y `free`

| Sitio | Operación actual | Clasificación | Destino del refactor |
| --- | --- | --- | --- |
| `engine/src/core/os.cpp:9,19` | `malloc` | backend OS del engine | conservar como primitiva raw centralizada y corregir alineación |
| `engine/src/core/os.cpp:13,26` | `free` | backend OS del engine | conservar emparejado con la primitiva raw |
| `engine/src/memory/malloc_allocator.cpp:25` | `std::calloc` | memoria poseída por el engine | usar backend raw alineado; separar zero-fill de allocate |
| `engine/src/memory/malloc_allocator.cpp:33` | `std::free` | memoria poseída por el engine | liberar mediante el backend emparejado |
| `engine/src/memory/linear_allocator.cpp:36` | `std::calloc` | bloque poseído por `LinearAllocator` | recibir explícitamente el allocator dueño del backing block |
| `engine/src/memory/linear_allocator.cpp:14` | `std::free` | bloque poseído por `LinearAllocator` | devolverlo al mismo allocator dueño |
| `engine/src/systems/memory_system.cpp:97` | `new MemorySystemInfo` | metadata poseída por el engine | allocator interno `untracked` |
| `engine/src/systems/memory_system.cpp:126` | `delete MemorySystemInfo` | metadata poseída por el engine | destrucción tipada y liberación por el allocator interno |
| `engine/src/platform/platform_linux.cpp:19` | `std::free(reply)` | ownership externo XCB | conservar el deallocator exigido por XCB |
| `engine/src/platform/platform_linux.cpp:168,202` | `std::free(error)` | ownership externo XCB | conservar el deallocator exigido por XCB |
| `engine/src/platform/platform_linux.cpp:286` | `std::free(event)` | ownership externo XCB | conservar el deallocator exigido por XCB |
| `engine/include/core/os.h:90,120` | placement new | construcción sobre storage del backend OS | conservar como primitiva tipada, comprobando `nullptr` |
| `engine/include/memory/allocator.h:113,119` | placement new | construcción sobre storage de `Allocator` | conservar como primitiva tipada, comprobando `nullptr` |

Los demás resultados textuales de `delete` corresponden a funciones de copia/movimiento declaradas `= delete`, no a liberaciones. `editor` no contiene llamadas directas a estas primitivas.

### Memoria poseída mediante helpers del engine

| Dominio | Sitios actuales | Clasificación |
| --- | --- | --- |
| Allocators raíz de App, Renderer y Event | `engine.cpp:98-124`, `renderer.cpp:23-83`, `event_system.cpp:10-27` | engine; migrar a ownership explícito y orden de vida documentado |
| Objetos App/Platform/Renderer | `app_creator.h`, `platform.cpp:20-55`, `renderer.cpp:18-84` | engine; construcción/destrucción tipada mediante allocator inyectado |
| Buffer completo de archivo | `file.cpp:97`, liberado por `vulkan/shaders/utils.cpp:43` | engine; pasar allocator explícito y liberar también en rutas de error |
| Eventos | `event_system.cpp:37-68` | engine; `dyarr` con allocator Event |
| Arrays y objetos Vulkan del renderer | `device.cpp`, `instance.cpp`, `swapchain.cpp`, `vulkan_renderer.cpp` | engine; conservar en allocators Renderer |
| Framebuffers, command buffers, fences y semáforos | `vulkan_renderer.cpp:55-67,481` | engine; storage `arr/dyarr`, recursos Vulkan con destructor propio |
| TextureData | `vulkan_renderer.cpp:365,465` | engine; objeto tipado en allocator Renderer |

## Inventario STL

### Tipos con ownership dinámico o capacidad de asignar

| Tipo/facilidad | Sitios | Clasificación y destino |
| --- | --- | --- |
| `std::vector<AllocationStats>` | `memory_system.cpp:54` | metadata del engine; `dyarr<AllocatorRecord>` interno |
| `std::unordered_map<void*, AllocatorInfo>` | `memory_system.cpp:50` | metadata del engine; `map<AllocationKey, AllocationRecord>` |
| `std::vector<u32>` | `object_shader_object_state.h:10` | renderer; contenedor propio |
| `std::vector<VkDescriptorSet>` | `object_shader_object_state.h:17`, `object_shader.h:70` | renderer; `arr` o `dyarr` según ciclo de vida |
| `std::vector<VkDescriptorSetLayout>` | `object_shader.cpp:193,438` | temporales renderer; storage propio de tamaño conocido |
| `std::string` en `MemorySystem` | `memory_system.cpp:25,31,41,42,57-80,268-389` | metadata y reporting; vistas persistentes más `strbuf/format_to` |
| `std::string` en logging | `logging_system.h:54,88`, `logging_system.cpp:12-81` | estilos y buffers; `strbuf/format_to`, sin allocation en la ruta normal |
| `std::string` en assertions | `assertion.h:12-30` | ruta crítica; `strbuf/format_to` y salida OS directa |
| Alias `str = std::string` | `defines.h:18` | sustituir por `nk::str` allocator-aware |
| Consumidores del alias `str` | `app.h:11`, `renderer.h:21,55`, `renderer.cpp:17`, `file.h:30,43`, `file.cpp:53`, `instance.cpp:83`, `device.cpp:412-415`, `shaders/utils.cpp:10` | escoger `str`, `strview` o `strbuf` según ownership |
| `std::function` | `memory_type.h:19-20,61`, `memory_system.cpp:419` | callbacks almacenados; interfaz C++ `MemoryTypeProvider` |
| `std::filesystem::current_path().string()` | `logging_system.cpp:40` | puede asignar; API de plataforma más buffer explícito |
| `std::format`/`std::vformat_to` dinámicos | `memory_system.cpp`, `logging_system.cpp/.h`, `assertion.h`, `shaders/utils.cpp` | resultados propietarios ocultos; `format_to` sobre `strbuf` o `str` explícito |

No hay `std::map`, smart pointers, lists, deques ni sets en `engine` o `editor`.

### Facilidades estándar sin ownership dinámico propio

Se clasifican como aceptables mientras no aparezca evidencia contraria:

- Traits, concepts, `std::move`, `std::forward`, placement construction y algoritmos sobre storage ajeno.
- `std::string_view`, aunque será reemplazado por `strview` para unificar la API de texto.
- `std::optional<T>` usado por colores y resultados de `dyarr`; almacena `T` inline.
- `std::initializer_list`, que sólo representa una vista temporal.
- Tipos numéricos, `std::numeric_limits`, `std::chrono`, `std::thread` y `std::this_thread`.
- `std::memcpy`, `std::memmove` y `std::memset`; su uso concreto seguirá sujeto a reglas de lifetime.

## Whitelist de ownership externo

Estas asignaciones o recursos no deben redirigirse a allocators del engine:

- XCB: replies, errores y eventos liberados con `free`; connection, key symbols y window con sus APIs XCB.
- Wayland/xdg-shell: display y proxies destruidos mediante `wl_*_destroy`, `*_release`, `xdg_*_destroy` o `wl_display_disconnect`.
- XKB: context, keymap y state liberados mediante `xkb_*_unref`.
- Mapeos y descriptores recibidos por Wayland: `mmap/munmap` y `close` emparejados.
- Vulkan: handles, descriptor sets, command buffers y device memory destruidos con la función Vulkan correspondiente y el mismo `VkAllocationCallbacks*`. Actualmente el callback es nulo y la memoria interna del driver no pertenece al engine.
- Streams libc: `FILE*` abierto con `fopen` y cerrado con `fclose`.

## Contratos finales fijados en fase 0

### Asignación y alineación

1. El runtime no usa excepciones.
2. `size_bytes == 0` devuelve `nullptr`, no llama al backend, no crea evento y no altera estadísticas.
3. Una alineación válida es una potencia de dos mayor que cero. El backend puede elevar internamente alineaciones pequeñas al mínimo exigido por la plataforma, pero el puntero resultante debe satisfacer la alineación solicitada.
4. Alineación cero, no potencia de dos o no representable es un error de contrato: se diagnostica sin asignar y se devuelve `nullptr`.
5. `sizeof(T) * count`, crecimiento de capacidad, padding y sumas de offsets se validan antes de operar. Un overflow se trata como fallo de asignación.
6. La asignación raw no inicializa el contenido. Zero-fill será una operación explícita; los contenedores construyen o inicializan sus elementos según su contrato.
7. Out-of-memory devuelve `nullptr`, deja el estado y las estadísticas sin cambios y puede emitir un diagnóstico allocation-free. Nunca cae silenciosamente a un allocator global.
8. Los helpers de construcción sólo ejecutan placement new después de obtener un puntero válido; si falla la reserva devuelven `nullptr` y no ejecutan el constructor.
9. Las estadísticas y eventos se actualizan sólo después de una operación exitosa. Deben distinguir bytes reservados, usados, pico y allocations activas.

### Liberación

1. Liberar `nullptr` es un no-op, devuelve `false` y no genera evento.
2. Una liberación válida usa el mismo allocator y el tamaño originalmente solicitado. Mismatches y double free se detectan en builds instrumentadas; son violaciones de contrato en Release.
3. Una operación válida devuelve `true` y actualiza estadísticas después de que el backend acepte la liberación.
4. Memoria retornada por una API externa conserva siempre su deallocator externo.

### Propagación de fallos

1. Inicializaciones y mutaciones de contenedores que puedan reservar memoria serán transaccionales: ante fallo no cambian datos, longitud ni capacidad y devuelven `false`.
2. Conservar los nombres actuales no obliga a conservar un retorno `void`; ignorar un futuro retorno booleano seguirá siendo sintácticamente posible durante la migración.
3. Operaciones que contractualmente deben devolver una referencia, como `dyarr_at`, usan una ruta fail-fast allocation-free si el crecimiento no puede completarse.
4. Un constructor propietario que no puede comunicar error, incluido `str{allocator, texto}`, produce un objeto válido o termina mediante la misma ruta fail-fast. Las mutaciones posteriores de `str`, como `reserve` o `append`, sí devuelven estado.

### Ownership y lifetime

1. Un contenedor posee su bloque y toma prestado su `Allocator`; el allocator debe sobrevivir al contenedor y a toda liberación futura.
2. No hay allocator global, service locator, allocator ambiental ni `NK_ALLOCATOR_SCOPE`.
3. Todo constructor que elija dominio recibe primero `Allocator&`; para texto la forma canónica es `str{allocator, "TEXTO"}`.
4. Cada objeto vivo se construye una vez y se destruye una vez. Storage reservado fuera de `[0, length)` no contiene objetos vivos.
5. Move assignment libera correctamente el estado previo del destino antes de transferir ownership y contempla self-move.
6. El ownership de un allocator sólo se transfiere mediante un token move-only con destructor correctamente tipado; un `Allocator*` prestado nunca implica ownership.

### `arr<T>`

- `data == nullptr` si y sólo si `length == 0`; un array vacío puede conservar el allocator prestado.
- La inicialización por longitud value-initializa exactamente `length` elementos; tipos escalares quedan en cero.
- La longitud no cambia hasta `clear/shutdown` o movimiento.
- `clear` destruye y libera el bloque, pero conserva el vínculo con el allocator; `shutdown` también desconecta o procesa el token de ownership.

### `dyarr<T>`

- Siempre se cumple `length <= capacity`; `data == nullptr` si y sólo si `capacity == 0`.
- Sólo `[0, length)` contiene objetos vivos.
- Crecer construye únicamente los nuevos objetos necesarios y relocaliza legalmente los existentes.
- `resize` construye al crecer y destruye al reducir.
- `reset` destruye elementos vivos y conserva el bloque; `clear` además libera el bloque; `shutdown` además desconecta el allocator.
- Inserción lejana value-initializa el hueco `[old_length, index)` y construye el valor en `index`.
- Reallocations invalidan punteros, referencias e iteradores. Inserción y borrado pueden invalidarlos desde la posición modificada.

### `LinearAllocator`

- El cursor se alinea antes de cada allocation y el uso incluye padding.
- Un bloque externo nunca se libera; un bloque poseído vuelve al allocator padre explícito.
- No existe liberación individual. `reset` invalida todas las subasignaciones, pone uso y count en cero y conserva backing storage.
- El allocator no conoce los tipos almacenados: sus owners deben destruir objetos antes del reset.

## Defectos conocidos: no preservar

| ID | Defecto de referencia |
| --- | --- |
| P0-D01 | `os::_native_allocate` y `MallocAllocator` ignoran la alineación. |
| P0-D02 | `LinearAllocator` ignora alineación y padding. |
| P0-D03 | Multiplicaciones, capacidades y offsets no comprueban overflow. |
| P0-D04 | `MallocAllocator` modifica contadores antes de saber si `calloc` funcionó y puede underflow en frees inválidos. |
| P0-D05 | Tamaño cero y liberación de `nullptr` no tienen una semántica uniforme. |
| P0-D06 | `MallocAllocator::m_data` sólo recuerda la última allocation y no representa todos sus bloques activos. |
| P0-D07 | El tracking desreferencia `MemorySystem` no inicializado; causa el segfault de `Arr.ArrInit`. |
| P0-D08 | `Allocator` depende directamente de métodos estáticos del `MemorySystem` concreto. |
| P0-D09 | Logging, assertions y formatting asignan memoria; los conteos están registrados arriba. |
| P0-D10 | El build Release completo no compila por un overload de test condicionado al tracking. |
| P0-D11 | CTest desde la raíz reporta `No tests were found`; sólo el subdirectorio `tests` los registra. |
| P0-D12 | `MemorySystem.MemorySystemInit` no prueba nada y el test de logging no contiene expectativas. |
| P0-D13 | `arr` reserva storage para clases sin construir objetos y posteriormente invoca destructores. |
| P0-D14 | Move assignment de `arr` puede perder el bloque previamente poseído por el destino. |
| P0-D15 | `arr_init_own/dyarr_init_own` no conservan el tipo dinámico ni el tamaño correcto para destruir el allocator. |
| P0-D16 | `dyarr` asigna sobre storage sin construir al hacer push de tipos no triviales. |
| P0-D17 | `dyarr_init_len` y crecimiento de `resize` declaran elementos vivos sin construirlos. |
| P0-D18 | `dyarr_at(index)` comprueba `length >= capacity` en vez de `index >= capacity`; un índice lejano puede escribir fuera del bloque. |
| P0-D19 | Inserción lejana usa la misma comprobación incorrecta de capacidad. |
| P0-D20 | Inserción y borrado usan `memmove` sobre objetos potencialmente no triviales. |
| P0-D21 | `pop`, `remove` y `reset` no cierran correctamente el lifetime de los elementos eliminados. |
| P0-D22 | Move assignment de `dyarr` puede perder storage previo y no maneja self-move. |
| P0-D23 | `LinearAllocator::reset` limpia todo el backing block incluso en Release y no coordina lifetimes. |
| P0-D24 | Los reportes de `MemorySystem` usan `std::string/std::format` mientras inspeccionan el propio tracking. |
| P0-D25 | Metadata de `MemorySystem`, vectors del renderer y callbacks `std::function` asignan fuera de allocators del engine. |
| P0-D26 | Los helpers de construcción pueden ejecutar placement new sobre `nullptr`. |
| P0-D27 | Macros de tracking cambian llamadas escritas dentro de templates y permiten mezclar rutas instrumentadas/no instrumentadas accidentalmente. |
| P0-D28 | `File::read_all_bytes` conserva el buffer si `fread` devuelve un tamaño parcial. |
| P0-D29 | Varias rutas usan el puntero de allocation sin comprobar fallo antes de `memset`, copia o acceso. |
| P0-D30 | Destructores y errores de allocator pueden entrar en logging asignador durante condiciones de memoria inválidas. |

## Criterio de salida

La fase 0 queda completa porque:

- Debug, Release producto y Release completo tienen resultado reproducible registrado.
- Cada test descubierto está clasificado como aprobado, abortado o no construible.
- Existe un benchmark pequeño repetible con resultados iniciales.
- Los usos directos de memoria, ownership STL, memoria del engine y ownership externo están inventariados.
- Las asignaciones actuales de logging/formatting/assertions están medidas.
- Tamaño, alineación, overflow, ownership, lifetime, tamaño cero y OOM tienen contratos explícitos.
- Los defectos conocidos tienen IDs y se declaran incompatibles con el resultado final.
