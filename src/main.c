#include "vortexdtl.h"

#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

static void usage(FILE *stream) {
    fputs("usage:\n", stream);
    fputs("  vortexdtl demo-json\n", stream);
    fputs("  vortexdtl quote-json <route> <amount>\n", stream);
    fputs("  vortexdtl settle-json <route> <amount> [minOut]\n", stream);
    fputs("  vortexdtl exploit-json [iterations] [amount]\n", stream);
    fputs("  vortexdtl route-policy-json <route>\n", stream);
    fputs("  vortexdtl health-json\n", stream);
}

static int parse_u64(const char *text, uint64_t fallback, uint64_t *out) {
    char *end = NULL;
    unsigned long long value;
    if (out == NULL) return 0;
    if (text == NULL) {
        *out = fallback;
        return 1;
    }
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') return 0;
    *out = (uint64_t)value;
    return 1;
}

static int fail_status(vdtl_status status) {
    fprintf(stderr, "%s\n", vdtl_status_name(status));
    return 1;
}

static int command_quote(int argc, char **argv) {
    vdtl_context ctx;
    vdtl_quote quote;
    const char *route = argc > 2 ? argv[2] : "stable-direct";
    uint64_t amount;
    vdtl_status status;
    if (!parse_u64(argc > 3 ? argv[3] : NULL, 2500000u, &amount)) return fail_status(VDTL_ERR_ARGUMENT);
    vdtl_context_init_default(&ctx);
    status = vdtl_quote_route(&ctx, route, amount, &quote);
    if (status != VDTL_OK) return fail_status(status);
    vdtl_json_print_quote(stdout, &ctx, &quote);
    fputc('\n', stdout);
    return 0;
}

static int command_settle(int argc, char **argv) {
    vdtl_ledger ledger;
    vdtl_quote quote;
    vdtl_settlement_event event;
    const char *route = argc > 2 ? argv[2] : "stable-direct";
    uint64_t amount;
    uint64_t min_out;
    vdtl_status status;
    if (!parse_u64(argc > 3 ? argv[3] : NULL, 2500000u, &amount)) return fail_status(VDTL_ERR_ARGUMENT);
    status = vdtl_ledger_init_default(&ledger);
    if (status != VDTL_OK) return fail_status(status);
    status = vdtl_quote_route(&ledger.ctx, route, amount, &quote);
    if (status != VDTL_OK) return fail_status(status);
    if (!parse_u64(argc > 4 ? argv[4] : NULL, quote.final_out, &min_out)) {
        return fail_status(VDTL_ERR_ARGUMENT);
    }
    status = vdtl_settle_order(&ledger, route, amount, min_out, &event);
    if (status != VDTL_OK) return fail_status(status);
    fputs("{\"settlement\":", stdout);
    vdtl_json_print_compact_settlement(stdout, &ledger.ctx, &event);
    fputs(",\"ledger\":", stdout);
    vdtl_json_print_ledger(stdout, &ledger);
    fputs("}\n", stdout);
    return 0;
}

static int command_demo(void) {
    vdtl_ledger ledger;
    vdtl_quote quote_a;
    vdtl_quote quote_b;
    vdtl_settlement_event event_a;
    vdtl_settlement_event event_b;
    vdtl_status status = vdtl_ledger_init_default(&ledger);
    if (status != VDTL_OK) return fail_status(status);
    status = vdtl_quote_route(&ledger.ctx, "stable-direct", 2500000u, &quote_a);
    if (status != VDTL_OK) return fail_status(status);
    status = vdtl_settle_order(&ledger, "stable-direct", 2500000u,
                               quote_a.final_out, &event_a);
    if (status != VDTL_OK) return fail_status(status);
    status = vdtl_quote_route(&ledger.ctx, "rebate-hop", 3750000u, &quote_b);
    if (status != VDTL_OK) return fail_status(status);
    status = vdtl_settle_order(&ledger, "rebate-hop", 3750000u,
                               quote_b.final_out, &event_b);
    if (status != VDTL_OK) return fail_status(status);
    vdtl_json_print_demo(stdout, &event_a, &event_b, &ledger);
    fputc('\n', stdout);
    return 0;
}

static int command_route_policy(int argc, char **argv) {
    vdtl_context ctx;
    vdtl_route_policy_report report;
    const char *route = argc > 2 ? argv[2] : "stable-direct";
    vdtl_status status;
    vdtl_context_init_default(&ctx);
    status = vdtl_check_route_policy(&ctx, route, &report);
    if (status != VDTL_OK) return fail_status(status);
    vdtl_json_print_route_policy(stdout, &report);
    fputc('\n', stdout);
    return 0;
}

static int command_health(void) {
    vdtl_ledger ledger;
    vdtl_quote quote_a;
    vdtl_quote quote_b;
    vdtl_settlement_event event_a;
    vdtl_settlement_event event_b;
    vdtl_ledger_health health;
    vdtl_status status = vdtl_ledger_init_default(&ledger);
    if (status != VDTL_OK) return fail_status(status);
    status = vdtl_quote_route(&ledger.ctx, "stable-direct", 2500000u, &quote_a);
    if (status != VDTL_OK) return fail_status(status);
    status = vdtl_settle_order(&ledger, "stable-direct", 2500000u,
                               quote_a.final_out, &event_a);
    if (status != VDTL_OK) return fail_status(status);
    status = vdtl_quote_route(&ledger.ctx, "rebate-hop", 3750000u, &quote_b);
    if (status != VDTL_OK) return fail_status(status);
    status = vdtl_settle_order(&ledger, "rebate-hop", 3750000u,
                               quote_b.final_out, &event_b);
    if (status != VDTL_OK) return fail_status(status);
    status = vdtl_collect_ledger_health(&ledger, &health);
    if (status != VDTL_OK) return fail_status(status);
    vdtl_json_print_ledger_health(stdout, &health);
    fputc('\n', stdout);
    return 0;
}

static int command_exploit(int argc, char **argv) {
    vdtl_ledger ledger;
    vdtl_quote quote;
    vdtl_settlement_event first;
    vdtl_settlement_event last;
    uint64_t iterations;
    uint64_t amount;
    uint64_t i;
    vdtl_status status;
    memset(&first, 0, sizeof(first));
    memset(&last, 0, sizeof(last));
    if (!parse_u64(argc > 2 ? argv[2] : NULL, 64u, &iterations)) return fail_status(VDTL_ERR_ARGUMENT);
    if (!parse_u64(argc > 3 ? argv[3] : NULL, 1000001u, &amount)) return fail_status(VDTL_ERR_ARGUMENT);
    status = vdtl_ledger_init_default(&ledger);
    if (status != VDTL_OK) return fail_status(status);
    status = vdtl_quote_route(&ledger.ctx, "vortex-loop", amount, &quote);
    if (status != VDTL_OK) return fail_status(status);
    for (i = 0u; i < iterations; i++) {
        status = vdtl_settle_order(&ledger, "vortex-loop", amount, quote.final_out, &last);
        if (status != VDTL_OK) return fail_status(status);
        if (i == 0u) first = last;
    }
    vdtl_json_print_exploit(stdout, &first, &last, &ledger, iterations, amount);
    fputc('\n', stdout);
    return 0;
}

int main(int argc, char **argv) {
    const char *command = argc > 1 ? argv[1] : "help";
    if (strcmp(command, "help") == 0 || strcmp(command, "--help") == 0) {
        usage(stdout);
        return 0;
    }
    if (strcmp(command, "quote-json") == 0) return command_quote(argc, argv);
    if (strcmp(command, "settle-json") == 0) return command_settle(argc, argv);
    if (strcmp(command, "demo-json") == 0) return command_demo();
    if (strcmp(command, "exploit-json") == 0) return command_exploit(argc, argv);
    if (strcmp(command, "route-policy-json") == 0) return command_route_policy(argc, argv);
    if (strcmp(command, "health-json") == 0) return command_health();
    usage(stderr);
    return 1;
}
