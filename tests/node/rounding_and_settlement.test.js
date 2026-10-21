import test from "node:test";
import assert from "node:assert/strict";
import {
    createLedger,
    quoteRoute,
    settleOrder,
} from "../helpers/vortex-model.js";

test("settlement debita la reserva y acredita el activo destino", () => {
    const ledger = createLedger();
    const event = settleOrder({
        ledger,
        routeId: "stable-direct",
        amountIn: 2_500_000,
        minOut: quoteRoute("stable-direct", 2_500_000).finalOut,
    });

    assert.equal(ledger.accounts.trader.vUSD, 49_997_500_006n);
    assert.equal(ledger.accounts.trader.vEUR, 2_311_806n);
    assert.equal(ledger.accounts.maker.vEUR, 463n);
    assert.equal(ledger.accounts.protocol.vEUR, 3009n);
    assert.equal(ledger.vaults.vUSD.received, 2_499_994n);
    assert.equal(ledger.vaults.vEUR.paid, 2_312_269n);
});
