# Security Policy

Vortex DTL es un laboratorio vulnerable por diseno. El codigo no debe
desplegarse en produccion ni conectarse a saldos reales.

## Alcance Del CTF

El alcance incluye:

- motor C de pricing y liquidacion;
- modelo JavaScript de referencia;
- tests de rutas, fees, rounding y settlement;
- escenarios CLI locales.

Queda fuera de alcance:

- persistencia real;
- redes publicas;
- claves privadas;
- integraciones con custodios;
- servicios externos.

## Supuestos Del Modelo

- Los ratios de ruta son confiables dentro del motor.
- Los assets usan enteros sin coma flotante.
- Las fees se expresan en basis points.
- La liquidacion es determinista.
- Los tests no requieren informacion fuera del repositorio.

## Advertencia Para Auditores

Este CTF contiene una vulnerabilidad economica intencional. El objetivo no es
encontrar overflows triviales, sino razonar sobre equivalencia entre fases de
pricing y liquidacion.

Las funciones de interes son:

- `vdtl_quote_route`;
- `vdtl_reserve_for_quote`;
- `vdtl_settlement_forward`;
- `vdtl_settle_order`.

## Reportes

Para el laboratorio, documenta:

- ruta usada;
- importe inicial;
- quote preliminar;
- importe reservado;
- importe acreditado;
- diferencia acumulada;
- motivo por el que los checks locales pasan.
