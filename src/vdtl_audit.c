#include "vortexdtl.h"

#include <string.h>

static void copy_text(char *dst, size_t dst_len, const char *src) {
    if (dst_len == 0u) return;
    if (src == NULL) src = "";
    (void)snprintf(dst, dst_len, "%s", src);
}

static vdtl_status sum_rebate_value(
    const vdtl_context *ctx,
    const uint64_t rebates[VDTL_MAX_ASSETS],
    uint64_t *out) {
    size_t i;
    uint64_t total = 0u;
    vdtl_status status;
    if (ctx == NULL || rebates == NULL || out == NULL) return VDTL_ERR_NULL;
    for (i = 0u; i < ctx->asset_count; i++) {
        uint64_t value;
        if (rebates[i] == 0u) continue;
        status = vdtl_value_micro_usd(ctx, ctx->assets[i].symbol,
                                      rebates[i], VDTL_ROUND_CEIL, &value);
        if (status != VDTL_OK) return status;
        status = vdtl_u64_add(total, value, &total);
        if (status != VDTL_OK) return status;
    }
    *out = total;
    return VDTL_OK;
}

static uint64_t count_lossy_quote_legs(const vdtl_quote *quote) {
    size_t i;
    uint64_t lossy = 0u;
    if (quote == NULL) return 0u;
    for (i = 0u; i < quote->leg_count; i++) {
        const vdtl_leg_quote *leg = &quote->legs[i];
        if (leg->gross_out == 0u) continue;
        if (leg->rated_common != leg->gross_out) {
            lossy += 1u;
        }
    }
    return lossy;
}

static uint64_t count_expansive_settlement_legs(
    const vdtl_quote *quote,
    const vdtl_quote *settlement) {
    size_t i;
    uint64_t expansive = 0u;
    if (quote == NULL || settlement == NULL) return 0u;
    for (i = 0u; i < quote->leg_count && i < settlement->leg_count; i++) {
        if (settlement->legs[i].gross_out > quote->legs[i].gross_out) {
            expansive += 1u;
        }
    }
    return expansive;
}

static vdtl_status fill_report(
    const vdtl_context *ctx,
    const char *route_id,
    uint64_t amount_in,
    const vdtl_quote *quote,
    const vdtl_reserve *reserve,
    const vdtl_quote *settlement,
    vdtl_audit_report *out) {
    uint64_t reserved_value;
    uint64_t target_value;
    uint64_t rebate_value;
    uint64_t credited_value;
    vdtl_status status;
    if (ctx == NULL || route_id == NULL || quote == NULL ||
        reserve == NULL || settlement == NULL || out == NULL) {
        return VDTL_ERR_NULL;
    }
    status = vdtl_value_micro_usd(ctx, quote->source_asset,
                                  reserve->reserve_in, VDTL_ROUND_FLOOR,
                                  &reserved_value);
    if (status != VDTL_OK) return status;
    status = vdtl_value_micro_usd(ctx, quote->target_asset,
                                  settlement->final_out, VDTL_ROUND_CEIL,
                                  &target_value);
    if (status != VDTL_OK) return status;
    status = sum_rebate_value(ctx, settlement->rebate_by_asset, &rebate_value);
    if (status != VDTL_OK) return status;
    status = vdtl_u64_add(target_value, rebate_value, &credited_value);
    if (status != VDTL_OK) return status;

    memset(out, 0, sizeof(*out));
    copy_text(out->route_id, sizeof(out->route_id), route_id);
    copy_text(out->source_asset, sizeof(out->source_asset), quote->source_asset);
    copy_text(out->target_asset, sizeof(out->target_asset), quote->target_asset);
    out->amount_in = amount_in;
    out->quote_out = quote->final_out;
    out->reserve_in = reserve->reserve_in;
    out->settlement_out = settlement->final_out;
    out->reserved_value = reserved_value;
    out->credited_value = credited_value;
    out->rebate_value = rebate_value;
    out->gap_value = credited_value > reserved_value ? credited_value - reserved_value : 0u;
    out->leg_count = (uint64_t)quote->leg_count;
    out->lossy_quote_legs = count_lossy_quote_legs(quote);
    out->expansive_settlement_legs = count_expansive_settlement_legs(quote, settlement);
    out->vulnerable = credited_value > reserved_value ? 1 : 0;
    return VDTL_OK;
}

vdtl_status vdtl_audit_route_amount(
    const vdtl_context *ctx,
    const char *route_id,
    uint64_t amount_in,
    vdtl_audit_report *out) {
    vdtl_quote quote;
    vdtl_reserve reserve;
    vdtl_quote settlement;
    vdtl_status status;
    if (ctx == NULL || route_id == NULL || out == NULL) return VDTL_ERR_NULL;
    status = vdtl_quote_route(ctx, route_id, amount_in, &quote);
    if (status != VDTL_OK) return status;
    status = vdtl_reserve_for_quote(ctx, route_id, quote.final_out, &reserve);
    if (status != VDTL_OK) return status;
    status = vdtl_settlement_forward(ctx, route_id, amount_in, &settlement);
    if (status != VDTL_OK) return status;
    return fill_report(ctx, route_id, amount_in, &quote, &reserve, &settlement, out);
}

vdtl_status vdtl_audit_event(
    const vdtl_context *ctx,
    const vdtl_settlement_event *event,
    vdtl_audit_report *out) {
    if (ctx == NULL || event == NULL || out == NULL) return VDTL_ERR_NULL;
    return fill_report(ctx, event->route_id, event->amount_in,
                       &event->quote, &event->reserve, &event->settlement, out);
}

vdtl_status vdtl_find_first_rounding_gap(
    const vdtl_context *ctx,
    const char *route_id,
    uint64_t start_amount,
    uint64_t end_amount,
    uint64_t step,
    vdtl_audit_report *out) {
    uint64_t amount;
    vdtl_status status;
    if (ctx == NULL || route_id == NULL || out == NULL) return VDTL_ERR_NULL;
    if (step == 0u || start_amount > end_amount) return VDTL_ERR_ARGUMENT;
    memset(out, 0, sizeof(*out));
    for (amount = start_amount; amount <= end_amount; amount += step) {
        vdtl_audit_report report;
        status = vdtl_audit_route_amount(ctx, route_id, amount, &report);
        if (status != VDTL_OK) return status;
        if (report.vulnerable) {
            *out = report;
            return VDTL_OK;
        }
        if (UINT64_MAX - amount < step) break;
    }
    return VDTL_ERR_NOT_FOUND;
}
