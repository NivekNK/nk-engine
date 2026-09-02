# Resultado del experimento de lookup vectorial de `map`

Fecha: 2026-09-02

Estado: rechazado; el árbol conserva exclusivamente el lookup escalar.

## Hipótesis

La metadata compacta mantiene fingerprints en bytes contiguos. El candidato
x86-64 cargaba 16 fingerprints con SSE2, producía una máscara de coincidencias
y recorría las distancias `u16` en orden para conservar exactamente las reglas
de terminación Robin Hood. Sólo se activaba para capacidades mayores a 128;
las tablas pequeñas mantenían la función escalar.

La comprobación de distancia no puede omitirse ni procesarse fuera de orden:
un slot vacío o una distancia almacenada menor que la distancia actual termina
la búsqueda antes de cualquier candidato posterior. Por eso el candidato aún
debía ejecutar el loop escalar por lane después de cada carga SIMD.

## Correctitud evaluada

El candidato compiló y pasó las 12 pruebas de `Map`, incluidas capacidad
mínima, lookup hit/miss, cluster largo, claves move-only y wrap-around con
backward-shift. El benchmark recorrió homes no alineados y grupos parciales al
cruzar el final de tablas de 16, 128 y 2048 slots.

No se implementó NEON: SSE2 falló la puerta en la primera plataforma baseline
y extender la misma estrategia rechazada a AArch64 no podía cambiar la
dependencia escalar de las distancias.

## Rendimiento por tamaño

Mediana de siete procesos; cada proceso informa la mediana de sus muestras:

| Rango/operación `u64` | Escalar ns/op | SSE2 ns/op | Cambio |
| --- | ---: | ---: | ---: |
| 0-16, hit | 8.853 | 8.850 | -0.0 % |
| 0-16, miss | 9.855 | 9.899 | +0.4 % |
| 17-128, hit | 8.581 | 8.539 | -0.5 % |
| 17-128, miss | 12.268 | 12.424 | +1.3 % |
| >128, hit | 8.837 | 11.145 | +26.1 % |
| >128, miss | 12.881 | 13.282 | +3.1 % |

La regresión también aparece en las cargas primarias grandes:

| Métrica | Escalar ns/op | SSE2 ns/op | Cambio |
| --- | ---: | ---: | ---: |
| hit `u64`, carga 80 % | 18.826 | 23.603 | +25.4 % |
| miss `u64`, carga 80 % | 14.692 | 15.005 | +2.1 % |
| hit `AllocationKey`, carga 80 % | 23.815 | 24.273 | +1.9 % |
| miss `AllocationKey`, carga 80 % | 19.642 | 20.064 | +2.1 % |
| hit `strview`, carga 80 % | 30.805 | 31.619 | +2.6 % |
| miss `strview`, carga 80 % | 17.614 | 19.415 | +10.2 % |

Los hits comunes terminan tras muy pocos probes. La carga de 16 bytes, la
creación de máscara y el segundo loop no se amortizan antes de encontrar la
clave. Los misses tampoco compensan el coste porque la condición Robin Hood
sigue obligando a inspeccionar cada distancia.

## Tamaño y compilación

Sobre el mismo `optimization_baseline` Release:

| Medición | Escalar | SSE2 | Cambio |
| --- | ---: | ---: | ---: |
| segmento `text` | 78012 B | 83884 B | +7.5 % |
| archivo ejecutable | 106264 B | 110360 B | +3.9 % |
| rebuild medido una vez | 4.911 s | 5.426 s | +10.5 % |

El tiempo de build es una muestra indicativa, no una distribución estable; el
aumento de code size sí es determinista para estos binarios.

## Decisión y reversión aplicada

La puerta exigía mejorar al menos 8 % las tablas mayores a 128 y permitía como
máximo 3 % de regresión. El candidato empeora el hit grande 26.1 %, por lo que
se rechaza en la primera iteración. Se retiraron:

- el include de intrinsics;
- el dispatch por capacidad;
- la función SSE2;
- las tres cargas temporales añadidas al benchmark.

No queda feature flag, implementación alternativa ni código SIMD muerto. La
metadata compacta y el lookup escalar aceptado permanecen sin cambios.
