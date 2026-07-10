#!/usr/bin/env node
import {
    checkRoutePolicy,
    compactLedger,
    compactLedgerHealth,
    compactQuote,
    compactRoutePolicy,
    compactSettlement,
    collectLedgerHealth,
    createLedger,
    quoteRoute,
    runExploit,
    settleOrder,
    toPlain,
} from "../tests/helpers/vortex-model.js";

function usage() {
    return [
        "usage:",
        "  node scripts/vortex-js.mjs demo-json",
        "  node scripts/vortex-js.mjs quote-json <route> <amount>",
        "  node scripts/vortex-js.mjs settle-json <route> <amount> [minOut]",
        "  node scripts/vortex-js.mjs exploit-json [iterations] [amount]",
        "  node scripts/vortex-js.mjs route-policy-json <route>",
        "  node scripts/vortex-js.mjs health-json",
    ].join("\n");
}

function parseAmount(value, fallback) {
    if (value === undefined) return fallback;
    if (!/^[0-9]+$/.test(value)) throw new Error(`invalid amount: ${value}`);
    return BigInt(value);
}

const [command, routeArg, amountArg, minOutArg] = process.argv.slice(2);

try {
    if (!command || command === "help" || command === "--help") {
        console.log(usage());
        process.exit(0);
    }

    if (command === "quote-json") {
        const route = routeArg ?? "stable-direct";
        const amount = parseAmount(amountArg, 2_500_000n);
        console.log(JSON.stringify(compactQuote(quoteRoute(route, amount))));
        process.exit(0);
    }

    if (command === "settle-json") {
        const route = routeArg ?? "stable-direct";
        const amount = parseAmount(amountArg, 2_500_000n);
        const quote = quoteRoute(route, amount);
        const minOut = parseAmount(minOutArg, quote.finalOut);
        const ledger = createLedger();
        const event = settleOrder({ ledger, routeId: route, amountIn: amount, minOut });
        console.log(
            JSON.stringify({
                settlement: compactSettlement(event),
                ledger: compactLedger(ledger),
            }),
        );
        process.exit(0);
    }

    if (command === "demo-json") {
        const ledger = createLedger();
        const direct = settleOrder({
            ledger,
            routeId: "stable-direct",
            amountIn: 2_500_000n,
            minOut: quoteRoute("stable-direct", 2_500_000n).finalOut,
        });
        const hop = settleOrder({
            ledger,
            routeId: "rebate-hop",
            amountIn: 3_750_000n,
            minOut: quoteRoute("rebate-hop", 3_750_000n).finalOut,
        });
        console.log(
            JSON.stringify({
                scenario: "demo",
                orders: 2,
                direct: compactSettlement(direct),
                hop: compactSettlement(hop),
                ledger: compactLedger(ledger),
            }),
        );
        process.exit(0);
    }

    if (command === "exploit-json") {
        const iterations = Number(routeArg ?? 64);
        const amount = parseAmount(amountArg, 1_000_001n);
        const result = runExploit({ iterations, amountIn: amount });
        console.log(
            JSON.stringify(
                toPlain({
                    scenario: "exploit",
                    routeId: result.routeId,
                    amountIn: result.amountIn,
                    iterations: result.iterations,
                    quoteOut: result.quote.finalOut,
                    first: compactSettlement(result.firstSettlement),
                    last: compactSettlement(result.lastSettlement),
                    ledger: compactLedger(result.ledger),
                }),
            ),
        );
        process.exit(0);
    }

    if (command === "route-policy-json") {
        const route = routeArg ?? "stable-direct";
        console.log(JSON.stringify(compactRoutePolicy(checkRoutePolicy(route))));
        process.exit(0);
    }

    if (command === "health-json") {
        const ledger = createLedger();
        settleOrder({
            ledger,
            routeId: "stable-direct",
            amountIn: 2_500_000n,
            minOut: quoteRoute("stable-direct", 2_500_000n).finalOut,
        });
        settleOrder({
            ledger,
            routeId: "rebate-hop",
            amountIn: 3_750_000n,
            minOut: quoteRoute("rebate-hop", 3_750_000n).finalOut,
        });
        console.log(JSON.stringify(compactLedgerHealth(collectLedgerHealth(ledger))));
        process.exit(0);
    }

    throw new Error(`unknown command: ${command}\n${usage()}`);
} catch (error) {
    console.error(error.message);
    process.exit(1);
}
