# Operación y validación

## Entorno

Node.js 24 ejecuta la suite y el cliente de referencia. La compilación nativa necesita un compilador C11 (`cc`, `gcc`, `clang` o MSVC). CI en GitHub usa Ubuntu y exige que el binario compile; el cliente JavaScript por sí solo no valida la equivalencia del motor C.

```bash
npm ci
npm run ci
./build/vortexdtl demo-json
./build/vortexdtl route-policy-json stable-direct
./build/vortexdtl liquidity-json vUSD 1000000 500
```

`npm run ci` ejecuta compilación obligatoria, sintaxis JavaScript y pruebas Node. Si no hay compilador local, `npm test` ejecuta solamente la suite del cliente de referencia; indique ese límite en cualquier informe. Los comandos con `-json` imprimen un documento por ejecución y salen con código no cero ante errores de entrada o estado.

## Procedimiento de revisión operativa

1. Revise `route-policy-json` para la ruta objetivo: `ok`, bitmask de avisos, número de legs y máximos de tarifa/utilización.
2. Solicite `quote-json` con importe y `minOut` esperados; conserve el identificador de ruta y las unidades mínimas por activo.
3. Ejecute `settle-json` en un contexto controlado y archive el recibo junto con los saldos antes y después. La CLI de ejemplo reinicializa el contexto en cada invocación.
4. Observe `health-json` después de una serie controlada de órdenes y contraste `available`, `paid`, `received` y balances por rol.
5. Use `liquidity-json` para estimar el colchón de un pago adicional. No trate `withinPolicy` como autorización de liquidación: solo indica si ese escenario aislado supera el colchón indicado.

Las identidades de publicación se validan con `release-integrity`: `main`, `production` y el tag anotado `v1.0.0` deben señalar el mismo commit. La release contiene código y documentación; no despliega un servicio ni conecta activos externos.
