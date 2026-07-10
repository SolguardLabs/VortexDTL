# Test Plan

El laboratorio separa los tests en cinco grupos.

## Quote Contract

`quote_contract.test.js` comprueba que la CLI devuelve JSON estable para
`stable-direct`. El objetivo es fijar nombres de campos, fee dinamica, rebate y
salida neta.

## Fees Y Rebates

`fees_and_rebates.test.js` valida que la fee se compone de:

```text
base + volatilidad + floor(utilizacion * cap / 10000)
```

Tambien comprueba que la quote agrupa importes por asset destino y que la
liquidacion final redondea rebates hacia arriba.

## Slippage

`slippage.test.js` confirma que `minOut` se compara contra la quote preliminar.
Ese comportamiento es correcto localmente, pero no protege la invariante
economica global.

## Rounding Y Settlement

`rounding_and_settlement.test.js` compara:

```text
quote(vortex-loop, 1000001)
reserve(quote.finalOut)
settlement(vortex-loop, 1000001)
```

El resultado esperado es:

```text
quoteOut     = 1000000
reserveIn    = 1000000
settlementOut= 1010000
```

## Reproduccion

`exploit_reproduction.test.js` repite la operacion y verifica que:

- el trader aumenta su balance en `vUSD`;
- el vault pierde la diferencia;
- `extractedValue` crece linealmente.

## Escaneo

`search_surface.test.js` y `scripts/audit-scan.mjs` exploran rangos de importes.
Esto ayuda a distinguir un caso aislado de una superficie real de ataque.
