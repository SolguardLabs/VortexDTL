#include "vortexdtl.h"

#include <inttypes.h>

static void json_string(FILE *stream, const char *value) {
    const unsigned char *p = (const unsigned char *)value;
    fputc('"', stream);
    while (*p != '\0') {
        if (*p == '"' || *p == '\\') {
            fputc('\\', stream);
            fputc((int)*p, stream);
        } else if (*p >= 0x20u) {
            fputc((int)*p, stream);
        }
        p++;
    }
    fputc('"', stream);
}

static void json_u64(FILE *stream, uint64_t value) {
    fprintf(stream, "%" PRIu64, value);
}

static void print_amount_map(FILE *stream, const vdtl_context *ctx, const uint64_t values[VDTL_MAX_ASSETS]) {
    size_t i;
    int first = 1;
    fputc('{', stream);
    for (i = 0u; i < ctx->asset_count; i++) {
        if (values[i] == 0u) continue;
        if (!first) fputc(',', stream);
        json_string(stream, ctx->assets[i].symbol);
        fputc(':', stream);
        json_u64(stream, values[i]);
        first = 0;
    }
    fputc('}', stream);
}

static void print_leg_quote(FILE *stream, const vdtl_leg_quote *leg) {
    fputc('{', stream);
    fputs("\"id\":", stream);
    json_string(stream, leg->id);
    fputs(",\"from\":", stream);
    json_string(stream, leg->from);
    fputs(",\"to\":", stream);
    json_string(stream, leg->to);
    fputs(",\"input\":", stream);
    json_u64(stream, leg->input);
    fputs(",\"grossOut\":", stream);
    json_u64(stream, leg->gross_out);
    fputs(",\"feeBps\":", stream);
    fprintf(stream, "%u", leg->fee_bps);
    fputs(",\"fee\":", stream);
    json_u64(stream, leg->fee);
    fputs(",\"rebate\":", stream);
    json_u64(stream, leg->rebate);
    fputs(",\"netOut\":", stream);
    json_u64(stream, leg->net_out);
    fputs(",\"rounding\":", stream);
    json_string(stream, vdtl_rounding_name(leg->rounding));
    fputc('}', stream);
}

void vdtl_json_print_quote(FILE *stream, const vdtl_context *ctx, const vdtl_quote *quote) {
    size_t i;
    fputc('{', stream);
    fputs("\"routeId\":", stream);
    json_string(stream, quote->route_id);
    fputs(",\"sourceAsset\":", stream);
    json_string(stream, quote->source_asset);
    fputs(",\"targetAsset\":", stream);
    json_string(stream, quote->target_asset);
    fputs(",\"amountIn\":", stream);
    json_u64(stream, quote->amount_in);
    fputs(",\"finalOut\":", stream);
    json_u64(stream, quote->final_out);
    fputs(",\"legs\":[", stream);
    for (i = 0u; i < quote->leg_count; i++) {
        if (i != 0u) fputc(',', stream);
        print_leg_quote(stream, &quote->legs[i]);
    }
    fputs("],\"feeByAsset\":", stream);
    print_amount_map(stream, ctx, quote->fee_by_asset);
    fputs(",\"rebateByAsset\":", stream);
    print_amount_map(stream, ctx, quote->rebate_by_asset);
    fputc('}', stream);
}

void vdtl_json_print_compact_settlement(
    FILE *stream,
    const vdtl_context *ctx,
    const vdtl_settlement_event *event) {
    (void)ctx;
    fputc('{', stream);
    fputs("\"routeId\":", stream);
    json_string(stream, event->route_id);
    fputs(",\"sourceAsset\":", stream);
    json_string(stream, event->source_asset);
    fputs(",\"targetAsset\":", stream);
    json_string(stream, event->target_asset);
    fputs(",\"amountIn\":", stream);
    json_u64(stream, event->amount_in);
    fputs(",\"minOut\":", stream);
    json_u64(stream, event->min_out);
    fputs(",\"quoteOut\":", stream);
    json_u64(stream, event->quote.final_out);
    fputs(",\"reserveIn\":", stream);
    json_u64(stream, event->reserve.reserve_in);
    fputs(",\"finalOut\":", stream);
    json_u64(stream, event->settlement.final_out);
    fputs(",\"reservedValue\":", stream);
    json_u64(stream, event->reserved_value);
    fputs(",\"creditedValue\":", stream);
    json_u64(stream, event->credited_value);
    fputs(",\"fees\":", stream);
    print_amount_map(stream, &ctx[0], event->settlement.fee_by_asset);
    fputs(",\"rebates\":", stream);
    print_amount_map(stream, &ctx[0], event->settlement.rebate_by_asset);
    fputc('}', stream);
}

static void print_account(FILE *stream, const vdtl_ledger *ledger, vdtl_account_role role) {
    size_t i;
    fputc('{', stream);
    for (i = 0u; i < ledger->ctx.asset_count; i++) {
        if (i != 0u) fputc(',', stream);
        json_string(stream, ledger->ctx.assets[i].symbol);
        fputc(':', stream);
        json_u64(stream, ledger->accounts[role][i]);
    }
    fputc('}', stream);
}

static void print_vault(FILE *stream, const vdtl_vault *vault) {
    fputc('{', stream);
    fputs("\"asset\":", stream);
    json_string(stream, vault->asset);
    fputs(",\"available\":", stream);
    json_u64(stream, vault->available);
    fputs(",\"reserved\":", stream);
    json_u64(stream, vault->reserved);
    fputs(",\"paid\":", stream);
    json_u64(stream, vault->paid);
    fputs(",\"received\":", stream);
    json_u64(stream, vault->received);
    fputc('}', stream);
}

void vdtl_json_print_ledger(FILE *stream, const vdtl_ledger *ledger) {
    size_t i;
    fputs("{\"accounts\":{\"trader\":", stream);
    print_account(stream, ledger, VDTL_ACCOUNT_TRADER);
    fputs(",\"maker\":", stream);
    print_account(stream, ledger, VDTL_ACCOUNT_MAKER);
    fputs(",\"protocol\":", stream);
    print_account(stream, ledger, VDTL_ACCOUNT_PROTOCOL);
    fputs("},\"vaults\":{", stream);
    for (i = 0u; i < ledger->ctx.asset_count; i++) {
        if (i != 0u) fputc(',', stream);
        json_string(stream, ledger->ctx.assets[i].symbol);
        fputc(':', stream);
        print_vault(stream, &ledger->vaults[i]);
    }
    fputs("},\"totals\":{\"reservedValue\":", stream);
    json_u64(stream, ledger->totals.reserved_value);
    fputs(",\"creditedValue\":", stream);
    json_u64(stream, ledger->totals.credited_value);
    fputs(",\"orders\":", stream);
    json_u64(stream, ledger->totals.orders);
    fputs("},\"events\":", stream);
    json_u64(stream, (uint64_t)ledger->events);
    fputc('}', stream);
}

void vdtl_json_print_demo(FILE *stream, const vdtl_settlement_event *a,
                          const vdtl_settlement_event *b, const vdtl_ledger *ledger) {
    fputs("{\"scenario\":\"demo\",\"orders\":2,\"direct\":", stream);
    vdtl_json_print_compact_settlement(stream, &ledger->ctx, a);
    fputs(",\"hop\":", stream);
    vdtl_json_print_compact_settlement(stream, &ledger->ctx, b);
    fputs(",\"ledger\":", stream);
    vdtl_json_print_ledger(stream, ledger);
    fputc('}', stream);
}

void vdtl_json_print_route_policy(FILE *stream, const vdtl_route_policy_report *report) {
    fputc('{', stream);
    fputs("\"routeId\":", stream);
    json_string(stream, report->route_id);
    fputs(",\"ok\":", stream);
    fputs(report->ok ? "true" : "false", stream);
    fputs(",\"legCount\":", stream);
    json_u64(stream, report->leg_count);
    fputs(",\"totalFeeBps\":", stream);
    json_u64(stream, report->total_fee_bps);
    fputs(",\"totalRebateBps\":", stream);
    json_u64(stream, report->total_rebate_bps);
    fputs(",\"maxLegFeeBps\":", stream);
    json_u64(stream, report->max_leg_fee_bps);
    fputs(",\"maxUtilizationBps\":", stream);
    json_u64(stream, report->max_utilization_bps);
    fputs(",\"warnings\":", stream);
    json_u64(stream, report->warnings);
    fputc('}', stream);
}

static void print_vault_metric(FILE *stream, const vdtl_vault_metric *metric) {
    fputc('{', stream);
    fputs("\"asset\":", stream);
    json_string(stream, metric->asset);
    fputs(",\"available\":", stream);
    json_u64(stream, metric->available);
    fputs(",\"paid\":", stream);
    json_u64(stream, metric->paid);
    fputs(",\"received\":", stream);
    json_u64(stream, metric->received);
    fputs(",\"availableValue\":", stream);
    json_u64(stream, metric->available_value);
    fputs(",\"paidValue\":", stream);
    json_u64(stream, metric->paid_value);
    fputs(",\"receivedValue\":", stream);
    json_u64(stream, metric->received_value);
    fputs(",\"utilizationBps\":", stream);
    json_u64(stream, metric->utilization_bps);
    fputc('}', stream);
}

void vdtl_json_print_ledger_health(FILE *stream, const vdtl_ledger_health *health) {
    size_t i;
    fputs("{\"vaultCount\":", stream);
    json_u64(stream, (uint64_t)health->vault_count);
    fputs(",\"totalAvailableValue\":", stream);
    json_u64(stream, health->total_available_value);
    fputs(",\"totalPaidValue\":", stream);
    json_u64(stream, health->total_paid_value);
    fputs(",\"totalReceivedValue\":", stream);
    json_u64(stream, health->total_received_value);
    fputs(",\"traderValue\":", stream);
    json_u64(stream, health->trader_value);
    fputs(",\"makerValue\":", stream);
    json_u64(stream, health->maker_value);
    fputs(",\"protocolValue\":", stream);
    json_u64(stream, health->protocol_value);
    fputs(",\"stressedVaults\":", stream);
    json_u64(stream, health->stressed_vaults);
    fputs(",\"vaults\":[", stream);
    for (i = 0u; i < health->vault_count; i++) {
        if (i != 0u) fputc(',', stream);
        print_vault_metric(stream, &health->vaults[i]);
    }
    fputs("]}", stream);
}

void vdtl_json_print_liquidity_projection(
    FILE *stream,
    const vdtl_liquidity_projection *projection) {
    fputs("{\"asset\":", stream);
    json_string(stream, projection->asset);
    fputs(",\"available\":", stream);
    json_u64(stream, projection->available);
    fputs(",\"requestedPayout\":", stream);
    json_u64(stream, projection->requested_payout);
    fputs(",\"bufferBps\":", stream);
    json_u64(stream, projection->buffer_bps);
    fputs(",\"requiredBuffer\":", stream);
    json_u64(stream, projection->required_buffer);
    fputs(",\"projectedAvailable\":", stream);
    json_u64(stream, projection->projected_available);
    fputs(",\"headroom\":", stream);
    json_u64(stream, projection->headroom);
    fputs(",\"unfunded\":", stream);
    json_u64(stream, projection->unfunded);
    fputs(",\"withinPolicy\":", stream);
    fputs(projection->within_policy ? "true" : "false", stream);
    fputc('}', stream);
}
