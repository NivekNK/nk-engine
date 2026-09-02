# Evidencia del layout compacto de `map`

Fecha: 2026-09-02

## Representación aceptada

Cada tabla conserva una única allocation, dividida internamente en:

```text
[fingerprints u8][padding][distance_plus_one u16][padding][slots K/V]
```

- `distance_plus_one == 0` representa un slot vacío. Por ello los 256 valores
  del fingerprint quedan disponibles y no chocan con estados de control.
- El fingerprint usa los 8 bits altos del hash rapidhash y evita comparar una
  clave cuando esos bits no coinciden.
- La distancia `u16` admite probes de 0 a 65534. Las tablas que podrían exceder
  ese rango ejecutan un preflight de metadata antes de mover objetos; una
  saturación produce `capacity_overflow`, nunca un truncamiento.
- El hash completo ya no se almacena por slot. Sólo se recalcula durante rehash,
  antes de mover la clave.
- El cálculo de offsets, padding, tamaño de metadata y tamaño de slots valida
  cada suma, multiplicación y redondeo antes de solicitar memoria.
- El objeto `map<u64, u64>` conserva sus 40 bytes; sólo cambia su backing
  storage.

Las tablas de hasta 32768 slots no pueden desbordar la codificación `u16` por
construcción, así que su fast path no paga el preflight. Esto fue necesario
para mantener inserción y churn dentro de la puerta.

## Memoria

Comparación con `docs/result-containers-phase-0-baseline.md`:

| Especialización/carga | Baseline | Candidato | Reducción |
| --- | ---: | ---: | ---: |
| `map<u64,u64>`, 4096/8192 | 327680 B | 155648 B | 52.5 % |
| `map<u64,u64>`, 6553/8192 | 327680 B | 155648 B | 52.5 % |
| `map<AllocationKey,AllocationRecord>`, 6553/8192 | 720896 B | 548864 B | 23.9 % |
| `map<strview,u64>`, 1638/2048 | 98304 B | 55296 B | 43.8 % |

El coste efectivo por slot pasa de 40 a 19 bytes para `u64/u64`, de 88 a 67
bytes para el registro del `MemorySystem` y de 48 a 27 bytes para
`strview/u64`, incluyendo metadata.

## Rendimiento

Las cifras son la mediana de siete procesos para `optimization_baseline` y de
nueve procesos para `collections_benchmark`. Cada proceso informa la mediana
de sus propias muestras. Un porcentaje negativo es una mejora.

| Métrica | Baseline ns/op | Candidato ns/op | Cambio |
| --- | ---: | ---: | ---: |
| hit `u64`, carga 50 % | 11.297 | 10.741 | -4.9 % |
| miss `u64`, carga 50 % | 15.494 | 12.895 | -16.8 % |
| hit `u64`, carga 80 % | 21.714 | 18.826 | -13.3 % |
| miss `u64`, carga 80 % | 18.355 | 14.692 | -20.0 % |
| hit `AllocationKey`, carga 80 % | 27.123 | 23.815 | -12.2 % |
| miss `AllocationKey`, carga 80 % | 26.061 | 19.642 | -24.6 % |
| hit `strview`, carga 80 % | 30.925 | 30.805 | -0.4 % |
| miss `strview`, carga 80 % | 19.253 | 17.614 | -8.5 % |
| churn `u64`, carga 80 % | 114.836 | 114.226 | -0.5 % |
| insert `u64` histórico | 22.981 | 21.059 | -8.4 % |
| remove `u64` histórico | 18.986 | 13.942 | -26.6 % |

## Regresión funcional

La cobertura específica incluye:

- un cluster de 300 hashes idénticos, suficiente para demostrar el rango por
  encima de `u8`, con valores move-only y destrucción exacta;
- clusters que cruzan el final del backing storage y borrado backward-shift;
- aliasing de un valor existente durante un crecimiento;
- slots con valores alineados a 64 bytes;
- OOM de crecimiento sin cambiar longitud, capacidad ni elementos;
- iteración, move de la tabla, lookup heterogéneo y reserva con overflow.

Validación ejecutada:

```bash
nix develop -c ctest --test-dir out/build/Linux-Debug --output-on-failure
NK_BUILD_DIR=/tmp/nk-engine-map-release .scripts/build.sh Release --target tests
nix develop -c ctest --test-dir /tmp/nk-engine-map-release --output-on-failure
NK_BUILD_DIR=/tmp/nk-engine-map-sanitized NK_ENABLE_SANITIZERS=ON \
  .scripts/build.sh Debug --target tests
nix develop -c ctest --test-dir /tmp/nk-engine-map-sanitized --output-on-failure
```

Resultados:

- Debug: 112/112.
- Release limpio: 104/104.
- ASan + UBSan limpio: 112/112, sin diagnósticos.

## Decisión de puerta

Se acepta el candidato por la primera condición: supera 40 % de reducción en
`u64/u64` y 20 % en el mapa real del `MemorySystem`, mientras todas las cargas
primarias son neutrales o mejores. No se requiere activar la reversión al
bucket AoS con hash completo.
