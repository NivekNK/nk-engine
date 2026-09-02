# Plan de refactorización de memoria y contenedores

> Estado: plan final, implementación en curso; fases 0 a 9 completas
>
> Versión del plan: 9
>
> Última actualización: 2026-09-02

## Objetivo

Reimplementar el sistema de allocators y los contenedores propios de NK Engine sin sustituirlos por contenedores STL, conservando la ergonomía y los nombres `arr`, `dyarr` y `str`.

El objetivo global es que toda memoria dinámica poseída por el engine pase por un `Allocator` explícito. Esto incluye contenedores, strings, renderer, sistemas, logging auxiliar y metadata de memoria. No se introducirá un allocator singleton, un allocator ambiental por hilo ni un macro de scope: cada subsistema y cada objeto propietario seguirán declarando de qué allocator dependen.

El trabajo se realizará en este orden:

1. Estabilizar los allocators que ya existen.
2. Refactorizar `arr`, `dyarr` y la infraestructura de texto actualmente representada por `str`.
3. Añadir `slice` y un único `map` asociativo.
4. Completar las pruebas y benchmarks aislados.
5. Migrar globalmente los usos actuales del motor, comenzando por logging y terminando por `MemorySystem`.

Cada fase debe dejar el proyecto compilable. Los call sites del motor no se migrarán hasta que las estructuras nuevas hayan superado la fase de pruebas.

## Decisiones de diseño

### Convención de nombres

Los tipos propios seguirán el patrón actual: nombres breves, semánticos y en minúsculas dentro de su namespace correspondiente. No es necesario que todos contengan el sufijo `arr`.

| Semántica | Nombre | Namespace |
| --- | --- | --- |
| Array propietario de longitud fija en runtime | `arr<T>` | `nk::cl` |
| Array propietario dinámico | `dyarr<T>` | `nk::cl` |
| Vista contigua no propietaria | `slice<T>` | `nk::cl` |
| Contenedor asociativo por hash | `map<K, V>` | `nk::cl` |
| String propietario y allocator-aware | `str` | `nk` |
| Vista de texto no propietaria | `strview` | `nk` |
| Buffer de texto fijo | `strbuf<N>` | `nk` |

### Un solo tipo de mapa

Se implementará únicamente `map<K, V>` como contenedor asociativo general.

Un slot map no es otra implementación equivalente de un hash map: es almacenamiento de objetos direccionado por handles generacionales. No se implementará en este ciclo porque el motor todavía no ha demostrado que necesite esa abstracción.

Si más adelante los recursos necesitan handles estables, se diseñará por separado como `pool<T>` acompañado por `handle<T>`. No formará parte de `map` ni condicionará su implementación inicial.

### Hash único del motor

`map` no expondrá un parámetro de plantilla para escoger arbitrariamente el algoritmo de hash. Todo el motor utilizará una sola función interna de 64 bits:

```cpp
u64 hash64(const void* data, u64 length, u64 seed);
```

La implementación elegida es **rapidhash V3**, sucesor oficial de wyhash:

- El repositorio de wyhash declara que el algoritmo evolucionó a rapidhash.
- Rapidhash está optimizado para AMD64 y AArch64 sin exigir instrucciones vectoriales específicas.
- Su implementación oficial declara compatibilidad con GCC, Clang y MSVC, y resultados satisfactorios en SMHasher y SMHasher3.
- Su implementación de referencia es compacta, header-only y usa licencia MIT.

Durante la implementación se copiará una revisión concreta a `engine/vendor`, registrando el commit y la licencia. Nunca se seguirá `master` de forma implícita.

Referencias:

- [rapidhash oficial](https://github.com/Nicoshev/rapidhash)
- [wyhash oficial](https://github.com/wangyi-fudan/wyhash)
- [xxHash oficial, usado como referencia comparativa](https://github.com/Cyan4973/xxHash)

#### Política del hash

- `map` llamará siempre a `hash64`; no habrá selección de wyhash, XXH3 u otros algoritmos en runtime.
- Las claves integrales, enums, punteros y tipos del motor tendrán adaptadores propios que terminan en `hash64`.
- Las estructuras personalizadas deberán definir cómo serializan sus campos relevantes; no se hasheará padding arbitrario de una estructura.
- Las cadenas se hashearán por contenido, no por la dirección del puntero.
- El seed predeterminado será constante para mantener ejecuciones reproducibles.
- El constructor podrá recibir otro seed, sin cambiar el algoritmo, para casos que requieran tablas independientes.
- Los resultados de `hash64` no se persistirán en assets ni archivos: son una decisión interna versionada del runtime.
- Rapidhash es no criptográfico. Datos directamente controlados por un atacante requerirán una decisión de seguridad separada.

### Estrategia de `map`

`map<K, V>` será una tabla plana de direccionamiento abierto con:

- Inserción Robin Hood.
- Borrado mediante backward-shift deletion.
- Sin listas enlazadas, nodos ni asignaciones por entrada.
- Sin tombstones permanentes.
- Capacidad potencia de dos.
- Factor de carga máximo inicial de 80 %, modificable solamente con evidencia de benchmarks.
- Metadata de ocupación, fingerprint y distancia de sondeo por bucket.
- Almacenamiento de `K` y `V` en el mismo bloque de buckets en la primera implementación.
- Referencias e iteradores invalidados por rehash; el contrato no ofrecerá estabilidad de direcciones.
- Construcción, movimiento y destrucción correctos para claves y valores no triviales.

No se implementarán dos layouts públicos. Si en el futuro un layout con valores densos e índices separados supera claramente al layout inicial, será un cambio interno de `map`, no un segundo tipo de contenedor.

### Política global de asignación

La migración abarcará toda memoria dinámica poseída por NK Engine:

- Objetos creados por `Engine`, `App`, `Platform`, `Renderer` y sistemas.
- Storage de `arr`, `dyarr`, `str` y `map`.
- Arrays temporales y persistentes del renderer Vulkan.
- Metadata de `MemorySystem`.
- Paths, líneas de archivo y otros textos que necesiten ownership.

Los allocators se inyectarán explícitamente en constructores o métodos `init`. Cuando un constructor reciba un allocator, este ocupará el primer argumento. `MemorySystem` observará allocators, pero no será un service locator para obtenerlos. Esto mantiene visibles los dominios de memoria y permite probar cada sistema con otro allocator.

La capa inferior será una API nativa sin tracking:

```text
OS raw memory
    -> MallocAllocator
        -> arr / dyarr / str / map
            -> sistemas del engine
```

`os::allocate_raw` y `os::free_raw` nunca emitirán eventos. Los wrappers nativos instrumentados y los allocators normales podrán emitirlos después de completar una operación.

No se redirigirá memoria cuya liberación esté definida por una API externa. Por ejemplo, replies y eventos de XCB deben conservar el `free` exigido por XCB; Wayland, XKB y Vulkan deben conservar sus funciones de destrucción correspondientes. Estos casos se documentarán en una whitelist de ownership externo.

### Tracking sin dependencia circular

`Allocator` dejará de llamar directamente a métodos estáticos de `MemorySystem`. La dependencia se invertirá mediante una interfaz C++ pequeña, sin tablas manuales de callbacks ni `void* context`:

```cpp
namespace nk::mem {
    struct AllocationTracker {
        virtual ~AllocationTracker() = default;

        virtual u32 register_allocator(
            Allocator& allocator,
            const AllocatorDescriptor& descriptor) noexcept = 0;

        virtual void unregister_allocator(u32 allocator_id) noexcept = 0;
        virtual void on_allocate(const AllocationEvent& event) noexcept = 0;
        virtual FreeValidation validate_free(
            u32 allocator_id,
            void* address,
            u64 size_bytes) noexcept = 0;
        virtual void on_free(const AllocationEvent& event) noexcept = 0;
        virtual void on_reset(const AllocatorResetEvent& event) noexcept = 0;
    };
}
```

`MemorySystem` implementará esta interfaz:

```cpp
class MemorySystem final : public mem::AllocationTracker {
    // Implementación y almacenamiento de metadata.
};
```

Cada `Allocator` almacenará un `AllocationTracker*` y un ID cuando esté instrumentado. Su inicialización deberá escoger explícitamente uno de estos modos:

- Runtime instrumentado, recibiendo un `AllocationTracker&` válido.
- Uso interno no instrumentado, recibiendo un tag explícito `untracked` reservado para metadata de `MemorySystem`, la capa OS y pruebas controladas.

No existirá un `nullptr` accidental que active silenciosamente un modo sin tracking. La llamada virtual sólo existirá en builds con tracking y ocurre por asignación/liberación, no por elemento del contenedor.

El almacenamiento interno de `MemorySystem` usará un `MallocAllocator` normal inicializado explícitamente como `untracked`:

```cpp
MallocAllocator m_metadata_allocator{untracked};
cl::dyarr<AllocatorRecord> m_allocators;
cl::map<AllocationKey, AllocationRecord> m_allocations;
```

Por tanto, un crecimiento de su `map` termina en memoria nativa sin volver a generar un evento. No se creará un tipo especial de bootstrap allocator.

`AllocationKey` incluirá `allocator_id` y dirección. La dirección por sí sola no es suficiente porque el bloque padre de un `LinearAllocator` y su primera subasignación pueden compartir el mismo valor de puntero.

`MemorySystem` contendrá desde su construcción un `EarlyAllocationJournal` de capacidad fija. El journal será storage inline de registros POD, no usará allocator y podrá guardar en orden registros de allocator, asignaciones, liberaciones, resets y bajas. Los IDs tempranos se obtendrán de un contador monotónico que tampoco asigna memoria.

El bootstrap tendrá estados explícitos:

1. `Cold`: existe el objeto `MemorySystem` y su journal fijo, pero todavía no sus tablas dinámicas.
2. `Bootstrapping`: se inicializan el allocator y los contenedores internos no instrumentados; los eventos tempranos siguen entrando al journal.
3. `Ready`: se reproduce el journal en orden y, desde entonces, los eventos se aplican directamente a las tablas.
4. `ShuttingDown`: ya se destruyeron o desconectaron los allocators del runtime; se reporta y se destruye la metadata.
5. `Stopped`: sólo permanecen disponibles las primitivas OS y los diagnósticos sin asignaciones.

Desbordar el journal nunca descartará eventos silenciosamente: se registrará el número de eventos perdidos mediante la salida directa a OS, el reporte se marcará como incompleto y la inicialización fallará en una build con tracking. La capacidad concreta se elegirá después del inventario de la fase 0 y tendrá una prueba de límite.

El journal es una defensa para el intervalo de bootstrap, no una licencia para asignar arbitrariamente antes de `MemorySystem`. Un allocator normal no podrá asignar antes de completar su propio `init`; todo allocator del runtime se conectará al tracker al inicializarse; sólo el modo `untracked` explícito podrá omitirlo. También se prohibirán constructores globales o estáticos del engine que posean memoria dinámica.

El tracking no pretende interceptar asignaciones del CRT, drivers o terceros anteriores al entry point. Sobrescribir globalmente `malloc` o `new` queda fuera del diseño.

Un guard de reentrada por hilo protegerá contra logging accidental o una configuración futura incorrecta, pero las barreras principales serán el allocator de metadata no instrumentado, el journal inline y el orden de inicialización.

### Logging y assertions durante bootstrap

La ruta de diagnósticos tempranos y las assertions no dependerán de memoria dinámica instrumentada. Deben funcionar antes de `MemorySystem::init`, dentro de un método de tracking, durante out-of-memory y después de desactivar el tracker. El `LoggingSystem` completo se inicializará después de que `MemorySystem` y el allocator raíz estén listos.

Para ello:

- `strview` reemplazará vistas de texto que no requieren ownership.
- `strbuf<N>` almacenará texto en capacidad fija y siempre mantendrá terminación nula.
- `format_to(strbuf&, format, args...)` cubrirá el subconjunto de formato usado por el engine mediante conversión sin asignaciones.
- Un mensaje que exceda la capacidad se truncará con una marca; nunca realizará un spill allocation dentro del logger.
- Los estilos ANSI se almacenarán en buffers fijos o literales.
- Assertion reporting escribirá directamente mediante `os::write`.
- Los reportes de `MemorySystem` formatearán en buffers fijos y sólo llamarán al logger fuera de la mutación de sus tablas.

`str` seguirá existiendo para texto propietario fuera de la ruta crítica. Será allocator-aware, terminará en `\0` y conservará una capacidad inline pequeña para evitar asignaciones en textos cortos. La capacidad exacta se fijará tras medir tamaño de objeto y usos reales.

El orden global de vida quedará explícito:

```text
Startup:  OS raw/diagnóstico temprano -> MemorySystem + journal -> MemorySystem::init -> allocator raíz -> LoggingSystem -> sistemas del engine
Shutdown: sistemas del engine -> allocators del runtime -> reporte/finalización de MemorySystem -> LoggingSystem -> OS
```

Assertions conservarán una salida directa a OS para seguir funcionando incluso antes de inicializar logging.

### Uso de la biblioteca estándar

- Se retirarán del runtime propio `std::vector`, `std::unordered_map`, `std::map`, `std::string`, `std::string_view`, `std::format` con salida dinámica y `std::function`.
- Los `new`/`delete` propietarios se sustituirán por construcción/destrucción mediante allocators. Placement new seguirá siendo una primitiva válida para construir objetos en storage propio.
- Traits y facilidades sin ownership dinámico, como `std::type_traits`, `std::initializer_list`, `std::optional`, `std::chrono`, `std::mutex`, `std::thread` y `std::to_chars`, pueden mantenerse cuando no exista una razón medida para sustituirlos.
- GoogleTest y utilidades STL pueden utilizarse dentro del binario de pruebas.
- Headers de terceros y memoria retornada por APIs externas no se modificarán para forzar nuestros allocators.

### Inventario inicial de migración global

El inventario de 2026-09-01 identifica estos grupos:

| Área | Uso actual | Destino |
| --- | --- | --- |
| `LoggingSystem` y assertions | `std::string`, `std::string_view`, `std::format`, `std::filesystem` | `strview`, `strbuf`, `format_to` y API de plataforma |
| `MemorySystem` | `new/delete`, `std::vector`, `std::unordered_map`, `std::string`, `std::format` | allocator interno no instrumentado, `dyarr`, `map`, vistas y buffers fijos |
| `MemoryType` | `std::function` | interfaz C++ `MemoryTypeProvider` |
| `ObjectShader` y estados | `std::vector` persistente y temporal | `arr`, `dyarr` o storage temporal de tamaño conocido |
| Core, renderer y archivos | alias `str = std::string` | `str`, `strview` o `strbuf` según ownership |
| `os.cpp` y allocators | `malloc/calloc/free` | primitivas OS raw centralizadas |
| XCB/Wayland/XKB/Vulkan | memoria con contrato externo | conservar deallocator exigido y documentarlo |

La auditoría se repetirá antes de cerrar la migración porque el repositorio puede incorporar nuevos usos durante las fases anteriores.

## Contratos que deben preservarse

### `Allocator`

- Sin excepciones.
- Alineación explícita y validada.
- Overflow de tamaños detectado antes de asignar.
- Fallos de asignación definidos, sin modificar estadísticas como si hubieran tenido éxito.
- Helpers tipados para memoria cruda, construcción y destrucción.
- Tracking de archivo y línea en desarrollo.
- Tracking mediante métodos de `AllocationTracker`, no mediante dependencia directa de `MemorySystem` ni tablas de callbacks estilo C.
- Selección explícita entre tracker válido y modo interno `untracked`; nunca se inferirá el segundo desde un puntero nulo.
- Un allocator normal rechazará asignaciones antes de completar su inicialización.
- Un allocator podrá probarse sin `MemorySystem` mediante otro `AllocationTracker` o mediante el tag `untracked` explícito.

### `arr<T>`

- Se conservan `arr`, `arr_init`, `arr_init_list`, `arr_clear`, `arr_shutdown`, `data`, `length`, `empty`, `first` y `last`.
- Continúa siendo propietario de un bloque de longitud fija asignada en runtime.
- El allocator se considera prestado por defecto.
- Los movimientos transfieren ownership sin requerir `arr_reset` manual.
- Los objetos vivos se construyen y destruyen exactamente una vez.

### `dyarr<T>`

- Se conservan `dyarr`, inicialización por capacidad o longitud, listas, `push`, `insert`, `resize`, `pop`, `remove`, `reset`, `clear` y `shutdown`.
- `dyarr_at(index)` conserva la facilidad de extender el array; el intervalo creado se inicializa correctamente.
- Insertar más allá de `length()` conserva esa facilidad y crea un intervalo válido.
- `reset()` destruye objetos vivos pero conserva la capacidad.
- `clear()` destruye objetos y libera el bloque, conservando el vínculo con un allocator prestado.
- `shutdown()` además elimina el vínculo y procesa ownership explícito del allocator.

### `str`, `strview` y `strbuf`

- `str` reemplazará el alias actual a `std::string` sin cambiar el nombre usado por el engine.
- `str` será propietario, allocator-aware, mutable y terminado en nulo; almacenará un allocator prestado para crecer fuera de su capacidad inline.
- Todo constructor que seleccione un dominio recibirá primero `Allocator&`: `str name{allocator}`, `str name{allocator, "TEXTO"}` y `str copy{allocator, source}`.
- No habrá constructor propietario predeterminado, constructor desde texto sin allocator, allocator global, allocator ambiental por hilo ni `NK_ALLOCATOR_SCOPE`.
- Incluso un texto que quepa inline quedará vinculado al allocator recibido, de modo que un crecimiento posterior conserve el mismo dominio.
- El copy constructor `str copy{source}` conservará el allocator prestado del origen; `str copy{allocator, source}` y `clone(Allocator&)` permitirán elegir otro dominio explícitamente.
- Copy assignment conservará el allocator del destino. Move construction transferirá el allocator del origen; move assignment sólo robará el bloque cuando los dominios sean compatibles y, en caso contrario, moverá el contenido usando el allocator del destino.
- `strview` será una vista no propietaria de caracteres y longitud; no exigirá terminación nula.
- `strbuf<N>` será un buffer propietario inline, sin allocator y con truncación detectable.
- Se conservarán las operaciones realmente usadas: asignación, append, clear, reserve, data/cstr, length, capacity y comparación.
- Logging y assertions sólo aceptarán vistas o buffers que no necesiten asignar durante la llamada.

### Ownership de allocators

Los contenedores poseen su almacenamiento, no poseen automáticamente el `Allocator*` recibido.

Los actuales `*_init_own` conservarán el nombre y la facilidad, pero deberán recibir un token de ownership move-only que incluya la destrucción correctamente tipada. El overload inseguro que acepta cualquier `Allocator*` se mantendrá solamente durante la transición y se retirará tras migrar los call sites. Actualmente no existen consumidores del motor que dependan de esos overloads.

## Fases de implementación

### Fase 0 — Contratos y línea base

- [x] Registrar el resultado actual de Debug y Release.
- [x] Registrar los tests que pasan, fallan o abortan.
- [x] Medir una línea base pequeña para asignación, crecimiento, inserción y borrado.
- [x] Inventariar cada `new`, `delete`, `malloc`, `free` y tipo STL con ownership dentro de `engine` y `editor`.
- [x] Clasificar cada uso como memoria del engine, memoria externa o facilidad estándar sin ownership.
- [x] Registrar asignaciones realizadas actualmente por logging, assertions y formatting.
- [x] Documentar las invariantes de tamaño, alineación, ownership y duración de objetos.
- [x] Definir el comportamiento exacto ante tamaño cero y out-of-memory.
- [x] Mantener una lista de defectos conocidos que no deben convertirse en comportamiento compatible.

Evidencia: [línea base, inventario y contratos de fase 0](memory-containers-phase-0-baseline.md).

Criterio de salida: contratos escritos y resultados de referencia reproducibles.

### Fase 1 — Base `Allocator`

- [x] Separar `os::allocate_raw/free_raw` de los wrappers nativos instrumentados.
- [x] Unificar el camino Release y el camino instrumentado.
- [x] Validar potencia de dos y valor mínimo de la alineación.
- [x] Detectar overflow de `sizeof(T) * count`.
- [x] Corregir helpers de allocate/free/construct/deconstruct.
- [x] Definir estadísticas coherentes: reservado, usado, pico y asignaciones activas.
- [x] Definir `AllocationEvent`, `AllocatorDescriptor` y los IDs sin depender de contenedores.
- [x] Definir `AllocationTracker` como interfaz C++ con métodos virtuales `noexcept`.
- [x] Adaptar `MemorySystem` a la interfaz conservando temporalmente su almacenamiento actual.
- [x] Implementar inicialización explícita con tracker o tag interno `untracked`.
- [x] Permitir attach/detach controlado de un tracker sin cambiar el allocator concreto.
- [x] Hacer que el tracking sea opcional, seguro y completamente eliminado por compilación en Release.
- [x] Definir los registros POD, contador de IDs y capacidad fija de `EarlyAllocationJournal` sin depender de contenedores.
- [x] Añadir estados `Cold`, `Bootstrapping`, `Ready`, `ShuttingDown` y `Stopped`, junto con el guard de reentrada.
- [x] Rechazar asignaciones de un allocator que todavía no haya completado su `init`.
- [x] Corregir move assignment para no perder estado previo.
- [x] Mantener temporalmente la sintaxis pública y los macros de asignación actuales durante la transición.

Evidencia: [implementación, contratos y verificación de fase 1](memory-containers-phase-1-allocator.md).

Criterio de salida: el contrato base soporta correctamente allocators alineados, distingue tracking de modo `untracked`, conserva eventos durante bootstrap y no incluye `memory_system.h` desde la interfaz pública del allocator.

### Fase 2 — `MallocAllocator`

- [x] Implementar asignación alineada correcta en Linux y Windows.
- [x] Emparejar cada forma de asignación con su liberación válida.
- [x] Separar memoria cruda de inicialización explícita a cero.
- [x] Actualizar estadísticas solamente tras operaciones exitosas.
- [x] Manejar `nullptr`, tamaño cero y alineaciones sobre `max_align_t`.
- [x] Detectar mismatches de tamaño y double free en builds instrumentadas.
- [x] Preservar wrappers compatibles hasta la migración de consumidores.

Evidencia: [implementación, contratos y verificación de `MallocAllocator`](memory-containers-phase-2-malloc-allocator.md).

Criterio de salida: todas las alineaciones soportadas son correctas y no existen underflows ni contadores falsos.

### Fase 3 — `LinearAllocator`

- [x] Alinear el cursor antes de cada asignación.
- [x] Contabilizar padding dentro del espacio utilizado.
- [x] Validar capacidad sin overflow.
- [x] Soportar correctamente bloques externos y bloques poseídos.
- [x] Implementar reset total conservando `_free_linear_allocator` como compatibilidad.
- [x] Evitar limpieza completa de memoria en Release salvo petición explícita.
- [x] Corregir constructor y asignación por movimiento.
- [x] Mantener prohibida la liberación individual.

Evidencia: [implementación, contratos y verificación de `LinearAllocator`](memory-containers-phase-3-linear-allocator.md).

Criterio de salida: secuencias de asignaciones con diferentes alineaciones nunca se solapan ni exceden el bloque.

No se añadirán todavía pool, frame, stack ni free-list allocators.

### Fase 4 — Primitivas de duración de objetos

- [x] Implementar helpers internos para construir, destruir, mover y relocalizar rangos.
- [x] Usar `memcpy`/`memmove` únicamente en tipos trivialmente relocables.
- [x] Usar placement construction y destrucción individual en tipos no triviales.
- [x] Sustituir la restricción global de tipo por requisitos específicos de cada operación.
- [x] Soportar tipos move-only y sobrealineados.

Evidencia: [contratos, integración y verificación de las primitivas de duración de objetos](memory-containers-phase-4-object-lifetime.md).

Criterio de salida: `arr` y `dyarr` pueden compartir un único conjunto revisado de primitivas sin duplicar lógica Debug/Release.

### Fase 5 — `arr<T>`

- [x] Reimplementar inicialización por longitud y lista.
- [x] Construir todos los elementos que pasan a estar vivos.
- [x] Corregir destructor, clear y shutdown.
- [x] Corregir move constructor y move assignment.
- [x] Eliminar la necesidad de `arr_reset` en transferencias.
- [x] Implementar ownership explícito para `arr_init_own`.
- [x] Añadir iteración `begin/end` sin coste adicional.
- [x] Preservar la API cómoda existente mediante wrappers de transición.

Evidencia: [contratos, ownership y verificación de `arr<T>`](memory-containers-phase-5-arr.md).

Criterio de salida: ningún elemento se usa antes de ser construido ni se destruye más de una vez.

### Fase 6 — `dyarr<T>`

- [x] Separar reserva de memoria de construcción de elementos.
- [x] Corregir crecimiento y el cálculo de capacidad para índices lejanos.
- [x] Reimplementar push y push_copy mediante construcción en el extremo.
- [x] Mantener push_ptr como alias de compatibilidad.
- [x] Reimplementar insert para tipos triviales y no triviales.
- [x] Inicializar correctamente los huecos de una inserción lejana.
- [x] Reimplementar resize, pop, remove, reset, clear y shutdown.
- [x] Corregir move assignment y self-move.
- [x] Implementar ownership explícito para `dyarr_init_own`.
- [x] Garantizar que operaciones dentro de capacidad no asignan memoria.

Evidencia: [contratos, crecimiento y verificación de `dyarr<T>`](memory-containers-phase-6-dyarr.md).

Criterio de salida: `dyarr<Framebuffer>`, `dyarr<CommandBuffer>` y `dyarr<Fence>` son legalmente almacenables sin `memmove` sobre objetos vivos.

### Fase 7 — Infraestructura de texto existente

Esta fase reemplaza la infraestructura que actualmente se oculta tras `using str = std::string`; no migra todavía sus consumidores.

#### 7.1 `strview`

- [x] Implementar vista de caracteres y longitud sin ownership.
- [x] Permitir construcción desde literales, `cstr`, `str`, `strbuf` y puntero+longitud.
- [x] Implementar comparación, búsqueda, prefijos, sufijos y subslices de texto.
- [x] Diferenciar explícitamente una vista de una cadena terminada en nulo.

#### 7.2 `str`

- [x] Reemplazar el alias por un tipo propietario allocator-aware.
- [x] Mantener terminación nula y longitud separada de capacidad.
- [x] Implementar capacidad inline pequeña y crecimiento mediante allocator explícito.
- [x] Implementar como sintaxis canónica `str{allocator}` y `str{allocator, texto}`, siempre con el allocator como primer argumento.
- [x] Omitir constructores propietarios desde texto sin allocator y cualquier consulta a estado global o ambiental.
- [x] Implementar assign, append, clear, reserve, move y copy con ownership definido.
- [x] Hacer que copy assignment preserve el allocator del destino y definir el movimiento entre allocators distintos.
- [x] Implementar acceso mediante `data`, `cstr`, `length`, `capacity` y `empty`.
- [x] Proteger contra overflow y asignación fallida.

#### 7.3 `strbuf<N>` y formatting

- [x] Implementar buffer inline terminado en nulo y sin allocator.
- [x] Implementar append y truncación detectable.
- [x] Implementar `format_to` sin asignaciones para strings, chars, bools, enteros, floats y punteros.
- [x] Soportar inicialmente los formatos usados por el repositorio: `{}`, ancho/alineación, zero-padding y precisión decimal.
- [x] Rechazar formatos no soportados en compile time cuando sea posible y mediante assertion en desarrollo en los demás casos.

Evidencia: [contratos, formatting y verificación de la infraestructura de texto](memory-containers-phase-7-text.md).

Criterio de salida: existen reemplazos propios listos para probar para `std::string`, `std::string_view` y la salida dinámica de `std::format`; los call sites antiguos conservan su semántica mediante el puente explícito `legacy_str` y aún no se han migrado a los tipos nuevos.

### Fase 8 — Estructuras adicionales

#### 8.1 `slice<T>`

- [x] Implementar puntero y longitud sin ownership.
- [x] Permitir conversión segura de mutable a const.
- [x] Crear slices desde `arr`, `dyarr`, puntero+longitud y arrays C.
- [x] Implementar acceso, iteración y subslices.
- [x] No almacenar allocator ni liberar memoria.

#### 8.2 Hash interno

- [x] Incorporar rapidhash V3 como submódulo fijado a tag, con commit y licencia registrados.
- [x] Encapsularlo detrás de `hash64` sin filtrar macros o nombres externos.
- [x] Implementar adaptadores de claves primitivas y del motor.
- [x] Añadir vectores de prueba para detectar cambios accidentales de versión.
- [x] Definir seeds deterministas de motor y seeds explícitos por tabla.

#### 8.3 `map<K, V>`

- [x] Implementar buckets planos mediante el allocator indicado.
- [x] Implementar búsqueda Robin Hood con terminación por distancia.
- [x] Implementar inserción y reemplazo.
- [x] Implementar borrado por backward shift, sin tombstones.
- [x] Implementar reserve y rehash.
- [x] Implementar find, contains, at, insert, insert_or_assign y remove.
- [x] Soportar claves y valores no triviales.
- [x] Documentar invalidación de referencias e iteradores.
- [x] No exponer selección de hashes alternativos: `map` siempre usa `hash64`.

Evidencia: [slices, hash V3 y tabla Robin Hood](memory-containers-phase-8-structures.md).

Criterio de salida: existe una única tabla asociativa propia, plana y sin asignaciones por nodo.

### Fase 9 — Pruebas y benchmarks aislados

- [x] Separar tests con tracking activo e inactivo en configuraciones coherentes.
- [x] Eliminar el `#undef NK_ACTIVE_MEMORY_SYSTEM` local de los tests actuales.
- [x] Probar `MemorySystem` sin inicializar, inicializado y finalizado.
- [x] Probar un `AllocationTracker` C++ falso sin depender de `MemorySystem`.
- [x] Probar attach, detach y destrucción de allocators con el tracker en cada estado.
- [x] Probar captura, reproducción ordenada, cancelación de pares allocate/free y overflow del journal temprano.
- [x] Probar que el allocator interno no instrumentado no genera recursión.
- [x] Probar que un allocator sin inicializar rechaza asignaciones y que el modo `untracked` siempre es explícito.
- [x] Probar alineaciones de tipos normales y sobrealineados.
- [x] Probar fallos de asignación con un allocator de pruebas.
- [x] Probar tipos triviales, move-only, no default-constructible y contadores de destructores.
- [x] Cubrir todas las operaciones públicas de `arr`, `dyarr`, `str`, `strview`, `strbuf`, `slice` y `map`.
- [x] Verificar las formas `str{allocator}`, `str{allocator, texto}`, copia heredada, copia a otro dominio y ausencia de construcción propietaria ambiental.
- [x] Probar formatting, límites y truncación de `strbuf` sin asignaciones.
- [x] Probar colisiones forzadas y cadenas largas de sondeo en `map`.
- [x] Probar borrado al inicio, centro y final de clusters Robin Hood.
- [x] Ejecutar ASan y UBSan en Linux.
- [x] Ejecutar Debug y Release.
- [x] Medir asignaciones reales del logger actual para comparar después de la migración.
- [x] Medir P50/P95/P99 para lookup, insert y remove.
- [x] Comparar rapidhash con la línea base sobre claves reales del motor, sin incluir algoritmos alternativos en el producto final.

Evidencia: [matriz de tests, sanitizers y benchmarks](memory-containers-phase-9-validation.md).

Criterio de salida: cero errores de sanitizers, cobertura de invariantes y ausencia de regresiones de rendimiento no explicadas.

### Fase 10 — Migración global del runtime

La migración se realizará después de aprobar la fase de pruebas. `MemorySystem` será el último consumidor porque depende de que allocators, logging, texto, `dyarr` y `map` ya sean estables.

#### 10.1 Logging y assertions

- [x] Sustituir `std::string`, `std::string_view` y `std::format` por `strview`, `strbuf` y `format_to`.
- [x] Sustituir styles dinámicos por buffers fijos o literales.
- [x] Obtener el path del proyecto sin `std::filesystem` en la ruta de logging.
- [x] Garantizar truncación segura de mensajes largos.
- [x] Garantizar cero asignaciones para logs normales, assertions y errores de allocator.
- [x] Impedir que la escritura de un log genere eventos de tracking.
- [x] Separar la salida OS de diagnóstico temprano del `LoggingSystem` y reordenar el entry point según el ciclo de vida definido.

#### 10.2 Core, texto y archivos

- [x] Eliminar el puente `legacy_str = std::string` al migrar sus consumidores al tipo `str` propio.
- [x] Migrar `ApplicationConfig`, `Renderer` y nombres de aplicación a `str`/`strview` según ownership.
- [x] Migrar `File`, paths, `read_line` y rutas de shaders con allocator explícito.
- [x] Usar `str{allocator, texto}` en todo call site propietario; usar `strview` para literales y entradas no propietarias.
- [x] Sustituir strings temporales de `Instance` y `Device` por vistas o buffers fijos.
- [x] Sustituir `std::function` de `MemoryType` por una interfaz C++ `MemoryTypeProvider` con métodos.
- [x] Sustituir los `new`/`delete` propietarios restantes por helpers de allocator.

#### 10.3 Contenedores del renderer

- [x] Sustituir `std::vector<VkDescriptorSet>` de `ObjectShader` por `arr` o `dyarr` según su ciclo de vida.
- [x] Sustituir `std::vector` de `ObjectShaderObjectState` por estructuras propias.
- [x] Sustituir los vectors temporales de `VkDescriptorSetLayout` por `arr`, `dyarr` o storage temporal conocido.
- [x] Migrar extensiones y resultados Vulkan triviales de `Instance` y `Device`.
- [x] Migrar buffers temporales de consultas Vulkan.
- [x] Migrar `Swapchain` y sus arrays de imágenes y views.
- [x] Migrar `Framebuffer` y `arr<VkImageView>`.
- [x] Validar `dyarr<Framebuffer>`, `dyarr<CommandBuffer>`, `dyarr<Fence>` y `dyarr<Fence*>`.
- [x] Migrar el almacenamiento de `TextureData`.

#### 10.4 Propagación global de allocators

- [x] Construir y destruir `Engine`, `App`, `Platform`, `Renderer` y sistemas mediante allocators explícitos.
- [x] Evitar allocators globales obtenidos mediante service locator.
- [x] No introducir allocators ambientales por hilo, scopes implícitos ni macros que oculten la dependencia.
- [x] Mantener dominios nombrados para App, Renderer y Event usando inicialmente `MallocAllocator`.
- [x] Migrar `EventSystem` y sus `dyarr<RegisteredEvent>`.
- [x] Migrar toda memoria nativa poseída por `File`, recursos y renderer.
- [x] Asegurar que los allocators mismos tienen un owner y orden de destrucción definidos.

#### 10.5 `MemorySystem`

- [x] Hacer que `MemorySystem` sea la implementación final de `AllocationTracker` conservando la interfaz desacoplada. (adelantado en fase 1)
- [x] Incorporar el `EarlyAllocationJournal` inline y reproducirlo antes de entrar en `Ready`. (adelantado en fase 1)
- [x] Crear su `MallocAllocator` de metadata con el tag explícito `untracked`.
- [x] Sustituir `std::vector<AllocationStats>` por `dyarr<AllocatorRecord>`.
- [x] Sustituir `std::unordered_map` por un único `map<AllocationKey, AllocationRecord>`.
- [x] Usar `{allocator_id, address}` como clave de asignación.
- [x] Inicializar y reservar los contenedores durante `Bootstrapping`.
- [x] Deshabilitar eventos antes de reportar y destruir durante `ShuttingDown`.
- [x] Reportar por separado el consumo agregado del allocator de metadata.
- [x] Evitar logs y cualquier operación asignadora mientras se mutan las tablas de tracking.
- [x] Verificar que no existen asignaciones normales antes de preparar el tracker ni después de finalizarlo.
- [x] Reportar de forma visible cualquier overflow del journal o intento de asignación fuera del ciclo de vida.

#### 10.6 Auditoría de ownership externo e integración

- [x] Clasificar los `malloc/free` restantes y conservar sólo los exigidos por APIs externas o por la capa OS raw.
- [x] Documentar XCB, Wayland, XKB, Vulkan y libc en la whitelist de ownership externo.
- [x] Confirmar mediante búsqueda que no quedan contenedores STL dinámicos fuera de la whitelist.
- [x] Ejecutar tests después de cada grupo de migración.
- [x] Ejecutar el motor en Wayland/Niri después de cada grupo de riesgo.

Criterio de salida: toda memoria dinámica poseída por NK Engine pasa por un allocator explícito, logging no asigna en su ruta normal y el motor inicia y cierra sin fugas ni dobles liberaciones detectadas.

### Fase 11 — Limpieza y cierre

- [x] Retirar overloads inseguros de `*_init_own`.
- [x] Retirar wrappers de compatibilidad sin consumidores.
- [x] Eliminar implementaciones duplicadas entre Debug y Release.
- [x] Retirar de `nkpch.h` los headers STL que ya no tengan consumidores del runtime.
- [x] Mantener una whitelist revisada para facilidades estándar sin ownership y memoria externa.
- [x] Confirmar cero asignaciones inesperadas en el frame estable.
- [x] Comparar resultados finales contra la línea base de la fase 0.
- [x] Documentar las APIs finales y ejemplos de uso.
- [x] Decidir con evidencia si se necesita posteriormente `pool<T>` + `handle<T>`.

Evidencia: [cierre, API final y comparación](memory-containers-phase-11-closeout.md).

Criterio de salida: la refactorización queda cerrada, documentada y sin compatibilidad temporal pendiente.

## Matriz de seguimiento

Estados permitidos: `pendiente`, `en progreso`, `bloqueada`, `completa`.

| ID | Fase | Estado | Commit/PR | Notas |
| --- | --- | --- | --- | --- |
| P0 | Contratos y línea base | completa | — | [evidencia](memory-containers-phase-0-baseline.md) |
| P1 | Base `Allocator` | completa | — | [evidencia](memory-containers-phase-1-allocator.md) |
| P2 | `MallocAllocator` | completa | — | [evidencia](memory-containers-phase-2-malloc-allocator.md) |
| P3 | `LinearAllocator` | completa | — | [evidencia](memory-containers-phase-3-linear-allocator.md) |
| P4 | Duración de objetos | completa | — | [evidencia](memory-containers-phase-4-object-lifetime.md) |
| P5 | `arr<T>` | completa | — | [evidencia](memory-containers-phase-5-arr.md) |
| P6 | `dyarr<T>` | completa | — | [evidencia](memory-containers-phase-6-dyarr.md) |
| P7.1 | `strview` | completa | — | [evidencia](memory-containers-phase-7-text.md) |
| P7.2 | `str` | completa | — | [evidencia](memory-containers-phase-7-text.md) |
| P7.3 | `strbuf<N>` y formatting | completa | — | [evidencia](memory-containers-phase-7-text.md) |
| P8.1 | `slice<T>` | completa | — | [evidencia](memory-containers-phase-8-structures.md) |
| P8.2 | `hash64`/rapidhash | completa | — | [evidencia](memory-containers-phase-8-structures.md) |
| P8.3 | `map<K, V>` | completa | — | [evidencia](memory-containers-phase-8-structures.md) |
| P9 | Tests y benchmarks | completa | — | [evidencia](memory-containers-phase-9-validation.md) |
| P10.1 | Logging y assertions | completa | — | [evidencia](memory-containers-phase-10-runtime-migration.md) |
| P10.2 | Core, texto y archivos | completa | — | [evidencia](memory-containers-phase-10-runtime-migration.md) |
| P10.3 | Contenedores del renderer | completa | — | [evidencia](memory-containers-phase-10-runtime-migration.md) |
| P10.4 | Propagación de allocators | completa | — | [evidencia](memory-containers-phase-10-runtime-migration.md) |
| P10.5 | `MemorySystem` | completa | — | [evidencia](memory-containers-phase-10-runtime-migration.md) |
| P10.6 | Auditoría e integración | completa | — | [evidencia](memory-containers-phase-10-runtime-migration.md) |
| P11 | Limpieza y cierre | completa | — | [evidencia](memory-containers-phase-11-closeout.md) |

## Defectos cerrados durante el ciclo

- [x] Logging y assertions ya no construyen `std::string` ni asignan en sus rutas medidas.
- [x] Se eliminó el puente temporal `legacy_str = std::string`.
- [x] `ObjectShader` y sus estados usan storage propio.
- [x] `MemoryType` usa una interfaz `Provider` no propietaria.
- [x] La inserción lejana de `dyarr` crece y construye el rango necesario.
- [x] La relocalización no trivial usa primitivas de lifetime, no `memmove`.
- [x] `resize` construye y destruye exactamente el rango vivo.
- [x] Move assignment de `arr` y `dyarr` libera primero el destino.
- [x] Reset destruye objetos no triviales.
- [x] `*_init_own` recibe exclusivamente `AllocatorOwner` y conserva un destructor tipado.

## Fuera de alcance de este ciclo

- Pool allocator.
- Frame allocator.
- Free-list allocator.
- Stack allocator.
- Contenedor inline/small-buffer genérico; `str` y `strbuf` sí tendrán storage inline específico.
- Sets, trees, queues y deques.
- `pool<T>` y handles generacionales, hasta tener un consumidor real.
- Sustitución de facilidades estándar sin ownership dinámico sólo por principio; se conservarán cuando sean adecuadas.
- Modificación de las reglas de ownership impuestas por XCB, Wayland, XKB, Vulkan u otras APIs externas.
- Hash criptográfico o protección completa contra entradas hostiles.
- Allocator ambiental, `thread_local` para seleccionar allocator, `NK_ALLOCATOR_SCOPE` o macros equivalentes.
