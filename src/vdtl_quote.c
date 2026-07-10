#include "vortexdtl.h"

#include <string.h>

static void copy_text(char *dst, size_t dst_len, const char *src) {
    if (dst_len == 0u) return;
    if (src == NULL) src = "";
    (void)snprintf(dst, dst_len, "%s", src);
}

static vdtl_status add_asset_amount(
    const vdtl_context *ctx,
    uint64_t map[VDTL_MAX_ASSETS],
    const char *symbol,
    uint64_t amount) {
    int index = vdtl_asset_index(ctx, symbol);
    if (index < 0) return VDTL_ERR_NOT_FOUND;
    return vdtl_u64_add(map[index], amount, &map[index]);
}

static vdtl_status compute_leg(
    const vdtl_context *ctx,
    const vdtl_leg *leg,
    uint64_t amount_in,
    vdtl_rounding amount_rounding,
    vdtl_rounding rebate_rounding,
    vdtl_leg_quote *out) {
    const vdtl_asset *source;
    const vdtl_asset *target;
    vdtl_status status;
    uint64_t common_in;
    uint64_t rated_common;
    uint64_t gross_out;
    uint64_t fee;
    uint64_t rebate;
    uint64_t net_out;
    uint32_t fee_bps;

    if (ctx == NULL || leg == NULL || out == NULL) return VDTL_ERR_NULL;
    if (leg->rate_den == 0u) return VDTL_ERR_DIV_ZERO;
    source = vdtl_find_asset(ctx, leg->from);
    target = vdtl_find_asset(ctx, leg->to);
    if (source == NULL || target == NULL) return VDTL_ERR_NOT_FOUND;

    status = vdtl_scale_amount(amount_in, source->decimals,
                               (uint8_t)VDTL_COMMON_DECIMALS,
                               amount_rounding, &common_in);
    if (status != VDTL_OK) return status;
    status = vdtl_mul_div_u64(common_in, leg->rate_num, leg->rate_den,
                              amount_rounding, &rated_common);
    if (status != VDTL_OK) return status;
    status = vdtl_scale_amount(rated_common, (uint8_t)VDTL_COMMON_DECIMALS,
                               target->decimals, amount_rounding, &gross_out);
    if (status != VDTL_OK) return status;

    fee_bps = vdtl_dynamic_fee_bps(leg);
    status = vdtl_bps_amount(gross_out, fee_bps, VDTL_ROUND_FLOOR, &fee);
    if (status != VDTL_OK) return status;
    status = vdtl_bps_amount(gross_out, leg->rebate_bps, rebate_rounding, &rebate);
    if (status != VDTL_OK) return status;
    status = vdtl_u64_sub(gross_out, fee, &net_out);
    if (status != VDTL_OK) return status;

    memset(out, 0, sizeof(*out));
    copy_text(out->id, sizeof(out->id), leg->id);
    copy_text(out->from, sizeof(out->from), source->symbol);
    copy_text(out->to, sizeof(out->to), target->symbol);
    out->input = amount_in;
    out->common_in = common_in;
    out->rated_common = rated_common;
    out->gross_out = gross_out;
    out->fee_bps = fee_bps;
    out->fee = fee;
    out->rebate = rebate;
    out->net_out = net_out;
    out->rounding = amount_rounding;
    return VDTL_OK;
}

static vdtl_status route_forward(
    const vdtl_context *ctx,
    const char *route_id,
    uint64_t amount_in,
    vdtl_rounding amount_rounding,
    vdtl_rounding rebate_rounding,
    vdtl_quote *out) {
    const vdtl_route *route;
    uint64_t amount = amount_in;
    size_t i;
    vdtl_status status;

    if (ctx == NULL || route_id == NULL || out == NULL) return VDTL_ERR_NULL;
    route = vdtl_find_route(ctx, route_id);
    if (route == NULL) return VDTL_ERR_NOT_FOUND;
    if (route->leg_count == 0u || route->leg_count > VDTL_MAX_LEGS) return VDTL_ERR_RANGE;

    memset(out, 0, sizeof(*out));
    copy_text(out->route_id, sizeof(out->route_id), route->id);
    copy_text(out->source_asset, sizeof(out->source_asset), route->legs[0].from);
    copy_text(out->target_asset, sizeof(out->target_asset), route->legs[route->leg_count - 1u].to);
    out->amount_in = amount_in;
    out->leg_count = route->leg_count;

    for (i = 0u; i < route->leg_count; i++) {
        status = compute_leg(ctx, &route->legs[i], amount, amount_rounding,
                             rebate_rounding, &out->legs[i]);
        if (status != VDTL_OK) return status;
        status = add_asset_amount(ctx, out->fee_by_asset, out->legs[i].to, out->legs[i].fee);
        if (status != VDTL_OK) return status;
        status = add_asset_amount(ctx, out->rebate_by_asset, out->legs[i].to,
                                  out->legs[i].rebate);
        if (status != VDTL_OK) return status;
        amount = out->legs[i].net_out;
    }
    out->final_out = amount;
    return VDTL_OK;
}

vdtl_status vdtl_quote_route(
    const vdtl_context *ctx,
    const char *route_id,
    uint64_t amount_in,
    vdtl_quote *out) {
    return route_forward(ctx, route_id, amount_in,
                         VDTL_ROUND_FLOOR, VDTL_ROUND_FLOOR, out);
}

vdtl_status vdtl_settlement_forward(
    const vdtl_context *ctx,
    const char *route_id,
    uint64_t amount_in,
    vdtl_quote *out) {
    return route_forward(ctx, route_id, amount_in,
                         VDTL_ROUND_CEIL, VDTL_ROUND_CEIL, out);
}

vdtl_status vdtl_reserve_for_quote(
    const vdtl_context *ctx,
    const char *route_id,
    uint64_t final_out,
    vdtl_reserve *out) {
    const vdtl_route *route;
    uint64_t required = final_out;
    int i;
    vdtl_status status;

    if (ctx == NULL || route_id == NULL || out == NULL) return VDTL_ERR_NULL;
    route = vdtl_find_route(ctx, route_id);
    if (route == NULL) return VDTL_ERR_NOT_FOUND;
    if (route->leg_count == 0u || route->leg_count > VDTL_MAX_LEGS) return VDTL_ERR_RANGE;

    memset(out, 0, sizeof(*out));
    copy_text(out->route_id, sizeof(out->route_id), route->id);
    out->final_out = final_out;
    out->leg_count = route->leg_count;

    for (i = (int)route->leg_count - 1; i >= 0; i--) {
        const vdtl_leg *leg = &route->legs[i];
        const vdtl_asset *source = vdtl_find_asset(ctx, leg->from);
        const vdtl_asset *target = vdtl_find_asset(ctx, leg->to);
        vdtl_reserve_leg *dst = &out->legs[i];
        uint32_t fee_bps = vdtl_dynamic_fee_bps(leg);
        uint64_t fee_pad;
        uint64_t gross_estimate;
        uint64_t target_common;
        uint64_t source_common;
        uint64_t source_raw;

        if (source == NULL || target == NULL) return VDTL_ERR_NOT_FOUND;
        status = vdtl_bps_amount(required, fee_bps, VDTL_ROUND_FLOOR, &fee_pad);
        if (status != VDTL_OK) return status;
        status = vdtl_u64_add(required, fee_pad, &gross_estimate);
        if (status != VDTL_OK) return status;
        status = vdtl_scale_amount(gross_estimate, target->decimals,
                                   (uint8_t)VDTL_COMMON_DECIMALS,
                                   VDTL_ROUND_FLOOR, &target_common);
        if (status != VDTL_OK) return status;
        status = vdtl_mul_div_u64(target_common, leg->rate_den, leg->rate_num,
                                  VDTL_ROUND_FLOOR, &source_common);
        if (status != VDTL_OK) return status;
        status = vdtl_scale_amount(source_common, (uint8_t)VDTL_COMMON_DECIMALS,
                                   source->decimals, VDTL_ROUND_FLOOR, &source_raw);
        if (status != VDTL_OK) return status;

        copy_text(dst->id, sizeof(dst->id), leg->id);
        copy_text(dst->from, sizeof(dst->from), source->symbol);
        copy_text(dst->to, sizeof(dst->to), target->symbol);
        dst->required_out = required;
        dst->fee_bps = fee_bps;
        dst->fee_pad = fee_pad;
        dst->gross_estimate = gross_estimate;
        dst->source_raw = source_raw;
        required = source_raw;
    }
    out->reserve_in = required;
    return VDTL_OK;
}

vdtl_status vdtl_value_micro_usd(
    const vdtl_context *ctx,
    const char *symbol,
    uint64_t amount,
    vdtl_rounding rounding,
    uint64_t *out) {
    const vdtl_asset *asset;
    uint64_t scale;
    if (ctx == NULL || symbol == NULL || out == NULL) return VDTL_ERR_NULL;
    asset = vdtl_find_asset(ctx, symbol);
    if (asset == NULL) return VDTL_ERR_NOT_FOUND;
    if (vdtl_pow10_u64(asset->decimals, &scale) != VDTL_OK) return VDTL_ERR_RANGE;
    return vdtl_mul_div_u64(amount, asset->price_micro_usd, scale, rounding, out);
}
