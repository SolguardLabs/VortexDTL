#include "vortexdtl.h"

#include <limits.h>
#include <string.h>

const char *vdtl_status_name(vdtl_status status) {
    switch (status) {
        case VDTL_OK:
            return "VDTL_OK";
        case VDTL_ERR_NULL:
            return "VDTL_ERR_NULL";
        case VDTL_ERR_NOT_FOUND:
            return "VDTL_ERR_NOT_FOUND";
        case VDTL_ERR_RANGE:
            return "VDTL_ERR_RANGE";
        case VDTL_ERR_OVERFLOW:
            return "VDTL_ERR_OVERFLOW";
        case VDTL_ERR_DIV_ZERO:
            return "VDTL_ERR_DIV_ZERO";
        case VDTL_ERR_SLIPPAGE:
            return "VDTL_ERR_SLIPPAGE";
        case VDTL_ERR_BALANCE:
            return "VDTL_ERR_BALANCE";
        case VDTL_ERR_VAULT:
            return "VDTL_ERR_VAULT";
        case VDTL_ERR_ARGUMENT:
            return "VDTL_ERR_ARGUMENT";
        default:
            return "VDTL_ERR_UNKNOWN";
    }
}

const char *vdtl_rounding_name(vdtl_rounding rounding) {
    switch (rounding) {
        case VDTL_ROUND_FLOOR:
            return "floor";
        case VDTL_ROUND_CEIL:
            return "ceil";
        case VDTL_ROUND_HALF_UP:
            return "half-up";
        default:
            return "unknown";
    }
}

vdtl_status vdtl_u64_add(uint64_t a, uint64_t b, uint64_t *out) {
    if (out == NULL) return VDTL_ERR_NULL;
    if (UINT64_MAX - a < b) return VDTL_ERR_OVERFLOW;
    *out = a + b;
    return VDTL_OK;
}

vdtl_status vdtl_u64_sub(uint64_t a, uint64_t b, uint64_t *out) {
    if (out == NULL) return VDTL_ERR_NULL;
    if (a < b) return VDTL_ERR_BALANCE;
    *out = a - b;
    return VDTL_OK;
}

vdtl_status vdtl_pow10_u64(uint8_t exp, uint64_t *out) {
    uint64_t value = 1u;
    uint8_t i;
    if (out == NULL) return VDTL_ERR_NULL;
    if (exp > 18u) return VDTL_ERR_RANGE;
    for (i = 0u; i < exp; i++) {
        if (UINT64_MAX / 10u < value) return VDTL_ERR_OVERFLOW;
        value *= 10u;
    }
    *out = value;
    return VDTL_OK;
}

vdtl_status vdtl_div_round_u64(
    uint64_t numerator,
    uint64_t denominator,
    vdtl_rounding rounding,
    uint64_t *out) {
    uint64_t q;
    uint64_t r;
    if (out == NULL) return VDTL_ERR_NULL;
    if (denominator == 0u) return VDTL_ERR_DIV_ZERO;
    q = numerator / denominator;
    r = numerator % denominator;
    if (r == 0u || rounding == VDTL_ROUND_FLOOR) {
        *out = q;
        return VDTL_OK;
    }
    if (rounding == VDTL_ROUND_CEIL) {
        if (q == UINT64_MAX) return VDTL_ERR_OVERFLOW;
        *out = q + 1u;
        return VDTL_OK;
    }
    if (rounding == VDTL_ROUND_HALF_UP) {
        if (r > denominator - r) {
            if (q == UINT64_MAX) return VDTL_ERR_OVERFLOW;
            *out = q + 1u;
        } else {
            *out = q;
        }
        return VDTL_OK;
    }
    return VDTL_ERR_ARGUMENT;
}

vdtl_status vdtl_mul_div_u64(
    uint64_t a,
    uint64_t b,
    uint64_t denominator,
    vdtl_rounding rounding,
    uint64_t *out) {
    if (out == NULL) return VDTL_ERR_NULL;
    if (denominator == 0u) return VDTL_ERR_DIV_ZERO;
#if defined(__SIZEOF_INT128__)
    {
        __uint128_t product = ((__uint128_t)a) * ((__uint128_t)b);
        __uint128_t q = product / denominator;
        __uint128_t r = product % denominator;
        if (r != 0u && rounding == VDTL_ROUND_CEIL) q += 1u;
        if (r != 0u && rounding == VDTL_ROUND_HALF_UP && r * 2u >= denominator) q += 1u;
        if (q > UINT64_MAX) return VDTL_ERR_OVERFLOW;
        *out = (uint64_t)q;
        return VDTL_OK;
    }
#else
    {
        long double product = (long double)a * (long double)b;
        long double qd = product / (long double)denominator;
        uint64_t q = (uint64_t)qd;
        long double floor_value = (long double)q;
        if (rounding == VDTL_ROUND_CEIL && qd > floor_value) {
            if (q == UINT64_MAX) return VDTL_ERR_OVERFLOW;
            q += 1u;
        } else if (rounding == VDTL_ROUND_HALF_UP && qd - floor_value >= 0.5L) {
            if (q == UINT64_MAX) return VDTL_ERR_OVERFLOW;
            q += 1u;
        }
        *out = q;
        return VDTL_OK;
    }
#endif
}

vdtl_status vdtl_scale_amount(
    uint64_t amount,
    uint8_t from_decimals,
    uint8_t to_decimals,
    vdtl_rounding rounding,
    uint64_t *out) {
    uint8_t delta;
    uint64_t factor;
    if (out == NULL) return VDTL_ERR_NULL;
    if (from_decimals == to_decimals) {
        *out = amount;
        return VDTL_OK;
    }
    if (from_decimals < to_decimals) {
        delta = (uint8_t)(to_decimals - from_decimals);
        if (vdtl_pow10_u64(delta, &factor) != VDTL_OK) return VDTL_ERR_RANGE;
        if (factor != 0u && amount > UINT64_MAX / factor) return VDTL_ERR_OVERFLOW;
        *out = amount * factor;
        return VDTL_OK;
    }
    delta = (uint8_t)(from_decimals - to_decimals);
    if (vdtl_pow10_u64(delta, &factor) != VDTL_OK) return VDTL_ERR_RANGE;
    return vdtl_div_round_u64(amount, factor, rounding, out);
}

vdtl_status vdtl_bps_amount(
    uint64_t amount,
    uint32_t bps,
    vdtl_rounding rounding,
    uint64_t *out) {
    if (bps > 10000u) return VDTL_ERR_RANGE;
    return vdtl_mul_div_u64(amount, (uint64_t)bps, 10000u, rounding, out);
}
