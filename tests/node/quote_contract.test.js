import test from "node:test";
import assert from "node:assert/strict";
import { loadJsonCommand } from "../helpers/vortex-cli.js";

test("quote-json devuelve una quote estable para la lane directa", () => {
    const quote = loadJsonCommand(["quote-json", "stable-direct", "2500000"]);

    assert.deepEqual(quote, {
        routeId: "stable-direct",
        sourceAsset: "vUSD",
        targetAsset: "vEUR",
        amountIn: 2500000,
        finalOut: 2311805,
        legs: [
            {
                id: "usd-eur-prime",
                from: "vUSD",
                to: "vEUR",
                input: 2500000,
                grossOut: 2314814,
                feeBps: 13,
                fee: 3009,
                rebate: 462,
                netOut: 2311805,
                rounding: "floor",
            },
        ],
        feeByAsset: { vEUR: 3009 },
        rebateByAsset: { vEUR: 462 },
    });
});

test("demo-json conserva metricas de liquidacion deterministas", () => {
    const demo = loadJsonCommand(["demo-json"]);

    assert.equal(demo.scenario, "demo");
    assert.equal(demo.orders, 2);
    assert.equal(demo.direct.quoteOut, 2311805);
    assert.equal(demo.direct.reserveIn, 2499994);
    assert.equal(demo.direct.finalOut, 2311806);
    assert.equal(demo.hop.quoteOut, 3464246);
    assert.equal(demo.hop.reserveIn, 3749976);
    assert.equal(demo.hop.finalOut, 3464247);
    assert.equal(demo.ledger.totals.reservedValue, 6249970);
    assert.equal(demo.ledger.totals.creditedValue, 6241660);
});
