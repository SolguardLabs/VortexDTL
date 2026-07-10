# Vortex DTL Challenge Notes

Este documento acompana al CTF. No contiene la solucion final, pero fija el
modelo mental que un auditor necesita para explorar el fallo.

## Historia Del Sistema

Vortex DTL simula un motor interno de swaps entre bovedas. El equipo de
protocolo queria soportar varios activos contables con precisiones distintas
sin usar coma flotante. Para mantener el motor simple, todas las rutas pasan por
una precision comun antes de convertir al asset destino.

El diseno se dividio en tres fases:

1. cotizar;
2. reservar;
3. liquidar.

La separacion parece limpia. La quote es rapida y conservadora. La reserva evita
bloquear mas fondos de los necesarios. La liquidacion final intenta favorecer la
ejecucion determinista cuando aparecen residuos.

La vulnerabilidad aparece porque esas tres frases no son la misma operacion.

## Ficheros Que Debes Leer

Empieza por:

- `include/vortexdtl.h`
- `src/vdtl_amount.c`
- `src/vdtl_quote.c`
- `src/vdtl_ledger.c`
- `tests/node/rounding_and_settlement.test.js`
- `tests/node/exploit_reproduction.test.js`

Luego mira:

- `tests/fixtures/scenarios.js`
- `tests/helpers/vortex-model.js`
- `src/vdtl_audit.c`

La implementacion JS existe para que el CTF pueda ejecutarse aunque no haya
compilador C instalado. El motor que debes auditar es el C.

## Preguntas De Auditoria

1. Que precision usa cada fase?
2. Donde se convierte de precision comun a precision del asset?
3. Que fase usa `floor`?
4. Que fase usa `ceil`?
5. La reserva se calcula desde el mismo input que la liquidacion?
6. La comparacion de slippage protege el importe reservado o solo la quote?
7. Donde se compara valor debitado contra valor acreditado?
8. Como cambian los resultados al pasar por assets de 2 o 3 decimales?
9. Que ocurre si el asset origen y el destino final son el mismo?
10. Que parte del sistema acumula la diferencia de liquidez?

## Rutas

### stable-direct

Ruta normal:

```text
vUSD(6) -> vEUR(6)
```

Incluye fee dinamica y rebate. Es util para comprobar que el motor no es
trivialmente incorrecto.

### rebate-hop

Ruta normal de dos legs:

```text
vUSD(6) -> vREB(4) -> vEUR(6)
```

Sirve para observar fees y rebates en assets distintos.

### vortex-loop

Ruta de CTF:

```text
vUSD(6) -> vQ7(7) -> vQ3(3) -> vQ8(8) -> vQ2(2) -> vQ5(5) -> vUSD(6)
```

Todos esos assets tienen la misma paridad economica en el fixture. Por tanto,
si la ruta fuese canonica, una vuelta completa no deberia crear valor. La
diferencia nace de los residuos.

## Invariante Esperada

Un motor robusto deberia poder defender una invariante como esta:

```text
value(reserved_input) >= value(final_credit + maker_rebates)
```

En Vortex DTL esa invariante no existe en `vdtl_settle_order`.

El codigo comprueba:

```text
quote.final_out >= min_out
trader_balance >= reserve.reserve_in
target_vault.available >= settlement.final_out
```

Esos checks son locales. Ninguno garantiza equivalencia economica entre fases.

## Formato De Entrega Esperado

Una solucion del CTF deberia incluir:

- ruta usada;
- importe inicial;
- salida preliminar;
- importe reservado;
- salida final;
- gap por operacion;
- numero de iteraciones;
- extraccion acumulada;
- explicacion de por que slippage y reservas no bloquean el ataque;
- propuesta de mitigacion.

## Mitigaciones Que Puedes Evaluar

No todas son equivalentes:

- reservar con `ceil` en la fase inversa;
- liquidar usando el importe reservado como input real;
- propagar residuos entre legs;
- limitar rutas con assets de baja precision;
- comparar valor reservado contra valor acreditado;
- convertir rebates a un unico dominio antes de pagarlos;
- eliminar redondeos divergentes y usar una funcion canonica.

La mitigacion minima suele ser insuficiente si deja otra fase con un dominio de
precision distinto.

## Comandos Utiles

```bash
node scripts/vortex-js.mjs quote-json vortex-loop 1000001
node scripts/vortex-js.mjs settle-json vortex-loop 1000001
node scripts/vortex-js.mjs exploit-json 64 1000001
node --test "tests/node/*.test.js"
```

Si tienes compilador C:

```bash
node scripts/build.mjs --required
build/vortexdtl quote-json vortex-loop 1000001
build/vortexdtl exploit-json 64 1000001
```

## Senales De Exito

Una reproduccion valida demuestra que:

- la ruta pasa `min_out`;
- la reserva se calcula sin error;
- la boveda destino paga;
- el balance del trader aumenta tras una vuelta que empieza y termina en
  `vUSD`;
- el vault `vUSD` pierde la misma diferencia acumulada;
- repetir la operacion incrementa la extraccion total linealmente.

El test `exploit_reproduction.test.js` muestra una version pequena. Una
solucion de auditoria deberia explicar la causa, no solo copiar el caso.
