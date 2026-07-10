import test from "node:test";
import assert from "node:assert/strict";
import { createLedger, quoteRoute, settleOrder, VortexError } from "../helpers/vortex-model.js";

test("settleOrder rechaza minOut por encima de la quote preliminar", () => {
    const ledger = createLedger();
    const quote = quoteRoute("stable-direct", 2_500_000);

    assert.throws(
        () =>
            settleOrder({
                ledger,
                routeId: "stable-direct",
                amountIn: 2_500_000,
                minOut: quote.finalOut + 1n,
            }),
        (error) => error instanceof VortexError && error.code === "VDTL_SLIPPAGE",
    );
});

test("settleOrder acepta minOut igual a la quote preliminar", () => {
    const ledger = createLedger();
    const quote = quoteRoute("stable-direct", 2_500_000);
    const event = settleOrder({
        ledger,
        routeId: "stable-direct",
        amountIn: 2_500_000,
        minOut: quote.finalOut,
    });

    assert.equal(event.quote.finalOut, quote.finalOut);
    assert.equal(event.reserve.reserveIn, 2_499_994n);
    assert.equal(event.settlement.finalOut, 2_311_806n);
    assert.equal(ledger.events.length, 1);
});
