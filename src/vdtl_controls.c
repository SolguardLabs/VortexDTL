#include "vortexdtl.h"

#include <string.h>

#define VDTL_POLICY_WARN_BROKEN_LINK 1u
#define VDTL_POLICY_WARN_EMPTY_ROUTE 2u
#define VDTL_POLICY_WARN_HIGH_FEE 4u
#define VDTL_POLICY_WARN_HIGH_REBATE 8u
#define VDTL_POLICY_WARN_BAD_RATE 16u
#define VDTL_POLICY_WARN_UNKNOWN_ASSET 32u
#define VDTL_POLICY_WARN_HIGH_UTILIZATION 64u

static void copy_text(char *dst, size_t dst_len, const char *src) {
    if (dst_len == 0u) return;
    if (src == NULL) src = "";
    (void)snprintf(dst, dst_len, "%s", src);
}

static vdtl_status add_value(uint64_t *slot, uint64_t value) {
    return vdtl_u64_add(*slot, value, slot);
}

vdtl_status vdtl_min_out_for_slippage(
    uint64_t quote_out,
    uint32_t slippage_bps,
    uint64_t *out) {
    uint64_t discount;
    vdtl_status status;
    if (out == NULL) return VDTL_ERR_NULL;
    if (slippage_bps > 10000u) return VDTL_ERR_RANGE;
    status = vdtl_bps_amount(quote_out, slippage_bps, VDTL_ROUND_FLOOR, &discount);
    if (status != VDTL_OK) return status;
    return vdtl_u64_sub(quote_out, discount, out);
}

vdtl_status vdtl_check_route_policy(
    const vdtl_context *ctx,
    const char *route_id,
    vdtl_route_policy_report *out) {
    const vdtl_route *route;
    size_t i;
    uint64_t total_fee = 0u;
    uint64_t total_rebate = 0u;
    uint64_t max_fee = 0u;
    uint64_t max_utilization = 0u;
    uint32_t warnings = 0u;
    vdtl_status status;

    if (ctx == NULL || route_id == NULL || out == NULL) return VDTL_ERR_NULL;
    route = vdtl_find_route(ctx, route_id);
    if (route == NULL) return VDTL_ERR_NOT_FOUND;

    memset(out, 0, sizeof(*out));
    copy_text(out->route_id, sizeof(out->route_id), route->id);
    out->leg_count = (uint64_t)route->leg_count;

    if (route->leg_count == 0u) warnings |= VDTL_POLICY_WARN_EMPTY_ROUTE;
    for (i = 0u; i < route->leg_count; i++) {
        const vdtl_leg *leg = &route->legs[i];
        uint32_t fee_bps = vdtl_dynamic_fee_bps(leg);
        if (leg->rate_num == 0u || leg->rate_den == 0u) warnings |= VDTL_POLICY_WARN_BAD_RATE;
        if (vdtl_find_asset(ctx, leg->from) == NULL ||
            vdtl_find_asset(ctx, leg->to) == NULL) {
            warnings |= VDTL_POLICY_WARN_UNKNOWN_ASSET;
        }
        if (i + 1u < route->leg_count &&
            strcmp(leg->to, route->legs[i + 1u].from) != 0) {
            warnings |= VDTL_POLICY_WARN_BROKEN_LINK;
        }
        if (fee_bps > 100u) warnings |= VDTL_POLICY_WARN_HIGH_FEE;
        if (leg->rebate_bps > 50u) warnings |= VDTL_POLICY_WARN_HIGH_REBATE;
        if (leg->utilization_bps > 9000u) warnings |= VDTL_POLICY_WARN_HIGH_UTILIZATION;

        status = vdtl_u64_add(total_fee, fee_bps, &total_fee);
        if (status != VDTL_OK) return status;
        status = vdtl_u64_add(total_rebate, leg->rebate_bps, &total_rebate);
        if (status != VDTL_OK) return status;
        if (fee_bps > max_fee) max_fee = fee_bps;
        if (leg->utilization_bps > max_utilization) max_utilization = leg->utilization_bps;
    }

    out->total_fee_bps = total_fee;
    out->total_rebate_bps = total_rebate;
    out->max_leg_fee_bps = max_fee;
    out->max_utilization_bps = max_utilization;
    out->warnings = warnings;
    out->ok = warnings == 0u ? 1 : 0;
    return VDTL_OK;
}

static vdtl_status account_value(
    const vdtl_ledger *ledger,
    vdtl_account_role role,
    uint64_t *out) {
    size_t i;
    uint64_t total = 0u;
    vdtl_status status;
    if (ledger == NULL || out == NULL) return VDTL_ERR_NULL;
    for (i = 0u; i < ledger->ctx.asset_count; i++) {
        uint64_t value;
        status = vdtl_value_micro_usd(&ledger->ctx, ledger->ctx.assets[i].symbol,
                                      ledger->accounts[role][i],
                                      VDTL_ROUND_CEIL, &value);
        if (status != VDTL_OK) return status;
        status = add_value(&total, value);
        if (status != VDTL_OK) return status;
    }
    *out = total;
    return VDTL_OK;
}

vdtl_status vdtl_collect_ledger_health(
    const vdtl_ledger *ledger,
    vdtl_ledger_health *out) {
    size_t i;
    vdtl_status status;
    if (ledger == NULL || out == NULL) return VDTL_ERR_NULL;
    memset(out, 0, sizeof(*out));
    out->vault_count = ledger->ctx.asset_count;

    for (i = 0u; i < ledger->ctx.asset_count; i++) {
        const vdtl_vault *vault = &ledger->vaults[i];
        vdtl_vault_metric *metric = &out->vaults[i];
        uint64_t observed_liquidity;
        copy_text(metric->asset, sizeof(metric->asset), vault->asset);
        metric->available = vault->available;
        metric->paid = vault->paid;
        metric->received = vault->received;
        status = vdtl_value_micro_usd(&ledger->ctx, vault->asset,
                                      vault->available, VDTL_ROUND_FLOOR,
                                      &metric->available_value);
        if (status != VDTL_OK) return status;
        status = vdtl_value_micro_usd(&ledger->ctx, vault->asset,
                                      vault->paid, VDTL_ROUND_CEIL,
                                      &metric->paid_value);
        if (status != VDTL_OK) return status;
        status = vdtl_value_micro_usd(&ledger->ctx, vault->asset,
                                      vault->received, VDTL_ROUND_FLOOR,
                                      &metric->received_value);
        if (status != VDTL_OK) return status;

        status = vdtl_u64_add(vault->available, vault->paid, &observed_liquidity);
        if (status != VDTL_OK) return status;
        if (observed_liquidity > 0u) {
            status = vdtl_mul_div_u64(vault->paid, 10000u,
                                      observed_liquidity,
                                      VDTL_ROUND_FLOOR,
                                      &metric->utilization_bps);
            if (status != VDTL_OK) return status;
        }
        if (metric->utilization_bps > 7500u) out->stressed_vaults += 1u;
        status = add_value(&out->total_available_value, metric->available_value);
        if (status != VDTL_OK) return status;
        status = add_value(&out->total_paid_value, metric->paid_value);
        if (status != VDTL_OK) return status;
        status = add_value(&out->total_received_value, metric->received_value);
        if (status != VDTL_OK) return status;
    }

    status = account_value(ledger, VDTL_ACCOUNT_TRADER, &out->trader_value);
    if (status != VDTL_OK) return status;
    status = account_value(ledger, VDTL_ACCOUNT_MAKER, &out->maker_value);
    if (status != VDTL_OK) return status;
    status = account_value(ledger, VDTL_ACCOUNT_PROTOCOL, &out->protocol_value);
    if (status != VDTL_OK) return status;
    return VDTL_OK;
}
