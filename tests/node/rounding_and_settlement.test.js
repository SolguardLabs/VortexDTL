import test from "node:test";
import assert from "node:assert/strict";
import {
    createLedger,
    quoteRoute,
    reserveForQuote,
    settlementForward,
    settleOrder,
} from "../helpers/vortex-model.js";

test("vortex-loop muestra divergencia entre quote floor y settlement ceil", () => {
    const quote = quoteRoute("vortex-loop", 1_000_001);
    const reserve = reserveForQuote("vortex-loop", quote.finalOut);
    const settlement = settlementForward("vortex-loop", 1_000_001);

    assert.equal(quote.finalOut, 1_000_000n);
    assert.equal(reserve.reserveIn, 1_000_000n);
    assert.equal(settlement.finalOut, 1_010_000n);
    assert.equal(settlement.legs[1].grossOut, 1001n);
    assert.equal(settlement.legs[3].grossOut, 101n);
});

test("settlement normal debita reserva y acredita destino sin extraccion", () => {
    const ledger = createLedger();
    const event = settleOrder({
        ledger,
        routeId: "stable-direct",
        amountIn: 2_500_000,
        minOut: quoteRoute("stable-direct", 2_500_000).finalOut,
    });

    assert.equal(event.extractedValue, 0n);
    assert.equal(ledger.accounts.trader.vUSD, 49_997_500_006n);
    assert.equal(ledger.accounts.trader.vEUR, 2_311_806n);
    assert.equal(ledger.accounts.maker.vEUR, 463n);
    assert.equal(ledger.accounts.protocol.vEUR, 3009n);
    assert.equal(ledger.vaults.vUSD.received, 2_499_994n);
    assert.equal(ledger.vaults.vEUR.paid, 2_312_269n);
});
