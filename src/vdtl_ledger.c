#include "vortexdtl.h"

#include <string.h>

static void copy_text(char *dst, size_t dst_len, const char *src) {
    if (dst_len == 0u) return;
    if (src == NULL) src = "";
    (void)snprintf(dst, dst_len, "%s", src);
}

static vdtl_status add_to(uint64_t *slot, uint64_t amount) {
    return vdtl_u64_add(*slot, amount, slot);
}

static vdtl_status sub_from(uint64_t *slot, uint64_t amount) {
    return vdtl_u64_sub(*slot, amount, slot);
}

vdtl_status vdtl_ledger_init_default(vdtl_ledger *ledger) {
    size_t i;
    if (ledger == NULL) return VDTL_ERR_NULL;
    memset(ledger, 0, sizeof(*ledger));
    vdtl_context_init_default(&ledger->ctx);
    for (i = 0u; i < ledger->ctx.asset_count; i++) {
        const vdtl_asset *asset = &ledger->ctx.assets[i];
        copy_text(ledger->vaults[i].asset, sizeof(ledger->vaults[i].asset), asset->symbol);
        ledger->vaults[i].available = asset->opening_liquidity;
        ledger->accounts[VDTL_ACCOUNT_TRADER][i] = asset->opening_user_balance;
    }
    return VDTL_OK;
}

static vdtl_status pay_rebates(
    vdtl_ledger *ledger,
    const vdtl_quote *settlement,
    uint64_t *rebate_value) {
    size_t i;
    vdtl_status status;
    uint64_t total_value = 0u;
    if (ledger == NULL || settlement == NULL || rebate_value == NULL) return VDTL_ERR_NULL;
    for (i = 0u; i < ledger->ctx.asset_count; i++) {
        uint64_t rebate = settlement->rebate_by_asset[i];
        uint64_t value;
        if (rebate == 0u) continue;
        if (ledger->vaults[i].available < rebate) return VDTL_ERR_VAULT;
        status = sub_from(&ledger->vaults[i].available, rebate);
        if (status != VDTL_OK) return status;
        status = add_to(&ledger->vaults[i].paid, rebate);
        if (status != VDTL_OK) return status;
        status = add_to(&ledger->accounts[VDTL_ACCOUNT_MAKER][i], rebate);
        if (status != VDTL_OK) return status;
        status = vdtl_value_micro_usd(&ledger->ctx, ledger->ctx.assets[i].symbol,
                                      rebate, VDTL_ROUND_CEIL, &value);
        if (status != VDTL_OK) return status;
        status = vdtl_u64_add(total_value, value, &total_value);
        if (status != VDTL_OK) return status;
    }
    *rebate_value = total_value;
    return VDTL_OK;
}

static vdtl_status accrue_fees(vdtl_ledger *ledger, const vdtl_quote *settlement) {
    size_t i;
    vdtl_status status;
    if (ledger == NULL || settlement == NULL) return VDTL_ERR_NULL;
    for (i = 0u; i < ledger->ctx.asset_count; i++) {
        uint64_t fee = settlement->fee_by_asset[i];
        if (fee == 0u) continue;
        status = add_to(&ledger->accounts[VDTL_ACCOUNT_PROTOCOL][i], fee);
        if (status != VDTL_OK) return status;
    }
    return VDTL_OK;
}

vdtl_status vdtl_settle_order(
    vdtl_ledger *ledger,
    const char *route_id,
    uint64_t amount_in,
    uint64_t min_out,
    vdtl_settlement_event *out) {
    vdtl_quote quote;
    vdtl_quote settlement;
    vdtl_reserve reserve;
    int source_index;
    int target_index;
    uint64_t reserved_value;
    uint64_t credited_value;
    uint64_t rebate_value;
    uint64_t target_value;
    vdtl_status status;

    if (ledger == NULL || route_id == NULL || out == NULL) return VDTL_ERR_NULL;
    status = vdtl_quote_route(&ledger->ctx, route_id, amount_in, &quote);
    if (status != VDTL_OK) return status;
    if (quote.final_out < min_out) return VDTL_ERR_SLIPPAGE;

    status = vdtl_reserve_for_quote(&ledger->ctx, route_id, quote.final_out, &reserve);
    if (status != VDTL_OK) return status;
    status = vdtl_settlement_forward(&ledger->ctx, route_id, amount_in, &settlement);
    if (status != VDTL_OK) return status;

    source_index = vdtl_asset_index(&ledger->ctx, quote.source_asset);
    target_index = vdtl_asset_index(&ledger->ctx, quote.target_asset);
    if (source_index < 0 || target_index < 0) return VDTL_ERR_NOT_FOUND;
    if (ledger->accounts[VDTL_ACCOUNT_TRADER][source_index] < reserve.reserve_in) {
        return VDTL_ERR_BALANCE;
    }

    status = sub_from(&ledger->accounts[VDTL_ACCOUNT_TRADER][source_index], reserve.reserve_in);
    if (status != VDTL_OK) return status;
    status = add_to(&ledger->accounts[VDTL_ACCOUNT_TRADER][target_index], settlement.final_out);
    if (status != VDTL_OK) return status;
    status = add_to(&ledger->vaults[source_index].available, reserve.reserve_in);
    if (status != VDTL_OK) return status;
    status = add_to(&ledger->vaults[source_index].received, reserve.reserve_in);
    if (status != VDTL_OK) return status;
    if (ledger->vaults[target_index].available < settlement.final_out) return VDTL_ERR_VAULT;
    status = sub_from(&ledger->vaults[target_index].available, settlement.final_out);
    if (status != VDTL_OK) return status;
    status = add_to(&ledger->vaults[target_index].paid, settlement.final_out);
    if (status != VDTL_OK) return status;

    status = pay_rebates(ledger, &settlement, &rebate_value);
    if (status != VDTL_OK) return status;
    status = accrue_fees(ledger, &settlement);
    if (status != VDTL_OK) return status;

    status = vdtl_value_micro_usd(&ledger->ctx, quote.source_asset,
                                  reserve.reserve_in, VDTL_ROUND_FLOOR,
                                  &reserved_value);
    if (status != VDTL_OK) return status;
    status = vdtl_value_micro_usd(&ledger->ctx, quote.target_asset,
                                  settlement.final_out, VDTL_ROUND_CEIL,
                                  &target_value);
    if (status != VDTL_OK) return status;
    status = vdtl_u64_add(target_value, rebate_value, &credited_value);
    if (status != VDTL_OK) return status;

    ledger->totals.reserved_value += reserved_value;
    ledger->totals.credited_value += credited_value;
    if (credited_value > reserved_value) {
        ledger->totals.extracted_value += credited_value - reserved_value;
    }
    ledger->totals.orders += 1u;
    ledger->events += 1u;

    memset(out, 0, sizeof(*out));
    copy_text(out->route_id, sizeof(out->route_id), route_id);
    copy_text(out->source_asset, sizeof(out->source_asset), quote.source_asset);
    copy_text(out->target_asset, sizeof(out->target_asset), quote.target_asset);
    out->amount_in = amount_in;
    out->min_out = min_out;
    out->quote = quote;
    out->reserve = reserve;
    out->settlement = settlement;
    out->reserved_value = reserved_value;
    out->credited_value = credited_value;
    out->extracted_value = credited_value > reserved_value ? credited_value - reserved_value : 0u;
    return VDTL_OK;
}
