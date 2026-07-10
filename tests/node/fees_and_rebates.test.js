import test from "node:test";
import assert from "node:assert/strict";
import {
    createCatalog,
    dynamicFeeBps,
    quoteRoute,
    settlementForward,
} from "../helpers/vortex-model.js";

test("la fee dinamica combina base, volatilidad y utilizacion", () => {
    const catalog = createCatalog();
    const stable = catalog.route("stable-direct").legs[0];
    const hopA = catalog.route("rebate-hop").legs[0];
    const hopB = catalog.route("rebate-hop").legs[1];

    assert.equal(dynamicFeeBps(stable), 13);
    assert.equal(dynamicFeeBps(hopA), 10);
    assert.equal(dynamicFeeBps(hopB), 13);
});

test("la quote agrupa fees y rebates por asset destino", () => {
    const quote = quoteRoute("rebate-hop", 3_750_000);

    assert.equal(quote.finalOut, 3_464_246n);
    assert.deepEqual(quote.feeByAsset, {
        vREB: 156n,
        vEUR: 4509n,
    });
    assert.deepEqual(quote.rebateByAsset, {
        vREB: 78n,
        vEUR: 1040n,
    });
});

test("la liquidacion final redondea rebates hacia arriba", () => {
    const settlement = settlementForward("rebate-hop", 3_750_000);

    assert.equal(settlement.finalOut, 3_464_247n);
    assert.deepEqual(settlement.rebateByAsset, {
        vREB: 79n,
        vEUR: 1041n,
    });
});
