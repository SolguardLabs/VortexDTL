#include "vortexdtl.h"

#include <string.h>

static void vdtl_copy(char *dst, size_t dst_len, const char *src) {
    if (dst_len == 0u) return;
    if (src == NULL) src = "";
    (void)snprintf(dst, dst_len, "%s", src);
}

static vdtl_asset asset(
    int id,
    const char *symbol,
    const char *name,
    uint8_t decimals,
    uint64_t price_micro_usd,
    uint64_t opening_liquidity,
    uint64_t opening_user_balance) {
    vdtl_asset a;
    memset(&a, 0, sizeof(a));
    a.id = id;
    vdtl_copy(a.symbol, sizeof(a.symbol), symbol);
    vdtl_copy(a.name, sizeof(a.name), name);
    a.decimals = decimals;
    a.price_micro_usd = price_micro_usd;
    a.opening_liquidity = opening_liquidity;
    a.opening_user_balance = opening_user_balance;
    return a;
}

static vdtl_leg leg(
    const char *id,
    const char *from,
    const char *to,
    uint64_t rate_num,
    uint64_t rate_den,
    uint32_t base_fee_bps,
    uint32_t volatility_fee_bps,
    uint32_t utilization_fee_cap_bps,
    uint32_t utilization_bps,
    uint32_t rebate_bps) {
    vdtl_leg l;
    memset(&l, 0, sizeof(l));
    vdtl_copy(l.id, sizeof(l.id), id);
    vdtl_copy(l.from, sizeof(l.from), from);
    vdtl_copy(l.to, sizeof(l.to), to);
    l.rate_num = rate_num;
    l.rate_den = rate_den;
    l.base_fee_bps = base_fee_bps;
    l.volatility_fee_bps = volatility_fee_bps;
    l.utilization_fee_cap_bps = utilization_fee_cap_bps;
    l.utilization_bps = utilization_bps;
    l.rebate_bps = rebate_bps;
    l.reserve_weight_bps = 10000u;
    return l;
}

static void route_header(
    vdtl_route *route,
    const char *id,
    const char *label,
    uint32_t max_slippage_bps) {
    memset(route, 0, sizeof(*route));
    vdtl_copy(route->id, sizeof(route->id), id);
    vdtl_copy(route->label, sizeof(route->label), label);
    route->max_slippage_bps = max_slippage_bps;
}

void vdtl_context_init_default(vdtl_context *ctx) {
    vdtl_route *route;
    if (ctx == NULL) return;
    memset(ctx, 0, sizeof(*ctx));

    ctx->asset_count = 8u;
    ctx->assets[0] = asset(0, "vUSD", "Vortex synthetic dollar", 6u,
                           1000000u, 2500000000000ULL, 50000000000ULL);
    ctx->assets[1] = asset(1, "vEUR", "Vortex synthetic euro", 6u,
                           1080000u, 900000000000ULL, 0u);
    ctx->assets[2] = asset(2, "vREB", "Rebate accounting token", 4u,
                           240000u, 350000000000ULL, 0u);
    ctx->assets[3] = asset(3, "vQ7", "Seven decimal internal quote unit", 7u,
                           1000000u, 2000000000000ULL, 0u);
    ctx->assets[4] = asset(4, "vQ3", "Three decimal internal quote unit", 3u,
                           1000000u, 2000000000ULL, 0u);
    ctx->assets[5] = asset(5, "vQ8", "Eight decimal internal quote unit", 8u,
                           1000000u, 2000000000000ULL, 0u);
    ctx->assets[6] = asset(6, "vQ2", "Two decimal internal quote unit", 2u,
                           1000000u, 2000000000ULL, 0u);
    ctx->assets[7] = asset(7, "vQ5", "Five decimal internal quote unit", 5u,
                           1000000u, 2000000000000ULL, 0u);

    ctx->route_count = 3u;

    route = &ctx->routes[0];
    route_header(route, "stable-direct", "vUSD to vEUR treasury lane", 60u);
    route->leg_count = 1u;
    route->legs[0] = leg("usd-eur-prime", "vUSD", "vEUR",
                         1000000u, 1080000u, 8u, 2u, 7u, 5300u, 2u);

    route = &ctx->routes[1];
    route_header(route, "rebate-hop", "vUSD to rebate token to vEUR", 90u);
    route->leg_count = 2u;
    route->legs[0] = leg("usd-reb-maker", "vUSD", "vREB",
                         1000000u, 240000u, 5u, 1u, 10u, 4200u, 5u);
    route->legs[1] = leg("reb-eur-exit", "vREB", "vEUR",
                         240000u, 1080000u, 6u, 2u, 9u, 6100u, 3u);

    route = &ctx->routes[2];
    route_header(route, "vortex-loop", "multi-precision settlement corridor", 120u);
    route->leg_count = 6u;
    route->legs[0] = leg("loop-usd-q7", "vUSD", "vQ7", 1u, 1u, 0u, 0u, 0u, 0u, 0u);
    route->legs[1] = leg("loop-q7-q3", "vQ7", "vQ3", 1u, 1u, 0u, 0u, 0u, 0u, 0u);
    route->legs[2] = leg("loop-q3-q8", "vQ3", "vQ8", 1u, 1u, 0u, 0u, 0u, 0u, 0u);
    route->legs[3] = leg("loop-q8-q2", "vQ8", "vQ2", 1u, 1u, 0u, 0u, 0u, 0u, 0u);
    route->legs[4] = leg("loop-q2-q5", "vQ2", "vQ5", 1u, 1u, 0u, 0u, 0u, 0u, 0u);
    route->legs[5] = leg("loop-q5-usd", "vQ5", "vUSD", 1u, 1u, 0u, 0u, 0u, 0u, 0u);
}

const vdtl_asset *vdtl_find_asset(const vdtl_context *ctx, const char *symbol) {
    size_t i;
    if (ctx == NULL || symbol == NULL) return NULL;
    for (i = 0u; i < ctx->asset_count; i++) {
        if (strcmp(ctx->assets[i].symbol, symbol) == 0) return &ctx->assets[i];
    }
    return NULL;
}

const vdtl_route *vdtl_find_route(const vdtl_context *ctx, const char *route_id) {
    size_t i;
    if (ctx == NULL || route_id == NULL) return NULL;
    for (i = 0u; i < ctx->route_count; i++) {
        if (strcmp(ctx->routes[i].id, route_id) == 0) return &ctx->routes[i];
    }
    return NULL;
}

int vdtl_asset_index(const vdtl_context *ctx, const char *symbol) {
    size_t i;
    if (ctx == NULL || symbol == NULL) return -1;
    for (i = 0u; i < ctx->asset_count; i++) {
        if (strcmp(ctx->assets[i].symbol, symbol) == 0) return (int)i;
    }
    return -1;
}

uint32_t vdtl_dynamic_fee_bps(const vdtl_leg *leg) {
    uint32_t utilization_fee;
    if (leg == NULL) return 0u;
    utilization_fee = (uint32_t)(((uint64_t)leg->utilization_bps *
                                  (uint64_t)leg->utilization_fee_cap_bps) /
                                 10000u);
    return leg->base_fee_bps + leg->volatility_fee_bps + utilization_fee;
}
