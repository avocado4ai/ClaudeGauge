#pragma once
#include "data_models.h"

// ============================================================
// Data Bindings — maps designer Data Source selections to AppState fields
// ============================================================

enum DataBinding {
    BIND_NONE = 0,
    // Claude.ai subscription utilization (0-100%)
    BIND_AI_5H_PCT,
    BIND_AI_7D_PCT,
    BIND_AI_7D_OPUS_PCT,
    BIND_AI_7D_SONNET_PCT,
    // Cost (USD float)
    BIND_COST_TODAY,
    BIND_COST_MONTH,
    // Token usage (0-100% of total)
    BIND_TOKENS_INPUT_PCT,
    BIND_TOKENS_OUTPUT_PCT,
    // Claude Code metrics
    BIND_CC_SESSIONS,
    BIND_CC_LINES_ADD,
    BIND_CC_LINES_RM,
    BIND_CC_COMMITS,
    BIND_CC_PRS,
    BIND_CC_COST,
    BIND_CC_EDIT_PCT,
    // Claude.ai reset countdowns (seconds remaining)
    BIND_AI_5H_CLOCK,
    BIND_AI_7D_CLOCK,
    // System
    BIND_WIFI_RSSI,
    BIND_UPTIME,
    BIND_FREE_HEAP,
};

// Resolve a binding to a float value.
// For percentage bindings: returns 0-100.
// For count bindings: returns the raw count.
// For cost bindings: returns USD value.
inline float resolveBinding(DataBinding bind, const AppState& s) {
    switch (bind) {
        case BIND_AI_5H_PCT:         return s.claude_ai.five_hour.utilization;
        case BIND_AI_7D_PCT:         return s.claude_ai.seven_day.utilization;
        case BIND_AI_7D_OPUS_PCT:    return s.claude_ai.seven_day_opus.utilization;
        case BIND_AI_7D_SONNET_PCT:  return s.claude_ai.seven_day_sonnet.utilization;
        case BIND_COST_TODAY:        return s.cost.today_usd;
        case BIND_COST_MONTH:        return s.cost.month_usd;
        case BIND_TOKENS_INPUT_PCT: {
            uint64_t total = s.usage.today_total.total();
            return total > 0 ? (float)s.usage.today_total.uncached_input * 100.0f / total : 0;
        }
        case BIND_TOKENS_OUTPUT_PCT: {
            uint64_t total = s.usage.today_total.total();
            return total > 0 ? (float)s.usage.today_total.output * 100.0f / total : 0;
        }
        case BIND_CC_SESSIONS:       return (float)s.code.total_sessions;
        case BIND_CC_LINES_ADD:      return (float)s.code.total_lines_added;
        case BIND_CC_LINES_RM:       return (float)s.code.total_lines_removed;
        case BIND_CC_COMMITS:        return (float)s.code.total_commits;
        case BIND_CC_PRS:            return (float)s.code.total_prs;
        case BIND_CC_COST:           return s.code.total_cost;
        case BIND_CC_EDIT_PCT: {
            uint16_t total = s.code.total_edit_accepted + s.code.total_edit_rejected;
            return total > 0 ? (float)s.code.total_edit_accepted * 100.0f / total : 0;
        }
        case BIND_AI_5H_CLOCK: {
            time_t now; time(&now);
            int32_t remaining = (int32_t)(s.claude_ai.five_hour.resets_at - now);
            return remaining > 0 ? (float)remaining : 0;
        }
        case BIND_AI_7D_CLOCK: {
            time_t now; time(&now);
            int32_t remaining = (int32_t)(s.claude_ai.seven_day.resets_at - now);
            return remaining > 0 ? (float)remaining : 0;
        }
        case BIND_WIFI_RSSI:         return (float)s.wifi_rssi;
        case BIND_UPTIME:            return (float)((millis() - s.uptime_start) / 1000);
        case BIND_FREE_HEAP:         return (float)(ESP.getFreeHeap() / 1024);
        default:                     return 0;
    }
}

// Format a binding value as text for display
inline void formatBinding(DataBinding bind, float value, char* buf, size_t bufLen) {
    switch (bind) {
        case BIND_AI_5H_PCT:
        case BIND_AI_7D_PCT:
        case BIND_AI_7D_OPUS_PCT:
        case BIND_AI_7D_SONNET_PCT:
        case BIND_TOKENS_INPUT_PCT:
        case BIND_TOKENS_OUTPUT_PCT:
        case BIND_CC_EDIT_PCT:
            snprintf(buf, bufLen, "%.0f%%", value);
            break;
        case BIND_COST_TODAY:
        case BIND_COST_MONTH:
        case BIND_CC_COST:
            snprintf(buf, bufLen, "$%.2f", value);
            break;
        case BIND_AI_5H_CLOCK:
        case BIND_AI_7D_CLOCK: {
            int32_t secs = (int32_t)value;
            if (secs <= 0) { snprintf(buf, bufLen, "0:00"); break; }
            int32_t d = secs / 86400;
            int32_t h = (secs % 86400) / 3600;
            int32_t m = (secs % 3600) / 60;
            int32_t s = secs % 60;
            if (d > 0)      snprintf(buf, bufLen, "%ldd %ldh%ldm", (long)d, (long)h, (long)m);
            else if (h > 0) snprintf(buf, bufLen, "%ld:%02ld:%02ld", (long)h, (long)m, (long)s);
            else            snprintf(buf, bufLen, "%ld:%02ld", (long)m, (long)s);
            break;
        }
        case BIND_WIFI_RSSI:
            snprintf(buf, bufLen, "%d dBm", (int)value);
            break;
        case BIND_UPTIME: {
            uint32_t sec = (uint32_t)value;
            if (sec >= 3600)
                snprintf(buf, bufLen, "%luh %lum", (unsigned long)(sec/3600), (unsigned long)((sec%3600)/60));
            else
                snprintf(buf, bufLen, "%lum %lus", (unsigned long)(sec/60), (unsigned long)(sec%60));
            break;
        }
        case BIND_FREE_HEAP:
            snprintf(buf, bufLen, "%luK", (unsigned long)value);
            break;
        default:
            snprintf(buf, bufLen, "%.0f", value);
            break;
    }
}

// Is this a percentage binding? (0-100 range, suitable for gauges/bars)
inline bool isPercentBinding(DataBinding bind) {
    return bind == BIND_AI_5H_PCT || bind == BIND_AI_7D_PCT ||
           bind == BIND_AI_7D_OPUS_PCT || bind == BIND_AI_7D_SONNET_PCT ||
           bind == BIND_TOKENS_INPUT_PCT || bind == BIND_TOKENS_OUTPUT_PCT ||
           bind == BIND_CC_EDIT_PCT;
}
