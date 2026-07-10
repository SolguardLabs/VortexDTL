#ifndef VORTEXDTL_H
#define VORTEXDTL_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VDTL_COMMON_DECIMALS 12u
#define VDTL_MAX_ASSETS 16u
#define VDTL_MAX_ROUTES 8u
#define VDTL_MAX_LEGS 12u
#define VDTL_SYMBOL_LEN 16u
#define VDTL_ID_LEN 48u
#define VDTL_LABEL_LEN 96u
#define VDTL_ACCOUNT_COUNT 3u

typedef enum vdtl_status {
    VDTL_OK = 0,
    VDTL_ERR_NULL = 1,
    VDTL_ERR_NOT_FOUND = 2,
    VDTL_ERR_RANGE = 3,
    VDTL_ERR_OVERFLOW = 4,
    VDTL_ERR_DIV_ZERO = 5,
    VDTL_ERR_SLIPPAGE = 6,
    VDTL_ERR_BALANCE = 7,
    VDTL_ERR_VAULT = 8,
    VDTL_ERR_ARGUMENT = 9
} vdtl_status;

typedef enum vdtl_rounding {
    VDTL_ROUND_FLOOR = 0,
    VDTL_ROUND_CEIL = 1,
    VDTL_ROUND_HALF_UP = 2
} vdtl_rounding;

typedef enum vdtl_account_role {
    VDTL_ACCOUNT_TRADER = 0,
    VDTL_ACCOUNT_MAKER = 1,
    VDTL_ACCOUNT_PROTOCOL = 2
} vdtl_account_role;

typedef struct vdtl_asset {
    int id;
    char symbol[VDTL_SYMBOL_LEN];
    char name[VDTL_LABEL_LEN];
    uint8_t decimals;
    uint64_t price_micro_usd;
    uint64_t opening_liquidity;
    uint64_t opening_user_balance;
} vdtl_asset;

typedef struct vdtl_leg {
    char id[VDTL_ID_LEN];
    char from[VDTL_SYMBOL_LEN];
    char to[VDTL_SYMBOL_LEN];
    uint64_t rate_num;
    uint64_t rate_den;
    uint32_t base_fee_bps;
    uint32_t volatility_fee_bps;
    uint32_t utilization_fee_cap_bps;
    uint32_t utilization_bps;
    uint32_t rebate_bps;
    uint32_t reserve_weight_bps;
} vdtl_leg;

typedef struct vdtl_route {
    char id[VDTL_ID_LEN];
    char label[VDTL_LABEL_LEN];
    uint32_t max_slippage_bps;
    size_t leg_count;
    vdtl_leg legs[VDTL_MAX_LEGS];
} vdtl_route;

typedef struct vdtl_context {
    size_t asset_count;
    size_t route_count;
    vdtl_asset assets[VDTL_MAX_ASSETS];
    vdtl_route routes[VDTL_MAX_ROUTES];
} vdtl_context;

typedef struct vdtl_leg_quote {
    char id[VDTL_ID_LEN];
    char from[VDTL_SYMBOL_LEN];
    char to[VDTL_SYMBOL_LEN];
    uint64_t input;
    uint64_t common_in;
    uint64_t rated_common;
    uint64_t gross_out;
    uint32_t fee_bps;
    uint64_t fee;
    uint64_t rebate;
    uint64_t net_out;
    vdtl_rounding rounding;
} vdtl_leg_quote;

typedef struct vdtl_quote {
    char route_id[VDTL_ID_LEN];
    char source_asset[VDTL_SYMBOL_LEN];
    char target_asset[VDTL_SYMBOL_LEN];
    uint64_t amount_in;
    uint64_t final_out;
    size_t leg_count;
    vdtl_leg_quote legs[VDTL_MAX_LEGS];
    uint64_t fee_by_asset[VDTL_MAX_ASSETS];
    uint64_t rebate_by_asset[VDTL_MAX_ASSETS];
} vdtl_quote;

typedef struct vdtl_reserve_leg {
    char id[VDTL_ID_LEN];
    char from[VDTL_SYMBOL_LEN];
    char to[VDTL_SYMBOL_LEN];
    uint64_t required_out;
    uint32_t fee_bps;
    uint64_t fee_pad;
    uint64_t gross_estimate;
    uint64_t source_raw;
} vdtl_reserve_leg;

typedef struct vdtl_reserve {
    char route_id[VDTL_ID_LEN];
    uint64_t final_out;
    uint64_t reserve_in;
    size_t leg_count;
    vdtl_reserve_leg legs[VDTL_MAX_LEGS];
} vdtl_reserve;

typedef struct vdtl_vault {
    char asset[VDTL_SYMBOL_LEN];
    uint64_t available;
    uint64_t reserved;
    uint64_t paid;
    uint64_t received;
} vdtl_vault;

typedef struct vdtl_ledger_totals {
    uint64_t reserved_value;
    uint64_t credited_value;
    uint64_t extracted_value;
    uint64_t orders;
} vdtl_ledger_totals;

typedef struct vdtl_ledger {
    vdtl_context ctx;
    uint64_t accounts[VDTL_ACCOUNT_COUNT][VDTL_MAX_ASSETS];
    vdtl_vault vaults[VDTL_MAX_ASSETS];
    vdtl_ledger_totals totals;
    size_t events;
} vdtl_ledger;

typedef struct vdtl_settlement_event {
    char route_id[VDTL_ID_LEN];
    char source_asset[VDTL_SYMBOL_LEN];
    char target_asset[VDTL_SYMBOL_LEN];
    uint64_t amount_in;
    uint64_t min_out;
    vdtl_quote quote;
    vdtl_reserve reserve;
    vdtl_quote settlement;
    uint64_t reserved_value;
    uint64_t credited_value;
    uint64_t extracted_value;
} vdtl_settlement_event;

typedef struct vdtl_audit_report {
    char route_id[VDTL_ID_LEN];
    char source_asset[VDTL_SYMBOL_LEN];
    char target_asset[VDTL_SYMBOL_LEN];
    uint64_t amount_in;
    uint64_t quote_out;
    uint64_t reserve_in;
    uint64_t settlement_out;
    uint64_t reserved_value;
    uint64_t credited_value;
    uint64_t rebate_value;
    uint64_t gap_value;
    uint64_t leg_count;
    uint64_t lossy_quote_legs;
    uint64_t expansive_settlement_legs;
    int vulnerable;
} vdtl_audit_report;

typedef struct vdtl_route_policy_report {
    char route_id[VDTL_ID_LEN];
    uint64_t leg_count;
    uint64_t total_fee_bps;
    uint64_t total_rebate_bps;
    uint64_t max_leg_fee_bps;
    uint64_t max_utilization_bps;
    uint32_t warnings;
    int ok;
} vdtl_route_policy_report;

typedef struct vdtl_vault_metric {
    char asset[VDTL_SYMBOL_LEN];
    uint64_t available;
    uint64_t paid;
    uint64_t received;
    uint64_t available_value;
    uint64_t paid_value;
    uint64_t received_value;
    uint64_t utilization_bps;
} vdtl_vault_metric;

typedef struct vdtl_ledger_health {
    size_t vault_count;
    vdtl_vault_metric vaults[VDTL_MAX_ASSETS];
    uint64_t total_available_value;
    uint64_t total_paid_value;
    uint64_t total_received_value;
    uint64_t trader_value;
    uint64_t maker_value;
    uint64_t protocol_value;
    uint64_t stressed_vaults;
} vdtl_ledger_health;

const char *vdtl_status_name(vdtl_status status);
const char *vdtl_rounding_name(vdtl_rounding rounding);

vdtl_status vdtl_u64_add(uint64_t a, uint64_t b, uint64_t *out);
vdtl_status vdtl_u64_sub(uint64_t a, uint64_t b, uint64_t *out);
vdtl_status vdtl_pow10_u64(uint8_t exp, uint64_t *out);
vdtl_status vdtl_div_round_u64(
    uint64_t numerator,
    uint64_t denominator,
    vdtl_rounding rounding,
    uint64_t *out);
vdtl_status vdtl_mul_div_u64(
    uint64_t a,
    uint64_t b,
    uint64_t denominator,
    vdtl_rounding rounding,
    uint64_t *out);
vdtl_status vdtl_scale_amount(
    uint64_t amount,
    uint8_t from_decimals,
    uint8_t to_decimals,
    vdtl_rounding rounding,
    uint64_t *out);
vdtl_status vdtl_bps_amount(
    uint64_t amount,
    uint32_t bps,
    vdtl_rounding rounding,
    uint64_t *out);

void vdtl_context_init_default(vdtl_context *ctx);
const vdtl_asset *vdtl_find_asset(const vdtl_context *ctx, const char *symbol);
const vdtl_route *vdtl_find_route(const vdtl_context *ctx, const char *route_id);
int vdtl_asset_index(const vdtl_context *ctx, const char *symbol);
uint32_t vdtl_dynamic_fee_bps(const vdtl_leg *leg);
vdtl_status vdtl_min_out_for_slippage(
    uint64_t quote_out,
    uint32_t slippage_bps,
    uint64_t *out);
vdtl_status vdtl_check_route_policy(
    const vdtl_context *ctx,
    const char *route_id,
    vdtl_route_policy_report *out);

vdtl_status vdtl_quote_route(
    const vdtl_context *ctx,
    const char *route_id,
    uint64_t amount_in,
    vdtl_quote *out);
vdtl_status vdtl_reserve_for_quote(
    const vdtl_context *ctx,
    const char *route_id,
    uint64_t final_out,
    vdtl_reserve *out);
vdtl_status vdtl_settlement_forward(
    const vdtl_context *ctx,
    const char *route_id,
    uint64_t amount_in,
    vdtl_quote *out);
vdtl_status vdtl_value_micro_usd(
    const vdtl_context *ctx,
    const char *symbol,
    uint64_t amount,
    vdtl_rounding rounding,
    uint64_t *out);

vdtl_status vdtl_ledger_init_default(vdtl_ledger *ledger);
vdtl_status vdtl_settle_order(
    vdtl_ledger *ledger,
    const char *route_id,
    uint64_t amount_in,
    uint64_t min_out,
    vdtl_settlement_event *out);
vdtl_status vdtl_audit_route_amount(
    const vdtl_context *ctx,
    const char *route_id,
    uint64_t amount_in,
    vdtl_audit_report *out);
vdtl_status vdtl_audit_event(
    const vdtl_context *ctx,
    const vdtl_settlement_event *event,
    vdtl_audit_report *out);
vdtl_status vdtl_find_first_rounding_gap(
    const vdtl_context *ctx,
    const char *route_id,
    uint64_t start_amount,
    uint64_t end_amount,
    uint64_t step,
    vdtl_audit_report *out);
vdtl_status vdtl_collect_ledger_health(
    const vdtl_ledger *ledger,
    vdtl_ledger_health *out);

void vdtl_json_print_quote(FILE *stream, const vdtl_context *ctx, const vdtl_quote *quote);
void vdtl_json_print_compact_settlement(
    FILE *stream,
    const vdtl_context *ctx,
    const vdtl_settlement_event *event);
void vdtl_json_print_ledger(FILE *stream, const vdtl_ledger *ledger);
void vdtl_json_print_demo(FILE *stream, const vdtl_settlement_event *a,
                          const vdtl_settlement_event *b, const vdtl_ledger *ledger);
void vdtl_json_print_exploit(FILE *stream, const vdtl_settlement_event *first,
                             const vdtl_settlement_event *last,
                             const vdtl_ledger *ledger,
                             uint64_t iterations,
                             uint64_t amount_in);
void vdtl_json_print_route_policy(FILE *stream, const vdtl_route_policy_report *report);
void vdtl_json_print_ledger_health(FILE *stream, const vdtl_ledger_health *health);

#ifdef __cplusplus
}
#endif

#endif
