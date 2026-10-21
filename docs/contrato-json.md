# Contrato JSON

La CLI y el cliente JavaScript exponen comandos equivalentes para cotización, liquidación, política de corredor, salud y proyección de liquidez. Los campos monetarios se expresan en unidades mínimas del activo o, cuando terminan en `Value`, en micro-USD. Los agregados grandes pueden exceder el entero seguro de JavaScript: el cliente convierte esos valores a cadenas decimales cuando es necesario.

## Cotización y recibo

`quote-json <route> <amount>` devuelve `routeId`, activos de origen/destino, `amountIn`, `finalOut`, detalle de `legs`, `feeByAsset` y `rebateByAsset`. `settle-json <route> <amount> [minOut]` devuelve `settlement` y `ledger`; el recibo incluye `quoteOut`, `reserveIn`, `finalOut`, `reservedValue`, `creditedValue`, tarifas y rebates. `ledger` contiene saldos por rol, bóvedas y totales de órdenes.

Ejemplo de cotización abreviada:

```json
{
  "routeId": "stable-direct",
  "sourceAsset": "vUSD",
  "targetAsset": "vEUR",
  "amountIn": 2500000,
  "finalOut": 2311805,
  "feeByAsset": { "vEUR": 3009 },
  "rebateByAsset": { "vEUR": 462 }
}
```

## Proyección de liquidez

`liquidity-json vUSD 1000000 500` devuelve:

```json
{
  "asset": "vUSD",
  "available": 2500000000000,
  "requestedPayout": 1000000,
  "bufferBps": 500,
  "requiredBuffer": 125000000000,
  "projectedAvailable": 2499999000000,
  "headroom": 2374999000000,
  "unfunded": 0,
  "withinPolicy": true
}
```

Los importes de este reporte están en unidades mínimas de vUSD. `requiredBuffer` se calcula sobre `opening_liquidity`; `available` es el saldo actual. La proyección no altera el libro. `withinPolicy` se refiere solo al colchón solicitado, no a todas las condiciones de una orden. Conserve versión, parámetros de entrada y catálogo asociado para reproducir un reporte.
