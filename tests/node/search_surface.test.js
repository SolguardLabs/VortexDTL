import test from "node:test";
import assert from "node:assert/strict";
import {
    createCatalog,
    quoteRoute,
    reserveForQuote,
    settlementForward,
    valueMicroUsd,
} from "../helpers/vortex-model.js";

function auditAmount(routeId, amountIn) {
    const catalog = createCatalog();
    const quote = quoteRoute(routeId, amountIn, { catalog });
    const reserve = reserveForQuote(routeId, quote.finalOut, { catalog });
    const settlement = settlementForward(routeId, amountIn, { catalog });
    const reservedValue = valueMicroUsd(catalog, quote.sourceAsset, reserve.reserveIn, "floor");
    const creditedValue = valueMicroUsd(catalog, quote.targetAsset, settlement.finalOut, "ceil");
    return {
        amountIn: BigInt(amountIn),
        quote,
        reserve,
        settlement,
        reservedValue,
        creditedValue,
        gap: creditedValue > reservedValue ? creditedValue - reservedValue : 0n,
    };
}

function findFirstGap(routeId, start, end) {
    for (let amount = start; amount <= end; amount += 1) {
        const report = auditAmount(routeId, amount);
        if (report.gap > 0n) return report;
    }
    return null;
}

test("el primer importe rentable de vortex-loop aparece tras una unidad de dust", () => {
    const report = findFirstGap("vortex-loop", 1_000_000, 1_000_020);

    assert.ok(report);
    assert.equal(report.amountIn, 1_000_001n);
    assert.equal(report.reserve.reserveIn, 1_000_000n);
    assert.equal(report.settlement.finalOut, 1_010_000n);
    assert.equal(report.gap, 10_000n);
});

test("la ruta directa no presenta gap positivo en el rango muestreado", () => {
    for (let amount = 2_500_000; amount < 2_500_100; amount += 1) {
        const report = auditAmount("stable-direct", amount);
        assert.equal(report.gap, 0n);
    }
});

test("la superficie vulnerable depende de legs expansivos de baja precision", () => {
    const report = auditAmount("vortex-loop", 1_000_001);
    const expansiveLegs = report.settlement.legs.filter((leg, index) => {
        return leg.grossOut > report.quote.legs[index].grossOut;
    });

    assert.deepEqual(
        expansiveLegs.map((leg) => leg.id),
        ["loop-q7-q3", "loop-q3-q8", "loop-q8-q2", "loop-q2-q5", "loop-q5-usd"],
    );
});
