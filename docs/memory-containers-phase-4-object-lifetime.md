# Fase 4 — Primitivas de duración de objetos

> Estado: completa
>
> Fecha: 2026-09-01

## Resultado

Las operaciones de duración de objetos viven ahora en `memory/object_lifetime.h` y son independientes del allocator y del estado de `MemorySystem`. `arr` y `dyarr` pueden incluir el mismo header y aplicar exactamente la misma lógica en Debug y Release.

El conjunto inicial contiene:

| Primitiva | Estado inicial | Estado final |
| --- | --- | --- |
| `construct_object` | Un slot de storage raw | Un objeto construido con los argumentos recibidos |
| `construct_value_range` | Un rango de storage raw | Objetos value-initialized |
| `construct_copy_range` | Destino raw no solapado y origen vivo | Copias vivas; el origen conserva su estado y lifetime |
| `construct_move_range` | Destino raw no solapado y origen vivo | Objetos construidos por movimiento; el origen sigue vivo |
| `move_assign_range` | Dos rangos de objetos vivos | Destino asignado por movimiento; admite solapamiento |
| `destroy_range` | Un rango de objetos vivos | Todos los lifetimes terminados, en orden inverso |
| `relocate_range` | Origen vivo y destino raw fuera del solapamiento | Objetos vivos en destino y todos los lifetimes del origen terminados |

`construct_move_range` y `relocate_range` son operaciones deliberadamente distintas. La primera deja objetos movidos pero vivos en el origen; la segunda transfiere el lifetime y permite liberar inmediatamente el bloque antiguo.

## Tipos triviales y no triviales

El criterio bytewise es conservador: `TriviallyRelocatable<T>` equivale por ahora a `std::is_trivially_copyable_v<T>`. No existe un opt-in manual capaz de declarar trivial a un tipo propietario por error.

Para estos tipos:

- copy construction y move construction de rangos usan `memcpy` porque sus contratos prohíben solapamiento;
- move assignment y relocation usan `memmove` porque admiten rangos solapados;
- destruction no recorre el rango cuando el destructor es trivial.

Para tipos no triviales se usa exclusivamente `std::construct_at`, movimiento o copia tipada y `std::destroy_at`. La relocation elige el sentido del recorrido: hacia atrás cuando el destino comienza dentro del origen por la derecha y hacia delante en los demás casos. Así, cada slot solapado se destruye antes de reutilizarse y ningún objeto se destruye dos veces.

El proyecto compila sin excepciones. Los constructores, asignaciones y destructores utilizados por estas primitivas deben respetar ese contrato global; los `noexcept` de cada helper reflejan los traits del tipo y no ocultan una operación potencialmente fallable.

## Requisitos por operación

`IArrT<T>` ya no exige que toda clase sea default-constructible. Ahora sólo delega en `ObjectStorageType<T>`, que acepta tipos objeto mutables y excluye `const`, `volatile` y arrays C.

Cada primitiva expresa su requisito real mediante `requires`:

- value construction requiere default construction;
- copy construction requiere copy construction;
- move construction requiere move construction;
- move assignment requiere move assignment;
- destruction requiere un destructor accesible;
- relocation requiere destrucción y, salvo en la ruta trivial, move construction.

Esto permite que storage y contenedores se declaren para tipos move-only sin fingir que todas las operaciones son válidas. Las fases de `arr` y `dyarr` aplicarán estos mismos requisitos a cada constructor y método público.

## Integración inicial

Se retiró `mem::realocate_n` de `core/defines.h`. Sus tres consumidores actuales usan `mem::relocate_range` con el orden explícito `destination, source, count`:

- conversión móvil de `dyarr` a `arr`;
- crecimiento normal de `dyarr`;
- crecimiento instrumentado de `dyarr`.

Esta integración cubre la transferencia entre bloques. Las operaciones actuales de inserción, borrado, resize y clear de los contenedores todavía conservan sus implementaciones anteriores y se sustituirán al reimplementar cada contenedor; adelantarlas aquí mezclaría el contrato común con decisiones de ownership y capacidad que aún no corresponden.

## Verificación

Las pruebas cubren:

- requisitos distintos para tipos default-constructible, copiables y move-only;
- value construction, copy construction, move construction y move assignment;
- destrucción individual de rangos no triviales;
- relocation entre bloques y con solapamiento hacia ambos lados;
- orden estable al mover rangos solapados;
- ruta bytewise de tipos triviales;
- rangos vacíos sin storage;
- allocation y relocation de un tipo move-only con alineación de 256 bytes;
- compilación de los usos existentes en `arr` y `dyarr` en ambas configuraciones.

Resultados reproducidos dentro del flake en Linux:

```text
Debug build:    correcto
Debug tests:    36/36
Release build:  correcto
Release tests:  31/31
```

## Trabajo deliberadamente diferido

- Aplicar requisitos por operación a toda la API pública de `arr` y `dyarr`.
- Reimplementar construcción, destrucción, ownership y movimientos completos de `arr`.
- Reimplementar inserciones, desplazamientos, resize, remove y crecimiento completo de `dyarr`.
- Considerar un trait de relocation trivial extensible sólo después de definir un contrato seguro para opt-ins propietarios.
- Ejecutar sanitizers y benchmarks integrales al completar los contenedores.
