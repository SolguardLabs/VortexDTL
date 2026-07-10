#!/usr/bin/env node
import {
    createCatalog,
    quoteRoute,
    reserveForQuote,
    settlementForward,
    toPlain,
    valueMicroUsd,
} from "../tests/helpers/vortex-model.js";

function parseInteger(value, fallback, label) {
    if (value === undefined) return fallback;
    if (!/^[0-9]+$/.test(value)) throw new Error(`${label} must be an unsigned integer`);
    return Number(value);
}

function audit(catalog, routeId, amountIn) {
    const quote = quoteRoute(routeId, amountIn, { catalog });
    const reserve = reserveForQuote(routeId, quote.finalOut, { catalog });
    const settlement = settlementForward(routeId, amountIn, { catalog });
    const reservedValue = valueMicroUsd(catalog, quote.sourceAsset, reserve.reserveIn, "floor");
    const creditedValue = valueMicroUsd(
        catalog,
        quote.targetAsset,
        settlement.finalOut,
        "ceil",
    );
    const gapValue = creditedValue > reservedValue ? creditedValue - reservedValue : 0n;
    const expansiveLegs = settlement.legs
        .filter((leg, index) => leg.grossOut > quote.legs[index].grossOut)
        .map((leg) => leg.id);

    return {
        amountIn: BigInt(amountIn),
        quoteOut: quote.finalOut,
        reserveIn: reserve.reserveIn,
        settlementOut: settlement.finalOut,
        reservedValue,
        creditedValue,
        gapValue,
        expansiveLegs,
    };
}

function insertTop(top, report, limit) {
    if (report.gapValue === 0n) return;
    top.push(report);
    top.sort((a, b) => {
        if (a.gapValue === b.gapValue) return Number(a.amountIn - b.amountIn);
        return a.gapValue > b.gapValue ? -1 : 1;
    });
    if (top.length > limit) top.pop();
}

try {
    const [routeArg, startArg, endArg, stepArg, limitArg] = process.argv.slice(2);
    const routeId = routeArg ?? "vortex-loop";
    const start = parseInteger(startArg, 1_000_000, "start");
    const end = parseInteger(endArg, start + 1000, "end");
    const step = parseInteger(stepArg, 1, "step");
    const limit = parseInteger(limitArg, 5, "limit");
    if (step <= 0 || end < start) throw new Error("invalid scan range");

    const catalog = createCatalog();
    const top = [];
    let firstGap = null;
    let scanned = 0;

    for (let amount = start; amount <= end; amount += step) {
        const report = audit(catalog, routeId, amount);
        scanned += 1;
        if (!firstGap && report.gapValue > 0n) firstGap = report;
        insertTop(top, report, limit);
    }

    console.log(
        JSON.stringify(
            toPlain({
                routeId,
                start,
                end,
                step,
                scanned,
                firstGap,
                top,
            }),
        ),
    );
} catch (error) {
    console.error(error.message);
    process.exit(1);
}
