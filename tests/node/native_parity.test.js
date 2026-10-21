import test from "node:test";
import assert from "node:assert/strict";
import { hasNativeBinary, runVortexCli } from "../helpers/vortex-cli.js";
import { compactQuote, createLedger, projectVaultLiquidity, quoteRoute, toPlain } from "../helpers/vortex-model.js";

test("el binario C coincide con el cliente en cotización directa", { skip: !hasNativeBinary() }, () => {
    const native = JSON.parse(runVortexCli(["quote-json", "stable-direct", "2500000"]));
    const reference = compactQuote(quoteRoute("stable-direct", 2_500_000n));
    assert.deepEqual(native, reference);
});

test("el binario C coincide con el cliente en proyección de colchón", { skip: !hasNativeBinary() }, () => {
    const native = JSON.parse(runVortexCli(["liquidity-json", "vUSD", "1000000", "500"]));
    const reference = toPlain(projectVaultLiquidity(createLedger(), "vUSD", 1_000_000n, 500));
    assert.deepEqual(native, reference);
});
