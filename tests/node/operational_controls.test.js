import test from "node:test";
import assert from "node:assert/strict";
import { DEFAULT_ASSETS, DEFAULT_ROUTES } from "../fixtures/scenarios.js";
import { loadJsonCommand } from "../helpers/vortex-cli.js";
import {
    ROUTE_POLICY_WARNINGS,
    checkRoutePolicy,
    collectLedgerHealth,
    createCatalog,
    createLedger,
    minOutForSlippage,
    projectVaultLiquidity,
    quoteRoute,
    settleOrder,
    VortexError,
} from "../helpers/vortex-model.js";

test("minOutForSlippage aplica descuento en basis points", () => {
    const quote = quoteRoute("stable-direct", 2_500_000);

    assert.equal(quote.finalOut, 2_311_805n);
    assert.equal(minOutForSlippage(quote.finalOut, 50), 2_300_246n);
    assert.equal(minOutForSlippage(quote.finalOut, 60), 2_297_935n);
    assert.throws(
        () => minOutForSlippage(quote.finalOut, 10_001),
        (error) => error instanceof VortexError && error.code === "VDTL_BAD_SLIPPAGE",
    );
});

test("checkRoutePolicy resume fees, rebates y utilizacion de rutas validas", () => {
    assert.deepEqual(checkRoutePolicy("stable-direct"), {
        routeId: "stable-direct",
        ok: true,
        legCount: 1n,
        totalFeeBps: 13n,
        totalRebateBps: 2n,
        maxLegFeeBps: 13n,
        maxUtilizationBps: 5300n,
        warnings: 0,
    });

    assert.deepEqual(checkRoutePolicy("rebate-hop"), {
        routeId: "rebate-hop",
        ok: true,
        legCount: 2n,
        totalFeeBps: 23n,
        totalRebateBps: 8n,
        maxLegFeeBps: 13n,
        maxUtilizationBps: 6100n,
        warnings: 0,
    });
});

test("checkRoutePolicy marca rutas operativamente sospechosas", () => {
    const brokenRoute = {
        id: "bad-route",
        label: "intentionally bad route",
        maxLegs: 2,
        maxSlippageBps: 100,
        legs: [
            {
                id: "bad-a",
                from: "vUSD",
                to: "vMISSING",
                rateNum: 0,
                rateDen: 1,
                baseFeeBps: 120,
                volatilityFeeBps: 0,
                utilizationFeeCapBps: 0,
                utilizationBps: 9500,
                rebateBps: 75,
                reserveWeightBps: 10_000,
            },
            {
                id: "bad-b",
                from: "vEUR",
                to: "vUSD",
                rateNum: 1,
                rateDen: 0,
                baseFeeBps: 1,
                volatilityFeeBps: 0,
                utilizationFeeCapBps: 0,
                utilizationBps: 0,
                rebateBps: 0,
                reserveWeightBps: 10_000,
            },
        ],
    };
    const catalog = createCatalog({
        assets: DEFAULT_ASSETS,
        routes: [...DEFAULT_ROUTES, brokenRoute],
    });
    const report = checkRoutePolicy("bad-route", { catalog });

    assert.equal(report.ok, false);
    assert.equal(report.legCount, 2n);
    assert.equal(report.totalFeeBps, 121n);
    assert.equal(report.totalRebateBps, 75n);
    assert.ok(report.warnings & ROUTE_POLICY_WARNINGS.BROKEN_LINK);
    assert.ok(report.warnings & ROUTE_POLICY_WARNINGS.HIGH_FEE);
    assert.ok(report.warnings & ROUTE_POLICY_WARNINGS.HIGH_REBATE);
    assert.ok(report.warnings & ROUTE_POLICY_WARNINGS.BAD_RATE);
    assert.ok(report.warnings & ROUTE_POLICY_WARNINGS.UNKNOWN_ASSET);
    assert.ok(report.warnings & ROUTE_POLICY_WARNINGS.HIGH_UTILIZATION);
});

test("collectLedgerHealth resume exposicion de vaults y cuentas", () => {
    const ledger = createLedger();
    settleOrder({
        ledger,
        routeId: "stable-direct",
        amountIn: 2_500_000,
        minOut: quoteRoute("stable-direct", 2_500_000).finalOut,
    });
    settleOrder({
        ledger,
        routeId: "rebate-hop",
        amountIn: 3_750_000,
        minOut: quoteRoute("rebate-hop", 3_750_000).finalOut,
    });

    const health = collectLedgerHealth(ledger);
    const usd = health.vaults.find((vault) => vault.asset === "vUSD");
    const eur = health.vaults.find((vault) => vault.asset === "vEUR");

    assert.equal(health.vaultCount, 8n);
    assert.equal(health.totalAvailableValue, 54_092_000_008_312n);
    assert.equal(health.totalPaidValue, 6_241_658n);
    assert.equal(health.totalReceivedValue, 6_249_970n);
    assert.equal(health.traderValue, 49_999_988_168n);
    assert.equal(health.makerValue, 3521n);
    assert.equal(health.protocolValue, 11_864n);
    assert.equal(health.stressedVaults, 0n);
    assert.equal(usd.receivedValue, 6_249_970n);
    assert.equal(eur.paidValue, 6_239_762n);
});

test("CLI expone policy y health report", () => {
    const policy = loadJsonCommand(["route-policy-json", "vortex-loop"]);
    const health = loadJsonCommand(["health-json"]);

    assert.equal(policy.ok, true);
    assert.equal(policy.legCount, 6);
    assert.equal(policy.totalFeeBps, 0);
    assert.equal(health.vaultCount, 8);
    assert.equal(health.totalPaidValue, 6_241_658);
    assert.equal(health.vaults[0].asset, "vUSD");
});

test("la proyección de liquidez conserva un colchón por activo", () => {
    const ledger = createLedger();
    const funded = projectVaultLiquidity(ledger, "vUSD", 1_000_000n, 500);
    assert.equal(funded.requiredBuffer, BigInt(ledger.catalog.asset("vUSD").openingLiquidity) / 20n);
    assert.equal(funded.projectedAvailable, funded.available - 1_000_000n);
    assert.equal(funded.headroom, funded.projectedAvailable - funded.requiredBuffer);
    assert.equal(funded.unfunded, 0n);
    assert.equal(funded.withinPolicy, true);

    const insufficient = projectVaultLiquidity(ledger, "vUSD", funded.available + 1n, 500);
    assert.equal(insufficient.projectedAvailable, 0n);
    assert.equal(insufficient.unfunded, 1n);
    assert.equal(insufficient.withinPolicy, false);
    assert.throws(() => projectVaultLiquidity(ledger, "vUSD", 1n, 10_001));
});

test("CLI publica una proyección JSON reproducible", () => {
    const projection = loadJsonCommand(["liquidity-json", "vUSD", "1000000", "500"]);
    assert.equal(projection.asset, "vUSD");
    assert.equal(projection.requestedPayout, 1_000_000);
    assert.equal(projection.bufferBps, 500);
    assert.equal(projection.withinPolicy, true);
});
