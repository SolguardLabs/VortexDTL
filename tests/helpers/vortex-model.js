import { COMMON_DECIMALS, DEFAULT_ASSETS, DEFAULT_ROUTES } from "../fixtures/scenarios.js";

const MAX_SAFE_BIGINT = BigInt(Number.MAX_SAFE_INTEGER);
const pow10Cache = new Map([[0, 1n]]);

export class VortexError extends Error {
    constructor(code, message, details = {}) {
        super(message);
        this.name = "VortexError";
        this.code = code;
        this.details = details;
    }
}

export function asBigInt(value) {
    if (typeof value === "bigint") return value;
    if (typeof value === "number") {
        if (!Number.isSafeInteger(value) || value < 0) {
            throw new VortexError("VDTL_BAD_AMOUNT", "amount must be a safe positive integer", {
                value,
            });
        }
        return BigInt(value);
    }
    if (typeof value === "string" && /^[0-9]+$/.test(value)) return BigInt(value);
    throw new VortexError("VDTL_BAD_AMOUNT", "amount must be an unsigned integer", { value });
}

export function pow10(exp) {
    if (!Number.isInteger(exp) || exp < 0 || exp > 18) {
        throw new VortexError("VDTL_BAD_DECIMALS", "decimal exponent out of range", { exp });
    }
    if (!pow10Cache.has(exp)) {
        let value = 1n;
        for (let i = 0; i < exp; i += 1) value *= 10n;
        pow10Cache.set(exp, value);
    }
    return pow10Cache.get(exp);
}

export function divRound(numerator, denominator, mode = "floor") {
    const n = asBigInt(numerator);
    const d = asBigInt(denominator);
    if (d === 0n) throw new VortexError("VDTL_DIV_ZERO", "division by zero");
    const q = n / d;
    const r = n % d;
    if (r === 0n || mode === "floor") return q;
    if (mode === "ceil") return q + 1n;
    if (mode === "half-up") return r * 2n >= d ? q + 1n : q;
    throw new VortexError("VDTL_BAD_ROUNDING", "unsupported rounding mode", { mode });
}

export function scaleAmount(amount, fromDecimals, toDecimals, mode = "floor") {
    const value = asBigInt(amount);
    if (fromDecimals === toDecimals) return value;
    if (fromDecimals < toDecimals) {
        return value * pow10(toDecimals - fromDecimals);
    }
    return divRound(value, pow10(fromDecimals - toDecimals), mode);
}

export function bpsAmount(amount, bps, mode = "floor") {
    if (!Number.isInteger(bps) || bps < 0 || bps > 10_000) {
        throw new VortexError("VDTL_BAD_BPS", "basis points out of range", { bps });
    }
    return divRound(asBigInt(amount) * BigInt(bps), 10_000n, mode);
}

export function dynamicFeeBps(leg) {
    const utilizationFee = Math.floor(
        (leg.utilizationBps * leg.utilizationFeeCapBps) / 10_000,
    );
    return leg.baseFeeBps + leg.volatilityFeeBps + utilizationFee;
}

export const ROUTE_POLICY_WARNINGS = Object.freeze({
    BROKEN_LINK: 1,
    EMPTY_ROUTE: 2,
    HIGH_FEE: 4,
    HIGH_REBATE: 8,
    BAD_RATE: 16,
    UNKNOWN_ASSET: 32,
    HIGH_UTILIZATION: 64,
});

export function minOutForSlippage(quoteOut, slippageBps) {
    if (!Number.isInteger(slippageBps) || slippageBps < 0 || slippageBps > 10_000) {
        throw new VortexError("VDTL_BAD_SLIPPAGE", "slippage basis points out of range", {
            slippageBps,
        });
    }
    const output = asBigInt(quoteOut);
    return output - bpsAmount(output, slippageBps, "floor");
}

export function createCatalog({
    assets = DEFAULT_ASSETS,
    routes = DEFAULT_ROUTES,
} = {}) {
    const assetBySymbol = new Map(assets.map((asset) => [asset.symbol, { ...asset }]));
    const routeById = new Map(
        routes.map((route) => [
            route.id,
            {
                ...route,
                legs: route.legs.map((leg) => ({ ...leg })),
            },
        ]),
    );
    return {
        assets: assets.map((asset) => ({ ...asset })),
        routes: routes.map((route) => ({ ...route, legs: route.legs.map((leg) => ({ ...leg })) })),
        asset(symbol) {
            const asset = assetBySymbol.get(symbol);
            if (!asset) throw new VortexError("VDTL_UNKNOWN_ASSET", "asset not found", { symbol });
            return asset;
        },
        route(id) {
            const route = routeById.get(id);
            if (!route) throw new VortexError("VDTL_UNKNOWN_ROUTE", "route not found", { id });
            return route;
        },
    };
}

export function checkRoutePolicy(routeId, options = {}) {
    const catalog = options.catalog ?? createCatalog();
    const route = catalog.route(routeId);
    let warnings = 0;
    let totalFeeBps = 0;
    let totalRebateBps = 0;
    let maxLegFeeBps = 0;
    let maxUtilizationBps = 0;

    if (route.legs.length === 0) warnings |= ROUTE_POLICY_WARNINGS.EMPTY_ROUTE;
    for (let index = 0; index < route.legs.length; index += 1) {
        const leg = route.legs[index];
        const feeBps = dynamicFeeBps(leg);
        if (leg.rateNum === 0 || leg.rateDen === 0) warnings |= ROUTE_POLICY_WARNINGS.BAD_RATE;
        try {
            catalog.asset(leg.from);
            catalog.asset(leg.to);
        } catch {
            warnings |= ROUTE_POLICY_WARNINGS.UNKNOWN_ASSET;
        }
        if (route.legs[index + 1] && leg.to !== route.legs[index + 1].from) {
            warnings |= ROUTE_POLICY_WARNINGS.BROKEN_LINK;
        }
        if (feeBps > 100) warnings |= ROUTE_POLICY_WARNINGS.HIGH_FEE;
        if (leg.rebateBps > 50) warnings |= ROUTE_POLICY_WARNINGS.HIGH_REBATE;
        if (leg.utilizationBps > 9000) warnings |= ROUTE_POLICY_WARNINGS.HIGH_UTILIZATION;

        totalFeeBps += feeBps;
        totalRebateBps += leg.rebateBps;
        maxLegFeeBps = Math.max(maxLegFeeBps, feeBps);
        maxUtilizationBps = Math.max(maxUtilizationBps, leg.utilizationBps);
    }

    return {
        routeId,
        ok: warnings === 0,
        legCount: BigInt(route.legs.length),
        totalFeeBps: BigInt(totalFeeBps),
        totalRebateBps: BigInt(totalRebateBps),
        maxLegFeeBps: BigInt(maxLegFeeBps),
        maxUtilizationBps: BigInt(maxUtilizationBps),
        warnings,
    };
}

export function quoteLeg(catalog, leg, amountIn) {
    const source = catalog.asset(leg.from);
    const target = catalog.asset(leg.to);
    const input = asBigInt(amountIn);
    const commonIn = scaleAmount(input, source.decimals, COMMON_DECIMALS, "floor");
    const ratedCommon = divRound(commonIn * BigInt(leg.rateNum), BigInt(leg.rateDen), "floor");
    const grossOut = scaleAmount(ratedCommon, COMMON_DECIMALS, target.decimals, "floor");
    const feeBps = dynamicFeeBps(leg);
    const fee = bpsAmount(grossOut, feeBps, "floor");
    const rebate = bpsAmount(grossOut, leg.rebateBps, "floor");
    const netOut = grossOut - fee;

    return {
        id: leg.id,
        from: source.symbol,
        to: target.symbol,
        input,
        commonIn,
        ratedCommon,
        grossOut,
        feeBps,
        fee,
        rebate,
        netOut,
        rounding: "floor",
    };
}

export function quoteRoute(routeId, amountIn, options = {}) {
    const catalog = options.catalog ?? createCatalog();
    const route = catalog.route(routeId);
    let amount = asBigInt(amountIn);
    const legs = [];
    const feeByAsset = {};
    const rebateByAsset = {};

    for (const leg of route.legs) {
        const result = quoteLeg(catalog, leg, amount);
        legs.push(result);
        feeByAsset[result.to] = (feeByAsset[result.to] ?? 0n) + result.fee;
        rebateByAsset[result.to] = (rebateByAsset[result.to] ?? 0n) + result.rebate;
        amount = result.netOut;
    }

    const first = route.legs[0];
    const last = route.legs[route.legs.length - 1];
    return {
        routeId,
        sourceAsset: first.from,
        targetAsset: last.to,
        amountIn: asBigInt(amountIn),
        finalOut: amount,
        legs,
        feeByAsset,
        rebateByAsset,
        phase: "quote",
    };
}

export function reserveForQuote(routeId, finalOut, options = {}) {
    const catalog = options.catalog ?? createCatalog();
    const route = catalog.route(routeId);
    let required = asBigInt(finalOut);
    const reverseLegs = [];

    for (let i = route.legs.length - 1; i >= 0; i -= 1) {
        const leg = route.legs[i];
        const source = catalog.asset(leg.from);
        const target = catalog.asset(leg.to);
        const feeBps = dynamicFeeBps(leg);
        const feePad = bpsAmount(required, feeBps, "floor");
        const grossEstimate = required + feePad;
        const targetCommon = scaleAmount(
            grossEstimate,
            target.decimals,
            COMMON_DECIMALS,
            "floor",
        );
        const sourceCommon = divRound(
            targetCommon * BigInt(leg.rateDen),
            BigInt(leg.rateNum),
            "floor",
        );
        const sourceRaw = scaleAmount(sourceCommon, COMMON_DECIMALS, source.decimals, "floor");

        reverseLegs.unshift({
            id: leg.id,
            from: source.symbol,
            to: target.symbol,
            requiredOut: required,
            feeBps,
            feePad,
            grossEstimate,
            sourceRaw,
            rounding: "floor",
        });
        required = sourceRaw;
    }

    return {
        routeId,
        reserveIn: required,
        finalOut: asBigInt(finalOut),
        legs: reverseLegs,
        phase: "reserve",
    };
}

export function settlementLeg(catalog, leg, amountIn) {
    const source = catalog.asset(leg.from);
    const target = catalog.asset(leg.to);
    const input = asBigInt(amountIn);
    const commonIn = scaleAmount(input, source.decimals, COMMON_DECIMALS, "ceil");
    const ratedCommon = divRound(commonIn * BigInt(leg.rateNum), BigInt(leg.rateDen), "ceil");
    const grossOut = scaleAmount(ratedCommon, COMMON_DECIMALS, target.decimals, "ceil");
    const feeBps = dynamicFeeBps(leg);
    const fee = bpsAmount(grossOut, feeBps, "floor");
    const rebate = bpsAmount(grossOut, leg.rebateBps, "ceil");
    const netOut = grossOut - fee;

    return {
        id: leg.id,
        from: source.symbol,
        to: target.symbol,
        input,
        commonIn,
        ratedCommon,
        grossOut,
        feeBps,
        fee,
        rebate,
        netOut,
        rounding: "ceil",
    };
}

export function settlementForward(routeId, amountIn, options = {}) {
    const catalog = options.catalog ?? createCatalog();
    const route = catalog.route(routeId);
    let amount = asBigInt(amountIn);
    const legs = [];
    const feeByAsset = {};
    const rebateByAsset = {};

    for (const leg of route.legs) {
        const result = settlementLeg(catalog, leg, amount);
        legs.push(result);
        feeByAsset[result.to] = (feeByAsset[result.to] ?? 0n) + result.fee;
        rebateByAsset[result.to] = (rebateByAsset[result.to] ?? 0n) + result.rebate;
        amount = result.netOut;
    }

    const first = route.legs[0];
    const last = route.legs[route.legs.length - 1];
    return {
        routeId,
        sourceAsset: first.from,
        targetAsset: last.to,
        amountIn: asBigInt(amountIn),
        finalOut: amount,
        legs,
        feeByAsset,
        rebateByAsset,
        phase: "settlement",
    };
}

export function valueMicroUsd(catalog, symbol, amount, mode = "floor") {
    const asset = catalog.asset(symbol);
    const raw = asBigInt(amount) * BigInt(asset.priceMicroUsd);
    return divRound(raw, pow10(asset.decimals), mode);
}

export function accountValueMicroUsd(catalog, balances) {
    let total = 0n;
    for (const asset of catalog.assets) {
        total += valueMicroUsd(catalog, asset.symbol, balances[asset.symbol] ?? 0n, "ceil");
    }
    return total;
}

function emptyBalances(catalog) {
    const balances = {};
    for (const asset of catalog.assets) balances[asset.symbol] = 0n;
    return balances;
}

export function createLedger(options = {}) {
    const catalog = options.catalog ?? createCatalog();
    const trader = emptyBalances(catalog);
    const maker = emptyBalances(catalog);
    const protocol = emptyBalances(catalog);
    const vaults = {};

    for (const asset of catalog.assets) {
        trader[asset.symbol] = BigInt(asset.openingUserBalance);
        vaults[asset.symbol] = {
            asset: asset.symbol,
            available: BigInt(asset.openingLiquidity),
            reserved: 0n,
            paid: 0n,
            received: 0n,
        };
    }

    return {
        catalog,
        accounts: {
            trader,
            maker,
            protocol,
        },
        vaults,
        events: [],
        totals: {
            reservedValue: 0n,
            creditedValue: 0n,
            orders: 0n,
        },
    };
}

export function collectLedgerHealth(ledger) {
    if (!ledger) throw new VortexError("VDTL_NO_LEDGER", "ledger is required");
    const vaults = [];
    let totalAvailableValue = 0n;
    let totalPaidValue = 0n;
    let totalReceivedValue = 0n;
    let stressedVaults = 0n;

    for (const asset of ledger.catalog.assets) {
        const vault = ledger.vaults[asset.symbol];
        const availableValue = valueMicroUsd(ledger.catalog, asset.symbol, vault.available, "floor");
        const paidValue = valueMicroUsd(ledger.catalog, asset.symbol, vault.paid, "ceil");
        const receivedValue = valueMicroUsd(ledger.catalog, asset.symbol, vault.received, "floor");
        const observedLiquidity = vault.available + vault.paid;
        const utilizationBps =
            observedLiquidity === 0n
                ? 0n
                : divRound(vault.paid * 10_000n, observedLiquidity, "floor");

        if (utilizationBps > 7500n) stressedVaults += 1n;
        totalAvailableValue += availableValue;
        totalPaidValue += paidValue;
        totalReceivedValue += receivedValue;
        vaults.push({
            asset: asset.symbol,
            available: vault.available,
            paid: vault.paid,
            received: vault.received,
            availableValue,
            paidValue,
            receivedValue,
            utilizationBps,
        });
    }

    return {
        vaultCount: BigInt(vaults.length),
        totalAvailableValue,
        totalPaidValue,
        totalReceivedValue,
        traderValue: accountValueMicroUsd(ledger.catalog, ledger.accounts.trader),
        makerValue: accountValueMicroUsd(ledger.catalog, ledger.accounts.maker),
        protocolValue: accountValueMicroUsd(ledger.catalog, ledger.accounts.protocol),
        stressedVaults,
        vaults,
    };
}

export function projectVaultLiquidity(ledger, asset, payout, bufferBps = 500) {
    if (!ledger) throw new VortexError("VDTL_NO_LEDGER", "ledger is required");
    if (!Number.isInteger(bufferBps) || bufferBps < 0 || bufferBps > 10_000) {
        throw new VortexError("VDTL_BAD_BPS", "buffer basis points out of range");
    }
    const config = ledger.catalog.asset(asset);
    const vault = ledger.vaults[asset];
    const requestedPayout = asBigInt(payout);
    const requiredBuffer = bpsAmount(config.openingLiquidity, bufferBps, "ceil");
    const unfunded = requestedPayout > vault.available ? requestedPayout - vault.available : 0n;
    const projectedAvailable = requestedPayout > vault.available ? 0n : vault.available - requestedPayout;
    const headroom = projectedAvailable > requiredBuffer ? projectedAvailable - requiredBuffer : 0n;
    return {
        asset,
        available: vault.available,
        requestedPayout,
        bufferBps: BigInt(bufferBps),
        requiredBuffer,
        projectedAvailable,
        headroom,
        unfunded,
        withinPolicy: unfunded === 0n && projectedAvailable >= requiredBuffer,
    };
}

function addBalance(balances, asset, amount) {
    balances[asset] = (balances[asset] ?? 0n) + asBigInt(amount);
}

function subBalance(balances, asset, amount, code = "VDTL_INSUFFICIENT_BALANCE") {
    const value = asBigInt(amount);
    const current = balances[asset] ?? 0n;
    if (current < value) {
        throw new VortexError(code, "insufficient balance", { asset, current, requested: value });
    }
    balances[asset] = current - value;
}

export function settleOrder({
    ledger,
    routeId,
    amountIn,
    minOut = 0,
    trader = "trader",
    maker = "maker",
} = {}) {
    if (!ledger) throw new VortexError("VDTL_NO_LEDGER", "ledger is required");
    const quote = quoteRoute(routeId, amountIn, { catalog: ledger.catalog });
    const minimum = asBigInt(minOut);
    if (quote.finalOut < minimum) {
        throw new VortexError("VDTL_SLIPPAGE", "minimum output is above preliminary quote", {
            routeId,
            quoted: quote.finalOut,
            minOut: minimum,
        });
    }

    const reserve = reserveForQuote(routeId, quote.finalOut, { catalog: ledger.catalog });
    const settlement = settlementForward(routeId, amountIn, { catalog: ledger.catalog });
    const sourceAsset = quote.sourceAsset;
    const targetAsset = quote.targetAsset;
    const sourceVault = ledger.vaults[sourceAsset];
    const targetVault = ledger.vaults[targetAsset];
    const traderBalances = ledger.accounts[trader];
    const makerBalances = ledger.accounts[maker];
    const protocolBalances = ledger.accounts.protocol;

    subBalance(traderBalances, sourceAsset, reserve.reserveIn);
    addBalance(traderBalances, targetAsset, settlement.finalOut);

    sourceVault.available += reserve.reserveIn;
    sourceVault.received += reserve.reserveIn;
    if (targetVault.available < settlement.finalOut) {
        throw new VortexError("VDTL_VAULT_DRAINED", "target vault cannot pay settlement", {
            asset: targetAsset,
            available: targetVault.available,
            requested: settlement.finalOut,
        });
    }
    targetVault.available -= settlement.finalOut;
    targetVault.paid += settlement.finalOut;

    let rebateValue = 0n;
    for (const [asset, rebate] of Object.entries(settlement.rebateByAsset)) {
        if (rebate === 0n) continue;
        const vault = ledger.vaults[asset];
        if (vault.available < rebate) {
            throw new VortexError("VDTL_REBATE_DRAINED", "rebate vault cannot pay maker", {
                asset,
                available: vault.available,
                requested: rebate,
            });
        }
        vault.available -= rebate;
        vault.paid += rebate;
        addBalance(makerBalances, asset, rebate);
        rebateValue += valueMicroUsd(ledger.catalog, asset, rebate, "ceil");
    }

    for (const [asset, fee] of Object.entries(settlement.feeByAsset)) {
        addBalance(protocolBalances, asset, fee);
    }

    const reservedValue = valueMicroUsd(ledger.catalog, sourceAsset, reserve.reserveIn, "floor");
    const creditedValue =
        valueMicroUsd(ledger.catalog, targetAsset, settlement.finalOut, "ceil") + rebateValue;
    ledger.totals.reservedValue += reservedValue;
    ledger.totals.creditedValue += creditedValue;
    ledger.totals.orders += 1n;

    const event = {
        index: ledger.events.length,
        routeId,
        sourceAsset,
        targetAsset,
        amountIn: asBigInt(amountIn),
        minOut: minimum,
        quote,
        reserve,
        settlement,
        reservedValue,
        creditedValue,
    };
    ledger.events.push(event);
    return event;
}

export function toPlain(value) {
    if (typeof value === "bigint") {
        return value <= MAX_SAFE_BIGINT ? Number(value) : value.toString();
    }
    if (Array.isArray(value)) return value.map((item) => toPlain(item));
    if (value && typeof value === "object") {
        const out = {};
        for (const [key, child] of Object.entries(value)) out[key] = toPlain(child);
        return out;
    }
    return value;
}

export function compactQuote(quote) {
    return toPlain({
        routeId: quote.routeId,
        sourceAsset: quote.sourceAsset,
        targetAsset: quote.targetAsset,
        amountIn: quote.amountIn,
        finalOut: quote.finalOut,
        legs: quote.legs.map((leg) => ({
            id: leg.id,
            from: leg.from,
            to: leg.to,
            input: leg.input,
            grossOut: leg.grossOut,
            feeBps: leg.feeBps,
            fee: leg.fee,
            rebate: leg.rebate,
            netOut: leg.netOut,
            rounding: leg.rounding,
        })),
        feeByAsset: quote.feeByAsset,
        rebateByAsset: quote.rebateByAsset,
    });
}

export function compactSettlement(event) {
    return toPlain({
        routeId: event.routeId,
        sourceAsset: event.sourceAsset,
        targetAsset: event.targetAsset,
        amountIn: event.amountIn,
        minOut: event.minOut,
        quoteOut: event.quote.finalOut,
        reserveIn: event.reserve.reserveIn,
        finalOut: event.settlement.finalOut,
        reservedValue: event.reservedValue,
        creditedValue: event.creditedValue,
        fees: event.settlement.feeByAsset,
        rebates: event.settlement.rebateByAsset,
    });
}

export function compactLedger(ledger) {
    return toPlain({
        accounts: ledger.accounts,
        vaults: ledger.vaults,
        totals: ledger.totals,
        events: ledger.events.length,
    });
}

export function compactRoutePolicy(report) {
    return toPlain(report);
}

export function compactLedgerHealth(health) {
    return toPlain(health);
}
