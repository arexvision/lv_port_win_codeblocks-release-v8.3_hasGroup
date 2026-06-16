#ifndef DEBUG_LINK_PC_H
#define DEBUG_LINK_PC_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (*debug_link_pc_rtc_offline_fn)(uint32_t seconds, char *err_buf, uint16_t err_buf_size);

void debug_link_pc_start(void);
void debug_link_pc_set_rtc_offline_handler(debug_link_pc_rtc_offline_fn handler);
bool debug_link_pc_manual_mode(void);
bool debug_link_pc_consume_connect_event(void);
uint16_t debug_link_pc_time_scale(void);
bool debug_link_pc_depth_goto_step(float current_depth_m, float *out_depth_m, bool *out_reached);

#ifdef __cplusplus
}
#endif

#ifdef DEBUG_LINK_PC_IMPLEMENTATION

#if defined(PC_SIMULATOR) && defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>

#include "../ui/alarm/alarm.h"
#include "../ui/core/data.h"
#include "../ui/core/ui_engine.h"
#include "../ui/core/ui_state.h"
#include "sim_alert_policy.h"
#include "lvgl/lvgl.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DEBUG_TCP_PORT 7623U
#define DEBUG_RX_BUF_SIZE 512U
#define DEBUG_TX_BUF_SIZE 768U
#define DEBUG_DEPTH_GOTO_DESCENT_MPM 18.0f
#define DEBUG_DEPTH_GOTO_ASCENT_MPM 10.0f
#define DEBUG_DEPTH_GOTO_MIN_MPM 0.1f
#define DEBUG_DEPTH_GOTO_MAX_MPM 120.0f
#define DEBUG_DEPTH_GOTO_EPSILON_M 0.01f
#define DEBUG_RTC_VALID_AFTER_EPOCH 1577836800LL
#define DEBUG_RTC_OFFLINE_MAX_SECONDS (30UL * 24UL * 60UL * 60UL)

typedef int (WSAAPI *wsa_startup_fn)(WORD, LPWSADATA);
typedef int (WSAAPI *wsa_cleanup_fn)(void);
typedef int (WSAAPI *wsa_get_last_error_fn)(void);
typedef SOCKET (WSAAPI *socket_fn)(int, int, int);
typedef int (WSAAPI *bind_fn)(SOCKET, const struct sockaddr *, int);
typedef int (WSAAPI *listen_fn)(SOCKET, int);
typedef SOCKET (WSAAPI *accept_fn)(SOCKET, struct sockaddr *, int *);
typedef int (WSAAPI *recv_fn)(SOCKET, char *, int, int);
typedef int (WSAAPI *send_fn)(SOCKET, const char *, int, int);
typedef int (WSAAPI *closesocket_fn)(SOCKET);
typedef int (WSAAPI *ioctlsocket_fn)(SOCKET, long, u_long *);

typedef struct
{
    HMODULE dll;
    bool loaded;
    bool started;
    bool manual_mode;
    bool connect_event;
    SOCKET listener;
    SOCKET client;
    lv_timer_t *timer;
    char rx_buf[DEBUG_RX_BUF_SIZE];
    uint16_t rx_len;
    uint16_t time_scale;
    uint32_t sample_time_s;
    bool depth_rate_valid;
    float depth_rate_last_m;
    uint32_t depth_rate_last_tick_ms;
    bool depth_goto_active;
    float depth_goto_target_m;
    float depth_goto_rate_mpm;
    bool rtc_sleep_mark_valid;
    time_t rtc_sleep_mark;
    debug_link_pc_rtc_offline_fn rtc_offline_handler;

    wsa_startup_fn WSAStartup_;
    wsa_cleanup_fn WSACleanup_;
    wsa_get_last_error_fn WSAGetLastError_;
    socket_fn socket_;
    bind_fn bind_;
    listen_fn listen_;
    accept_fn accept_;
    recv_fn recv_;
    send_fn send_;
    closesocket_fn closesocket_;
    ioctlsocket_fn ioctlsocket_;
} debug_link_pc_t;

static debug_link_pc_t s_debug_link =
{
    .listener = INVALID_SOCKET,
    .client = INVALID_SOCKET,
    .time_scale = 1,
};

static uint16_t debug_swap16(uint16_t value)
{
    return (uint16_t)(((value & 0x00FFU) << 8) | ((value & 0xFF00U) >> 8));
}

static bool debug_load_winsock(void)
{
    if (s_debug_link.loaded)
    {
        return true;
    }

    s_debug_link.dll = LoadLibraryA("Ws2_32.dll");
    if (!s_debug_link.dll)
    {
        printf("[DBG] Ws2_32.dll load failed\r\n");
        return false;
    }

#define DBG_LOAD_PROC(field, name, type)                                      \
    do                                                                            \
    {                                                                             \
        s_debug_link.field = (type)GetProcAddress(s_debug_link.dll, name);         \
        if (!s_debug_link.field)                                                   \
        {                                                                         \
            printf("[DBG] missing Winsock proc: %s\r\n", name);                   \
            FreeLibrary(s_debug_link.dll);                                        \
            s_debug_link.dll = NULL;                                              \
            return false;                                                         \
        }                                                                         \
    } while (0)

    DBG_LOAD_PROC(WSAStartup_, "WSAStartup", wsa_startup_fn);
    DBG_LOAD_PROC(WSACleanup_, "WSACleanup", wsa_cleanup_fn);
    DBG_LOAD_PROC(WSAGetLastError_, "WSAGetLastError", wsa_get_last_error_fn);
    DBG_LOAD_PROC(socket_, "socket", socket_fn);
    DBG_LOAD_PROC(bind_, "bind", bind_fn);
    DBG_LOAD_PROC(listen_, "listen", listen_fn);
    DBG_LOAD_PROC(accept_, "accept", accept_fn);
    DBG_LOAD_PROC(recv_, "recv", recv_fn);
    DBG_LOAD_PROC(send_, "send", send_fn);
    DBG_LOAD_PROC(closesocket_, "closesocket", closesocket_fn);
    DBG_LOAD_PROC(ioctlsocket_, "ioctlsocket", ioctlsocket_fn);

#undef DBG_LOAD_PROC

    s_debug_link.loaded = true;
    return true;
}

static int debug_last_error(void)
{
    return s_debug_link.WSAGetLastError_ ? s_debug_link.WSAGetLastError_() : 0;
}

static bool debug_is_would_block(int err)
{
    return err == WSAEWOULDBLOCK || err == WSAEINPROGRESS || err == WSAEALREADY;
}

static void debug_close_socket(SOCKET *sock)
{
    if (sock && *sock != INVALID_SOCKET && s_debug_link.closesocket_)
    {
        s_debug_link.closesocket_(*sock);
        *sock = INVALID_SOCKET;
    }
}

static bool debug_set_nonblocking(SOCKET sock)
{
    u_long mode = 1;
    return s_debug_link.ioctlsocket_(sock, FIONBIO, &mode) == 0;
}

static void debug_send_raw(const char *text)
{
    size_t len;

    if (!text || s_debug_link.client == INVALID_SOCKET)
    {
        return;
    }

    len = strlen(text);
    if (len > 0U)
    {
        (void)s_debug_link.send_(s_debug_link.client, text, (int)len, 0);
    }
}

static void debug_sendf(const char *fmt, ...)
{
    char out[DEBUG_TX_BUF_SIZE];
    va_list ap;

    if (s_debug_link.client == INVALID_SOCKET)
    {
        return;
    }

    va_start(ap, fmt);
    vsnprintf(out, sizeof(out), fmt, ap);
    va_end(ap);
    debug_send_raw(out);
}

static char *debug_trim(char *text)
{
    char *end;

    if (!text)
    {
        return text;
    }
    while (*text && isspace((unsigned char)*text))
    {
        text++;
    }
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1]))
    {
        *--end = '\0';
    }
    return text;
}

static char *debug_next_token(char **cursor)
{
    char *start;

    if (!cursor || !*cursor)
    {
        return NULL;
    }
    while (**cursor && isspace((unsigned char)**cursor))
    {
        (*cursor)++;
    }
    if (**cursor == '\0')
    {
        return NULL;
    }
    start = *cursor;
    while (**cursor && !isspace((unsigned char)**cursor))
    {
        (*cursor)++;
    }
    if (**cursor)
    {
        **cursor = '\0';
        (*cursor)++;
    }
    return start;
}

static bool debug_streq(const char *a, const char *b)
{
    if (!a || !b)
    {
        return false;
    }
    while (*a && *b)
    {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
        {
            return false;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static bool debug_parse_float(const char *text, float *out)
{
    char *end = NULL;
    float value;

    if (!text || !out)
    {
        return false;
    }
    value = strtof(text, &end);
    if (end == text || (end && *end != '\0'))
    {
        return false;
    }
    *out = value;
    return true;
}

static bool debug_parse_int(const char *text, int *out)
{
    char *end = NULL;
    long value;

    if (!text || !out)
    {
        return false;
    }
    value = strtol(text, &end, 10);
    if (end == text || (end && *end != '\0'))
    {
        return false;
    }
    *out = (int)value;
    return true;
}

static bool debug_parse_bool(const char *text, bool *out)
{
    if (!text || !out)
    {
        return false;
    }
    if (debug_streq(text, "1") ||
            debug_streq(text, "on") ||
            debug_streq(text, "true") ||
            debug_streq(text, "yes") ||
            debug_streq(text, "manual"))
    {
        *out = true;
        return true;
    }
    if (debug_streq(text, "0") ||
            debug_streq(text, "off") ||
            debug_streq(text, "false") ||
            debug_streq(text, "no") ||
            debug_streq(text, "auto"))
    {
        *out = false;
        return true;
    }
    return false;
}

static void debug_format_gas_name(char *out, size_t out_size, int o2, int he)
{
    if (!out || out_size == 0U)
    {
        return;
    }
    if (o2 <= 0)
    {
        snprintf(out, out_size, "--");
    }
    else if (he > 0)
    {
        snprintf(out, out_size, "TX %d/%d", o2, he);
    }
    else if (o2 == 21)
    {
        snprintf(out, out_size, "AIR");
    }
    else if (o2 == 100)
    {
        snprintf(out, out_size, "O2 100%%");
    }
    else
    {
        snprintf(out, out_size, "NX %d", o2);
    }
}

static void debug_exec_line(char *line);

static void debug_update_gas_derived(void)
{
    float fio2 = (float)bus_get_gas_mix_o2() / 100.0f;
    float fihe = (float)bus_get_gas_mix_he() / 100.0f;
    float fin2 = 1.0f - fio2 - fihe;
    float ambient_ata = 1.0f + g_sensor_data.depth / 10.0f;
    float surface_density = fio2 * 1.428f + fihe * 0.179f + fin2 * 1.251f;

    bus_set_fio2(fio2 * 100.0f);
    bus_set_gas_density(surface_density * ambient_ata);
}

static void debug_depth_goto_cancel(void)
{
    s_debug_link.depth_goto_active = false;
    s_debug_link.depth_goto_target_m = 0.0f;
    s_debug_link.depth_goto_rate_mpm = 0.0f;
}

static bool debug_rtc_now(time_t *out_now)
{
    time_t now;

    if (!out_now)
    {
        return false;
    }

    now = time(NULL);
    if (now < (time_t)DEBUG_RTC_VALID_AFTER_EPOCH)
    {
        return false;
    }

    *out_now = now;
    return true;
}

static bool debug_apply_rtc_offline_seconds(uint32_t seconds, const char *ok_name)
{
    char err_buf[96];

    if (seconds == 0U)
    {
        debug_send_raw("ERR usage: rtc_offline <seconds>\r\n");
        return false;
    }
    if (s_debug_link.rtc_offline_handler == NULL)
    {
        debug_send_raw("ERR rtc_offline unavailable\r\n");
        return false;
    }

    err_buf[0] = '\0';
    if (!s_debug_link.rtc_offline_handler(seconds, err_buf, (uint16_t)sizeof(err_buf)))
    {
        debug_sendf("ERR %s\r\n", err_buf[0] != '\0' ? err_buf : "rtc_offline failed");
        return false;
    }

    debug_depth_goto_cancel();
    bus_set_ascent_rate(0.0f);
    s_debug_link.depth_rate_valid = false;
    debug_sendf("OK %s %lu AIR\r\n", ok_name ? ok_name : "rtc_offline", (unsigned long)seconds);
    return true;
}

static void debug_rtc_sleep_status(void)
{
    time_t now;
    long long offline_s = 0;

    if (!debug_rtc_now(&now))
    {
        debug_send_raw("ERR rtc invalid\r\n");
        return;
    }

    if (s_debug_link.rtc_sleep_mark_valid && now > s_debug_link.rtc_sleep_mark)
    {
        offline_s = (long long)(now - s_debug_link.rtc_sleep_mark);
    }

    debug_sendf("OK rtc_sleep_status valid=%u mark=%lld now=%lld offline=%lld\r\n",
                s_debug_link.rtc_sleep_mark_valid ? 1U : 0U,
                (long long)s_debug_link.rtc_sleep_mark,
                (long long)now,
                offline_s);
}

static bool debug_depth_goto_start(float target_depth_m, float rate_mpm)
{
    if (target_depth_m < 0.0f)
    {
        target_depth_m = 0.0f;
    }
    if (rate_mpm < 0.0f)
    {
        rate_mpm = -rate_mpm;
    }
    if (rate_mpm > 0.0f && (rate_mpm < DEBUG_DEPTH_GOTO_MIN_MPM || rate_mpm > DEBUG_DEPTH_GOTO_MAX_MPM))
    {
        debug_sendf("ERR goto speed %.1f out of range %.1f..%.0f m/min\r\n", (double)rate_mpm, (double)DEBUG_DEPTH_GOTO_MIN_MPM, (double)DEBUG_DEPTH_GOTO_MAX_MPM);
        return false;
    }

    s_debug_link.depth_goto_active = true;
    s_debug_link.depth_goto_target_m = target_depth_m;
    s_debug_link.depth_goto_rate_mpm = rate_mpm;
    s_debug_link.depth_rate_valid = false;
    return true;
}

static void debug_apply_depth_sample(float depth)
{
    uint32_t sample_time_s;
    uint32_t now_ms;

    if (depth < 0.0f)
    {
        depth = 0.0f;
    }

    debug_depth_goto_cancel();

    sample_time_s = g_sensor_data.dive_time_s;
    s_debug_link.sample_time_s = sample_time_s;
    now_ms = lv_tick_get();

    if (s_debug_link.depth_rate_valid)
    {
        uint32_t delta_ms = now_ms - s_debug_link.depth_rate_last_tick_ms;
        if (delta_ms > 0U)
        {
            float delta_min = (float)delta_ms / 60000.0f;
            bus_set_ascent_rate((s_debug_link.depth_rate_last_m - depth) / delta_min);
        }
    }
    else
    {
        bus_set_ascent_rate(0.0f);
    }
    s_debug_link.depth_rate_valid = true;
    s_debug_link.depth_rate_last_m = depth;
    s_debug_link.depth_rate_last_tick_ms = now_ms;

    dive_log_append_sampled((float)sample_time_s, depth);
    bus_set_depth(depth);
    debug_update_gas_derived();
    sim_alert_tick();
}

static bool debug_try_packet_line_rx(const char *data, int len)
{
    char text[DEBUG_RX_BUF_SIZE];
    char *trimmed;
    uint16_t text_len = 0U;

    if (!data || len <= 0)
    {
        return false;
    }

    for (int i = 0; i < len; i++)
    {
        if (data[i] == '\r' || data[i] == '\n')
        {
            return false;
        }
    }

    if ((uint32_t)s_debug_link.rx_len + (uint32_t)len >= sizeof(text))
    {
        s_debug_link.rx_len = 0;
        debug_send_raw("ERR line too long\r\n");
        return true;
    }

    if (s_debug_link.rx_len > 0U)
    {
        memcpy(text, s_debug_link.rx_buf, s_debug_link.rx_len);
        text_len = s_debug_link.rx_len;
        s_debug_link.rx_len = 0;
    }

    memcpy(&text[text_len], data, (size_t)len);
    text_len = (uint16_t)(text_len + (uint16_t)len);
    text[text_len] = '\0';

    trimmed = debug_trim(text);
    if (!trimmed || trimmed[0] == '\0')
    {
        return true;
    }

    debug_exec_line(trimmed);
    return true;
}

static void debug_send_help(void)
{
    debug_send_raw(
        "TCP debug commands:\r\n"
        "  <number> writes depth directly and appends one trajectory sample\r\n"
        "  help | state | back [2] | manual on|off | auto on|off | speed <1..120>\r\n"
        "  depth <m> | goto <m> [m_min]|stop | sample <time_s> <depth_m> | rate <m_min> | time <s> | surface <s>\r\n"
        "  rtc_offline <seconds> | rtc_sleep_mark | rtc_sleep_apply | rtc_sleep_status\r\n"
        "  ndl <min> | tts <min> | stop <none|safety|deco> <ndl> <depth> <total_s> <left_s> <zone0|1>\r\n"
        "  pod <0|1> <bar> | batt <pct> | temp <c> | bat_temp <c> | prj_temp <c>\r\n"
        "  heading <deg> | ppo2 <slot> <bar> | gf <low> <high> | gf99 <pct> | surf_gf <pct>\r\n"
        "  last_deco <3|6> | final_stop <3|6>\r\n"
        "  cns <pct> | otu <value> | mod <m> | ceiling <m> | mix <o2> <he> | dens <g_l> | fio2 <pct>\r\n"
        "  gas_count <n> | gas <slot> [name] | gas_slot <slot> <o2> <he> <mod> [name]\r\n"
        "  layout <default|current|gas|empty|side|top|bottom>\r\n"
        "  a <id> | a clear [id|all] | a auto on|off | a list\r\n"
        "  alarm <info|warn|crit> <text> | alarm clear\r\n"
        "  alert ids: asc po2 po2min po2w ceil lock batt dead ndl cns otu safety depth time ss done\r\n"
        "Slots are 0-based. TCP disables the auto depth script; goto defaults to 18m/min down, 10m/min up; optional goto speed uses simulated time.\r\n");
}

static void debug_send_state(void)
{
    debug_sendf(
        "STATE tcp=%u depth_manual=%u manual=%u speed=%u goto=%u target=%.1f goto_rate=%.1f depth=%.1f rate=%+.1f time=%lu gas=%u:%s batt=%.0f temp=%.1f pod=%.0f/%.0f gf=%u/%u last_deco=%um\r\n",
        s_debug_link.client != INVALID_SOCKET ? 1U : 0U,
        (s_debug_link.manual_mode || s_debug_link.client != INVALID_SOCKET) ? 1U : 0U,
        s_debug_link.manual_mode ? 1U : 0U,
        (unsigned)debug_link_pc_time_scale(),
        s_debug_link.depth_goto_active ? 1U : 0U,
        (double)s_debug_link.depth_goto_target_m,
        (double)s_debug_link.depth_goto_rate_mpm,
        (double)g_sensor_data.depth,
        (double)g_sensor_data.ascent_rate,
        (unsigned long)g_sensor_data.dive_time_s,
        (unsigned)g_sensor_data.gas_active_idx,
        g_sensor_data.gas_name,
        (double)g_sensor_data.battery_pct,
        (double)g_sensor_data.temperature_c,
        (double)g_sensor_data.pod1_bar,
        (double)g_sensor_data.pod2_bar,
        (unsigned)g_sensor_data.gf_low,
        (unsigned)g_sensor_data.gf_high,
        (unsigned)((g_sys_config.last_deco_stop_m == 6U) ? 6U : 3U));
}

static void debug_layout_fill_left(ble_ui_sync_payload_t *payload)
{
    static const uint8_t side_def[][3] =
    {
        { COMP_NDL_STOP_1606,  0, 0 },
        { COMP_DEPTH_1612,     0, 1 },
        { COMP_DIVE_TIME_1606, 0, 3 },
        { COMP_GAS_1606,       0, 4 },
        { COMP_EMPTY,          0, 5 },
        { COMP_EMPTY,          1, 5 },
        { COMP_SYS_1606,       0, 6 },
    };
    static const uint8_t top_def[][3] =
    {
        { COMP_NDL_STOP_1606,  0, 0 },
        { COMP_DEPTH_1612,     2, 0 },
        { COMP_DIVE_TIME_1606, 4, 0 },
        { COMP_GAS_1606,       4, 1 },
        { COMP_TEMP_0806,      6, 0 },
        { COMP_BATTERY_0806,   6, 1 },
    };
    const uint8_t (*items)[3] = ui_layout_is_vertical_split() ? side_def : top_def;
    uint8_t count = (uint8_t)(ui_layout_is_vertical_split() ? (sizeof(side_def) / sizeof(side_def[0]))
                                                            : (sizeof(top_def) / sizeof(top_def[0])));

    payload->left_count = count;
    for (uint8_t i = 0U; i < payload->left_count; i++)
    {
        payload->left_widgets[i].widget_id = items[i][0];
        payload->left_widgets[i].x = items[i][1];
        payload->left_widgets[i].y = items[i][2];
    }
}

static void debug_layout_fill_custom(ble_ui_sync_payload_t *payload, bool gas_layout)
{
    static const uint8_t side_default[][3] =
    {
        { COMP_DEPTH_1606,     0, 0 },
        { COMP_PPO2_0806,      0, 2 },
        { COMP_BATTERY_0806,   0, 3 },
        { COMP_POD_0806,       0, 4 },
        { COMP_NDL_STOP_1606,  1, 0 },
        { COMP_CNS_0806,       1, 2 },
        { COMP_OTU_0806,       1, 3 },
        { COMP_HEADING_0806,   1, 4 },
        { COMP_GAS_1606,       2, 0 },
        { COMP_DIVE_TIME_1606, 2, 2 },
    };
    static const uint8_t side_gas[][3] =
    {
        { COMP_GAS_1606,       0, 0 },
        { COMP_PPO2_0806,      0, 2 },
        { COMP_MOD_0806,       0, 3 },
        { COMP_FIO2_0806,      0, 4 },
        { COMP_GAS_MIX_1606,   1, 0 },
        { COMP_GAS_DENS_0806,  1, 2 },
    };
    static const uint8_t top_default[][3] =
    {
        { COMP_DEPTH_1606,     0, 0 },
        { COMP_PPO2_0806,      0, 2 },
        { COMP_BATTERY_0806,   0, 3 },
        { COMP_POD_0806,       0, 4 },
        { COMP_NDL_STOP_1606,  1, 0 },
        { COMP_CNS_0806,       1, 2 },
        { COMP_OTU_0806,       1, 3 },
        { COMP_HEADING_0806,   1, 4 },
        { COMP_GAS_1606,       2, 0 },
        { COMP_DIVE_TIME_1606, 2, 2 },
    };
    static const uint8_t top_gas[][3] =
    {
        { COMP_GAS_1606,       0, 0 },
        { COMP_PPO2_0806,      2, 0 },
        { COMP_MOD_0806,       3, 0 },
        { COMP_FIO2_0806,      4, 0 },
        { COMP_GAS_MIX_1606,   0, 1 },
        { COMP_GAS_DENS_0806,  2, 1 },
    };
    const bool horizontal = !ui_layout_is_vertical_split();
    const uint8_t (*items)[3] = horizontal ? (gas_layout ? top_gas : top_default)
                                           : (gas_layout ? side_gas : side_default);
    uint8_t count = (uint8_t)(horizontal
        ? (gas_layout ? (sizeof(top_gas) / sizeof(top_gas[0])) : (sizeof(top_default) / sizeof(top_default[0])))
        : (gas_layout ? (sizeof(side_gas) / sizeof(side_gas[0])) : (sizeof(side_default) / sizeof(side_default[0]))));

    payload->custom_5f_count = count;
    for (uint8_t i = 0U; i < count; i++)
    {
        payload->custom_5f_widgets[i].widget_id = items[i][0];
        payload->custom_5f_widgets[i].r = items[i][1];
        payload->custom_5f_widgets[i].c = items[i][2];
    }
}

static void debug_apply_layout_profile(bool gas_layout)
{
    static ble_ui_sync_payload_t payload;
    static const uint8_t card_order[8] =
    {
        PAGE_ID_BLANK,
        PAGE_ID_COMPASS,
        PAGE_ID_DECO,
        PAGE_ID_PLAN,
        PAGE_ID_GAS,
        PAGE_ID_CUSTOM_GRID,
        PAGE_ID_UNUSED,
        PAGE_ID_UNUSED,
    };

    memset(&payload, 0, sizeof(payload));
    payload.version = BLE_CFG_VERSION;
    memcpy(payload.card_order, card_order, sizeof(payload.card_order));
    debug_layout_fill_left(&payload);
    debug_layout_fill_custom(&payload, gas_layout);
    bus_set_ui_layout(&payload);
}

static void debug_apply_empty_custom_layout_profile(void)
{
    static ble_ui_sync_payload_t payload;
    static const uint8_t card_order[8] =
    {
        PAGE_ID_COMPASS,
        PAGE_ID_CUSTOM_GRID,
        PAGE_ID_BLANK,
        PAGE_ID_DECO,
        PAGE_ID_PLAN,
        PAGE_ID_GAS,
        PAGE_ID_UNUSED,
        PAGE_ID_UNUSED,
    };

    memset(&payload, 0, sizeof(payload));
    payload.version = BLE_CFG_VERSION;
    memcpy(payload.card_order, card_order, sizeof(payload.card_order));
    debug_layout_fill_left(&payload);
    payload.custom_5f_count = 0U;
    bus_set_ui_layout(&payload);
}

static void debug_exec_line(char *line)
{
    char *cursor;
    char *cmd;
    char *arg;

    line = debug_trim(line);
    if (!line || line[0] == '\0')
    {
        return;
    }

    cursor = line;

    {
        float direct_depth;
        if (debug_parse_float(line, &direct_depth))
        {
            debug_apply_depth_sample(direct_depth);
            debug_sendf("OK depth %.1f\r\n", (double)g_sensor_data.depth);
            return;
        }
    }

    cmd = debug_next_token(&cursor);
    if (!cmd)
    {
        return;
    }

    if (debug_streq(cmd, "help") || debug_streq(cmd, "?"))
    {
        debug_send_help();
        return;
    }

    if (debug_streq(cmd, "state"))
    {
        debug_send_state();
        return;
    }

    if (debug_streq(cmd, "layout") || debug_streq(cmd, "ui_layout"))
    {
        char *profile = debug_next_token(&cursor);

        if (debug_streq(profile, "default") || debug_streq(profile, "current"))
        {
            debug_apply_layout_profile(false);
            debug_send_raw("OK layout default\r\n");
            return;
        }
        if (debug_streq(profile, "gas"))
        {
            debug_apply_layout_profile(true);
            debug_send_raw("OK layout gas\r\n");
            return;
        }
        if (debug_streq(profile, "empty"))
        {
            debug_apply_empty_custom_layout_profile();
            debug_send_raw("OK layout empty\r\n");
            return;
        }
        if (debug_streq(profile, "side"))
        {
            bus_switch_layout_profile(THEME_TECH, ORDER_REVERSE);
            debug_send_raw("OK layout side\r\n");
            return;
        }
        if (debug_streq(profile, "top"))
        {
            bus_switch_layout_profile(THEME_CLASSIC, ORDER_NORMAL);
            debug_send_raw("OK layout top\r\n");
            return;
        }
        if (debug_streq(profile, "bottom"))
        {
            bus_switch_layout_profile(THEME_CLASSIC, ORDER_REVERSE);
            debug_send_raw("OK layout bottom\r\n");
            return;
        }

        debug_send_raw("ERR usage: layout <default|current|gas|empty|side|top|bottom>\r\n");
        return;
    }

    if (debug_streq(cmd, "a") || debug_streq(cmd, "alert"))
    {
        char *sub = debug_next_token(&cursor);
        alarm_id_t alarm_id = ALARM_ID_COUNT;

        if (!sub || debug_streq(sub, "list"))
        {
            debug_send_raw("ALERT ids: asc po2 po2min po2w ceil lock batt dead ndl cns otu safety depth time ss done\r\n");
            return;
        }

        if (debug_streq(sub, "auto"))
        {
            bool enabled;
            if (!debug_parse_bool(debug_next_token(&cursor), &enabled))
            {
                debug_send_raw("ERR usage: a auto on|off\r\n");
                return;
            }
            s_sim_alert_auto_enabled = enabled;
            if (!enabled)
            {
                sim_alert_clear_auto_active();
            }
            debug_sendf("OK a auto %s\r\n", enabled ? "on" : "off");
            return;
        }

        if (debug_streq(sub, "clear") || debug_streq(sub, "clr"))
        {
            char *name = debug_next_token(&cursor);
            if (!name || debug_streq(name, "all"))
            {
                sim_alert_clear_all();
                debug_send_raw("OK a clear all\r\n");
                return;
            }
            if (!sim_alert_alarm_id_from_text(name, &alarm_id))
            {
                debug_send_raw("ERR usage: a clear [id|all]\r\n");
                return;
            }
            sim_alert_set_forced(alarm_id, false);
            debug_sendf("OK a clear %s\r\n", sim_alert_alarm_id_name(alarm_id));
            return;
        }

        if (!sim_alert_alarm_id_from_text(sub, &alarm_id))
        {
            debug_send_raw("ERR usage: a <id> | a clear [id|all] | a auto on|off | a list\r\n");
            return;
        }

        {
            char *state = debug_next_token(&cursor);
            bool active = true;
            if (state && !debug_parse_bool(state, &active))
            {
                debug_send_raw("ERR usage: a <id> [on|off]\r\n");
                return;
            }
            sim_alert_set_forced(alarm_id, active);
            debug_sendf("OK a %s %s\r\n",
                        sim_alert_alarm_id_name(alarm_id),
                        active ? "on" : "off");
            return;
        }
    }

    if (debug_streq(cmd, "back") || debug_streq(cmd, "esc"))
    {
        char *hold_arg = debug_next_token(&cursor);
        if (hold_arg)
        {
            int hold_seconds;
            if (!debug_streq(cmd, "back") ||
                !debug_parse_int(hold_arg, &hold_seconds) ||
                hold_seconds != 2 ||
                debug_next_token(&cursor) != NULL)
            {
                debug_send_raw("ERR usage: back [2]\r\n");
                return;
            }

            if (!alarm_confirm_current())
            {
                ui_handle_back();
            }
            else
            {
                ui_state_set_alarm_pending_click(false);
            }
            debug_send_raw("OK back 2\r\n");
            return;
        }

        ui_handle_back();
        debug_send_raw("OK back\r\n");
        return;
    }

    if (debug_streq(cmd, "goto") || debug_streq(cmd, "go"))
    {
        float target_depth_m;
        float rate_mpm = 0.0f;
        char *rate_arg;
        arg = debug_next_token(&cursor);
        if (debug_streq(arg, "stop") || debug_streq(arg, "cancel") || debug_streq(arg, "off"))
        {
            debug_depth_goto_cancel();
            debug_send_raw("OK goto off\r\n");
            return;
        }
        if (!debug_parse_float(arg, &target_depth_m))
        {
            debug_send_raw("ERR usage: goto <depth_m> [m_min]|stop\r\n");
            return;
        }
        rate_arg = debug_next_token(&cursor);
        if (rate_arg != NULL && !debug_parse_float(rate_arg, &rate_mpm))
        {
            debug_send_raw("ERR usage: goto <depth_m> [m_min]|stop\r\n");
            return;
        }
        if (!debug_depth_goto_start(target_depth_m, rate_mpm))
        {
            return;
        }
        if (s_debug_link.depth_goto_rate_mpm > 0.0f)
        {
            debug_sendf("OK goto %.1f %.1fm/min\r\n", (double)s_debug_link.depth_goto_target_m, (double)s_debug_link.depth_goto_rate_mpm);
        }
        else
        {
            debug_sendf("OK goto %.1f auto\r\n", (double)s_debug_link.depth_goto_target_m);
        }
        return;
    }

    if (debug_streq(cmd, "speed") || debug_streq(cmd, "scale"))
    {
        int speed;
        if (!debug_parse_int(debug_next_token(&cursor), &speed) ||
                speed < 1 || speed > 120)
        {
            debug_send_raw("ERR usage: speed <1..120>\r\n");
            return;
        }
        s_debug_link.time_scale = (uint16_t)speed;
        debug_sendf("OK speed %u\r\n", (unsigned)s_debug_link.time_scale);
        return;
    }

    if (debug_streq(cmd, "manual") || debug_streq(cmd, "mode"))
    {
        bool enabled;
        arg = debug_next_token(&cursor);
        if (!debug_parse_bool(arg, &enabled))
        {
            debug_send_raw("ERR usage: manual on|off\r\n");
            return;
        }
        s_debug_link.manual_mode = enabled;
        if (enabled)
        {
            bus_set_ascent_rate(0.0f);
        }
        else
        {
            debug_depth_goto_cancel();
        }
        debug_sendf("OK manual %s\r\n", enabled ? "on" : "off");
        return;
    }

    if (debug_streq(cmd, "auto"))
    {
        bool enabled;
        arg = debug_next_token(&cursor);
        if (!debug_parse_bool(arg, &enabled))
        {
            debug_send_raw("ERR usage: auto on|off\r\n");
            return;
        }
        s_debug_link.manual_mode = !enabled;
        if (enabled)
        {
            debug_depth_goto_cancel();
        }
        debug_sendf("OK auto %s\r\n", enabled ? "on" : "off");
        return;
    }

    if (debug_streq(cmd, "depth"))
    {
        float depth;
        if (!debug_parse_float(debug_next_token(&cursor), &depth))
        {
            debug_send_raw("ERR usage: depth <m>\r\n");
            return;
        }
        debug_apply_depth_sample(depth);
        debug_sendf("OK depth %.1f\r\n", (double)g_sensor_data.depth);
        return;
    }

    if (debug_streq(cmd, "sample"))
    {
        int time_s;
        float depth;
        if (!debug_parse_int(debug_next_token(&cursor), &time_s) ||
                !debug_parse_float(debug_next_token(&cursor), &depth) ||
                time_s < 0)
        {
            debug_send_raw("ERR usage: sample <time_s> <depth_m>\r\n");
            return;
        }
        debug_depth_goto_cancel();
        bus_set_dive_time((uint32_t)time_s);
        dive_log_append_sampled((float)time_s, depth);
        bus_set_depth(depth);
        if (s_debug_link.depth_rate_valid && (uint32_t)time_s > s_debug_link.sample_time_s)
        {
            float delta_min = (float)((uint32_t)time_s - s_debug_link.sample_time_s) / 60.0f;
            bus_set_ascent_rate((s_debug_link.depth_rate_last_m - depth) / delta_min);
        }
        else
        {
            bus_set_ascent_rate(0.0f);
        }
        s_debug_link.sample_time_s = (uint32_t)time_s;
        s_debug_link.depth_rate_valid = true;
        s_debug_link.depth_rate_last_m = depth;
        s_debug_link.depth_rate_last_tick_ms = lv_tick_get();
        sim_alert_tick();
        debug_sendf("OK sample %d %.1f\r\n", time_s, (double)depth);
        return;
    }

    if (debug_streq(cmd, "rate") || debug_streq(cmd, "ascent"))
    {
        float rate_mpm;
        if (!debug_parse_float(debug_next_token(&cursor), &rate_mpm))
        {
            debug_send_raw("ERR usage: rate <m_min>\r\n");
            return;
        }
        bus_set_ascent_rate(rate_mpm);
        sim_alert_tick();
        debug_sendf("OK rate %+.1f\r\n", (double)g_sensor_data.ascent_rate);
        return;
    }

    if (debug_streq(cmd, "time"))
    {
        int time_s;
        if (!debug_parse_int(debug_next_token(&cursor), &time_s) || time_s < 0)
        {
            debug_send_raw("ERR usage: time <seconds>\r\n");
            return;
        }
        bus_set_dive_time((uint32_t)time_s);
        s_debug_link.sample_time_s = (uint32_t)time_s;
        debug_sendf("OK time %d\r\n", time_s);
        return;
    }

    if (debug_streq(cmd, "surface"))
    {
        int time_s;
        if (!debug_parse_int(debug_next_token(&cursor), &time_s) || time_s < 0)
        {
            debug_send_raw("ERR usage: surface <seconds>\r\n");
            return;
        }
        bus_set_surface_time((uint32_t)time_s);
        debug_sendf("OK surface %d\r\n", time_s);
        return;
    }

    if (debug_streq(cmd, "rtc_sleep_mark"))
    {
        time_t now;
        if (debug_next_token(&cursor) != NULL)
        {
            debug_send_raw("ERR usage: rtc_sleep_mark\r\n");
            return;
        }
        if (!debug_rtc_now(&now))
        {
            debug_send_raw("ERR rtc invalid\r\n");
            return;
        }

        s_debug_link.rtc_sleep_mark = now;
        s_debug_link.rtc_sleep_mark_valid = true;
        debug_sendf("OK rtc_sleep_mark rtc=%lld\r\n", (long long)now);
        return;
    }

    if (debug_streq(cmd, "rtc_sleep_status"))
    {
        if (debug_next_token(&cursor) != NULL)
        {
            debug_send_raw("ERR usage: rtc_sleep_status\r\n");
            return;
        }
        debug_rtc_sleep_status();
        return;
    }

    if (debug_streq(cmd, "rtc_sleep_apply"))
    {
        time_t now;
        uint32_t offline_seconds;

        if (debug_next_token(&cursor) != NULL)
        {
            debug_send_raw("ERR usage: rtc_sleep_apply\r\n");
            return;
        }
        if (!s_debug_link.rtc_sleep_mark_valid)
        {
            debug_send_raw("ERR rtc_sleep_mark required\r\n");
            return;
        }
        if (!debug_rtc_now(&now))
        {
            debug_send_raw("ERR rtc invalid\r\n");
            return;
        }
        if (now <= s_debug_link.rtc_sleep_mark)
        {
            debug_sendf("ERR rtc not advanced mark=%lld now=%lld\r\n", (long long)s_debug_link.rtc_sleep_mark, (long long)now);
            return;
        }
        if ((unsigned long long)(now - s_debug_link.rtc_sleep_mark) > (unsigned long long)DEBUG_RTC_OFFLINE_MAX_SECONDS)
        {
            debug_sendf("ERR rtc offline too large %llu > %lu\r\n",
                        (unsigned long long)(now - s_debug_link.rtc_sleep_mark),
                        (unsigned long)DEBUG_RTC_OFFLINE_MAX_SECONDS);
            return;
        }

        offline_seconds = (uint32_t)(now - s_debug_link.rtc_sleep_mark);
        if (debug_apply_rtc_offline_seconds(offline_seconds, "rtc_sleep_apply"))
        {
            s_debug_link.rtc_sleep_mark = now;
            s_debug_link.rtc_sleep_mark_valid = true;
        }
        return;
    }

    if (debug_streq(cmd, "rtc_offline"))
    {
        int seconds;
        char *extra;

        if (!debug_parse_int(debug_next_token(&cursor), &seconds) || seconds <= 0)
        {
            debug_send_raw("ERR usage: rtc_offline <seconds>\r\n");
            return;
        }
        extra = debug_next_token(&cursor);
        if (extra != NULL)
        {
            debug_send_raw("ERR usage: rtc_offline <seconds>\r\n");
            return;
        }
        (void)debug_apply_rtc_offline_seconds((uint32_t)seconds, "rtc_offline");
        return;
    }

    if (debug_streq(cmd, "ndl"))
    {
        int ndl;
        if (!debug_parse_int(debug_next_token(&cursor), &ndl))
        {
            debug_send_raw("ERR usage: ndl <minutes>\r\n");
            return;
        }
        bus_set_ndl((int16_t)ndl);
        debug_sendf("OK ndl %d\r\n", ndl);
        return;
    }

    if (debug_streq(cmd, "tts"))
    {
        int tts;
        if (!debug_parse_int(debug_next_token(&cursor), &tts) || tts < 0)
        {
            debug_send_raw("ERR usage: tts <minutes>\r\n");
            return;
        }
        bus_set_tts((uint16_t)tts);
        debug_sendf("OK tts %d\r\n", tts);
        return;
    }

    if (debug_streq(cmd, "stop"))
    {
        char *type_text = debug_next_token(&cursor);
        int ndl;
        float depth;
        int total_s;
        int left_s;
        int zone;
        stop_type_t type = STOP_NONE;

        if (debug_streq(type_text, "safety"))
        {
            type = STOP_SAFETY;
        }
        else if (debug_streq(type_text, "deco"))
        {
            type = STOP_DECO;
        }
        else if (!debug_streq(type_text, "none"))
        {
            debug_send_raw("ERR usage: stop <none|safety|deco> <ndl> <depth> <total_s> <left_s> <zone0|1>\r\n");
            return;
        }

        if (!debug_parse_int(debug_next_token(&cursor), &ndl) ||
                !debug_parse_float(debug_next_token(&cursor), &depth) ||
                !debug_parse_int(debug_next_token(&cursor), &total_s) ||
                !debug_parse_int(debug_next_token(&cursor), &left_s) ||
                !debug_parse_int(debug_next_token(&cursor), &zone) ||
                total_s < 0 || left_s < 0)
        {
            debug_send_raw("ERR usage: stop <none|safety|deco> <ndl> <depth> <total_s> <left_s> <zone0|1>\r\n");
            return;
        }

        bus_update_deco((int16_t)ndl,
                             type,
                             depth,
                             (uint16_t)total_s,
                             (uint16_t)left_s,
                             zone != 0);
        debug_send_raw("OK stop\r\n");
        return;
    }

    if (debug_streq(cmd, "pod"))
    {
        int idx;
        float bar;
        if (!debug_parse_int(debug_next_token(&cursor), &idx) ||
                !debug_parse_float(debug_next_token(&cursor), &bar) ||
                idx < 0 || idx > 1)
        {
            debug_send_raw("ERR usage: pod <0|1> <bar>\r\n");
            return;
        }
        bus_set_pod((uint8_t)idx, bar);
        debug_sendf("OK pod %d %.0f\r\n", idx, (double)bar);
        return;
    }

    if (debug_streq(cmd, "batt") || debug_streq(cmd, "battery"))
    {
        float pct;
        if (!debug_parse_float(debug_next_token(&cursor), &pct))
        {
            debug_send_raw("ERR usage: batt <pct>\r\n");
            return;
        }
        bus_set_battery(pct);
        debug_sendf("OK batt %.0f\r\n", (double)pct);
        return;
    }

    if (debug_streq(cmd, "temp") ||
            debug_streq(cmd, "bat_temp") ||
            debug_streq(cmd, "prj_temp"))
    {
        float temp;
        if (!debug_parse_float(debug_next_token(&cursor), &temp))
        {
            debug_send_raw("ERR usage: temp <c> | bat_temp <c> | prj_temp <c>\r\n");
            return;
        }
        if (debug_streq(cmd, "bat_temp"))
        {
            bus_set_bat_temperature(temp);
        }
        else if (debug_streq(cmd, "prj_temp"))
        {
            bus_set_prj_temperature(temp);
        }
        else
        {
            bus_set_temperature(temp);
        }
        debug_sendf("OK %s %.1f\r\n", cmd, (double)temp);
        return;
    }

    if (debug_streq(cmd, "heading"))
    {
        int heading;
        if (!debug_parse_int(debug_next_token(&cursor), &heading))
        {
            debug_send_raw("ERR usage: heading <deg>\r\n");
            return;
        }
        if (heading < 0)
        {
            heading = 0;
        }
        bus_set_heading((uint16_t)(heading % 360));
        debug_sendf("OK heading %d\r\n", heading % 360);
        return;
    }

    if (debug_streq(cmd, "ppo2"))
    {
        int idx;
        float value;
        if (!debug_parse_int(debug_next_token(&cursor), &idx) ||
                !debug_parse_float(debug_next_token(&cursor), &value) ||
                idx < 0 || idx >= GAS_COUNT)
        {
            debug_send_raw("ERR usage: ppo2 <slot> <bar>\r\n");
            return;
        }
        bus_set_ppo2((uint8_t)idx, value);
        debug_sendf("OK ppo2 %d %.2f\r\n", idx, (double)value);
        return;
    }

    if (debug_streq(cmd, "gf"))
    {
        int low;
        int high;
        if (!debug_parse_int(debug_next_token(&cursor), &low) ||
                !debug_parse_int(debug_next_token(&cursor), &high) ||
                low < 0 || low > 100 || high < 0 || high > 100)
        {
            debug_send_raw("ERR usage: gf <low> <high>\r\n");
            return;
        }
        bus_set_gf_setting((uint8_t)low, (uint8_t)high);
        debug_sendf("OK gf %d/%d\r\n", low, high);
        return;
    }

    if (debug_streq(cmd, "last_deco") ||
            debug_streq(cmd, "last_stop") ||
            debug_streq(cmd, "final_stop"))
    {
        int depth_m;
        if (!debug_parse_int(debug_next_token(&cursor), &depth_m) ||
                (depth_m != 3 && depth_m != 6))
        {
            debug_send_raw("ERR usage: last_deco <3|6>\r\n");
            return;
        }
        bus_set_last_deco_stop((uint8_t)depth_m);
        debug_sendf("OK last_deco %dm\r\n", depth_m);
        return;
    }

    if (debug_streq(cmd, "gf99") || debug_streq(cmd, "surf_gf") ||
            debug_streq(cmd, "cns") || debug_streq(cmd, "otu") ||
            debug_streq(cmd, "mod") || debug_streq(cmd, "ceiling") ||
            debug_streq(cmd, "dens") || debug_streq(cmd, "fio2"))
    {
        float value;
        if (!debug_parse_float(debug_next_token(&cursor), &value))
        {
            debug_send_raw("ERR numeric value required\r\n");
            return;
        }
        if (debug_streq(cmd, "gf99"))
        {
            bus_set_gf99(value);
        }
        else if (debug_streq(cmd, "surf_gf"))
        {
            bus_set_surf_gf(value);
        }
        else if (debug_streq(cmd, "cns"))
        {
            bus_set_cns((uint8_t)value);
        }
        else if (debug_streq(cmd, "otu"))
        {
            bus_set_otu((uint16_t)value);
        }
        else if (debug_streq(cmd, "mod"))
        {
            bus_set_mod(value);
        }
        else if (debug_streq(cmd, "ceiling"))
        {
            bus_set_ceiling(value);
        }
        else if (debug_streq(cmd, "dens"))
        {
            bus_set_gas_density(value);
        }
        else
        {
            bus_set_fio2(value);
        }
        debug_sendf("OK %s %.1f\r\n", cmd, (double)value);
        return;
    }

    if (debug_streq(cmd, "mix"))
    {
        int o2;
        int he;
        if (!debug_parse_int(debug_next_token(&cursor), &o2) ||
                !debug_parse_int(debug_next_token(&cursor), &he) ||
                o2 < 0 || o2 > 100 || he < 0 || he > 100 || o2 + he > 100)
        {
            debug_send_raw("ERR usage: mix <o2_pct> <he_pct>\r\n");
            return;
        }
        bus_set_gas_mix((uint8_t)o2, (uint8_t)he);
        debug_update_gas_derived();
        debug_sendf("OK mix %d/%d\r\n", o2, he);
        return;
    }

    if (debug_streq(cmd, "gas_count"))
    {
        int count;
        if (!debug_parse_int(debug_next_token(&cursor), &count) ||
                count < 0 || count > GAS_COUNT)
        {
            debug_send_raw("ERR usage: gas_count <0..5>\r\n");
            return;
        }
        bus_set_gas_slot_count((uint8_t)count);
        debug_sendf("OK gas_count %d\r\n", count);
        return;
    }

    if (debug_streq(cmd, "gas"))
    {
        int idx;
        char *name;
        if (!debug_parse_int(debug_next_token(&cursor), &idx) ||
                idx < 0 || idx >= GAS_COUNT)
        {
            debug_send_raw("ERR usage: gas <slot> [name]\r\n");
            return;
        }
        name = debug_trim(cursor);
        if (!name || name[0] == '\0')
        {
            name = g_sensor_data.gas_slot_name[idx][0] ? g_sensor_data.gas_slot_name[idx] : g_sensor_data.gas_name;
        }
        bus_set_gas((uint8_t)idx, name);
        debug_sendf("OK gas %d %s\r\n", idx, name);
        return;
    }

    if (debug_streq(cmd, "gas_slot"))
    {
        int idx;
        int o2;
        int he;
        float mod;
        char name[16];
        char *name_arg;

        if (!debug_parse_int(debug_next_token(&cursor), &idx) ||
                !debug_parse_int(debug_next_token(&cursor), &o2) ||
                !debug_parse_int(debug_next_token(&cursor), &he) ||
                !debug_parse_float(debug_next_token(&cursor), &mod) ||
                idx < 0 || idx >= GAS_COUNT ||
                o2 < 0 || o2 > 100 || he < 0 || he > 100 || o2 + he > 100)
        {
            debug_send_raw("ERR usage: gas_slot <slot> <o2> <he> <mod> [name]\r\n");
            return;
        }

        name_arg = debug_trim(cursor);
        if (name_arg && name_arg[0])
        {
            snprintf(name, sizeof(name), "%s", name_arg);
        }
        else
        {
            debug_format_gas_name(name, sizeof(name), o2, he);
        }
        bus_set_gas_slot((uint8_t)idx, name, (uint8_t)o2, (uint8_t)he, mod, bus_get_mod_ppo2());
        debug_sendf("OK gas_slot %d %s %d/%d %.0f\r\n", idx, name, o2, he, (double)mod);
        return;
    }

    if (debug_streq(cmd, "alarm"))
    {
        char *level_text = debug_next_token(&cursor);
        char *text = debug_trim(cursor);
        alarm_level_t level = ALARM_INFO;

        if (debug_streq(level_text, "clear"))
        {
            alarm_clear_custom();
            debug_send_raw("OK alarm clear\r\n");
            return;
        }

        if (debug_streq(level_text, "warn"))
        {
            level = ALARM_WARN;
        }
        else if (debug_streq(level_text, "crit") || debug_streq(level_text, "critical"))
        {
            level = ALARM_CRIT;
        }
        else if (!debug_streq(level_text, "info"))
        {
            debug_send_raw("ERR usage: alarm <info|warn|crit> <text> | alarm clear\r\n");
            return;
        }

        if (!text || text[0] == '\0')
        {
            text = "DEBUG ALARM";
        }
        alarm_raise_custom(level, text, COMP_EMPTY);
        debug_sendf("OK alarm %s %s\r\n", level_text, text);
        return;
    }

    debug_sendf("ERR unknown command: %s\r\n", cmd);
}

static void debug_process_rx_bytes(const char *data, int len)
{
    for (int i = 0; i < len; i++)
    {
        char ch = data[i];
        if (ch == '\r')
        {
            continue;
        }
        if (ch == '\n')
        {
            s_debug_link.rx_buf[s_debug_link.rx_len] = '\0';
            debug_exec_line(s_debug_link.rx_buf);
            s_debug_link.rx_len = 0;
            continue;
        }
        if (s_debug_link.rx_len + 1U >= DEBUG_RX_BUF_SIZE)
        {
            s_debug_link.rx_len = 0;
            debug_send_raw("ERR line too long\r\n");
            continue;
        }
        s_debug_link.rx_buf[s_debug_link.rx_len++] = ch;
    }
}

static void debug_disconnect_client(void)
{
    debug_depth_goto_cancel();
    debug_close_socket(&s_debug_link.client);
    s_debug_link.rx_len = 0;
    printf("[DBG] TCP debug client disconnected\r\n");
}

static void debug_poll_cb(lv_timer_t *timer)
{
    (void)timer;

    if (!s_debug_link.started || s_debug_link.listener == INVALID_SOCKET)
    {
        return;
    }

    if (s_debug_link.client == INVALID_SOCKET)
    {
        SOCKET client = s_debug_link.accept_(s_debug_link.listener, NULL, NULL);
        if (client != INVALID_SOCKET)
        {
            debug_set_nonblocking(client);
            s_debug_link.client = client;
            s_debug_link.rx_len = 0;
            s_debug_link.connect_event = true;
            s_debug_link.time_scale = 1;
            debug_depth_goto_cancel();
            s_debug_link.sample_time_s = g_sensor_data.dive_time_s;
            s_debug_link.depth_rate_valid = false;
            s_debug_link.depth_rate_last_m = g_sensor_data.depth;
            s_debug_link.depth_rate_last_tick_ms = lv_tick_get();
            s_debug_link.rtc_sleep_mark_valid = false;
            s_debug_link.rtc_sleep_mark = 0;
            bus_set_ascent_rate(0.0f);
            printf("[DBG] TCP debug client connected\r\n");
            debug_send_raw("Debug TCP ready on 127.0.0.1:7623\r\n");
            debug_send_raw("TCP connected: debug data will reset, auto depth is disabled, Arex deco core is active.\r\n");
            debug_send_raw("Type help for commands.\r\n");
        }
        else if (!debug_is_would_block(debug_last_error()))
        {
            printf("[DBG] accept failed: %d\r\n", debug_last_error());
        }
        return;
    }

    for (;;)
    {
        char buf[160];
        int n = s_debug_link.recv_(s_debug_link.client, buf, sizeof(buf), 0);
        if (n > 0)
        {
            if (debug_try_packet_line_rx(buf, n))
            {
                continue;
            }
            debug_process_rx_bytes(buf, n);
            continue;
        }
        if (n == 0)
        {
            debug_disconnect_client();
            return;
        }

        if (!debug_is_would_block(debug_last_error()))
        {
            printf("[DBG] recv failed: %d\r\n", debug_last_error());
            debug_disconnect_client();
        }
        return;
    }
}

void debug_link_pc_start(void)
{
    WSADATA wsa;
    struct sockaddr_in addr;

    if (s_debug_link.started)
    {
        return;
    }
    if (!debug_load_winsock())
    {
        return;
    }
    if (s_debug_link.WSAStartup_((WORD)0x0202U, &wsa) != 0)
    {
        printf("[DBG] WSAStartup failed\r\n");
        return;
    }

    s_debug_link.listener = s_debug_link.socket_(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s_debug_link.listener == INVALID_SOCKET)
    {
        printf("[DBG] socket failed: %d\r\n", debug_last_error());
        s_debug_link.WSACleanup_();
        return;
    }
    if (!debug_set_nonblocking(s_debug_link.listener))
    {
        printf("[DBG] ioctlsocket failed: %d\r\n", debug_last_error());
        debug_close_socket(&s_debug_link.listener);
        s_debug_link.WSACleanup_();
        return;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = debug_swap16((uint16_t)DEBUG_TCP_PORT);
    addr.sin_addr.s_addr = 0x0100007FUL;  /* 127.0.0.1 in network byte order on little-endian Windows. */

    if (s_debug_link.bind_(s_debug_link.listener, (const struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        printf("[DBG] bind 127.0.0.1:%u failed: %d\r\n",
               (unsigned)DEBUG_TCP_PORT,
               debug_last_error());
        debug_close_socket(&s_debug_link.listener);
        s_debug_link.WSACleanup_();
        return;
    }
    if (s_debug_link.listen_(s_debug_link.listener, 1) != 0)
    {
        printf("[DBG] listen failed: %d\r\n", debug_last_error());
        debug_close_socket(&s_debug_link.listener);
        s_debug_link.WSACleanup_();
        return;
    }

    s_debug_link.timer = lv_timer_create(debug_poll_cb, 50, NULL);
    s_debug_link.started = true;
    printf("[DBG] TCP debug link listening on 127.0.0.1:%u\r\n",
           (unsigned)DEBUG_TCP_PORT);
}

void debug_link_pc_set_rtc_offline_handler(debug_link_pc_rtc_offline_fn handler)
{
    s_debug_link.rtc_offline_handler = handler;
}

bool debug_link_pc_manual_mode(void)
{
    return s_debug_link.manual_mode || s_debug_link.client != INVALID_SOCKET;
}

bool debug_link_pc_depth_goto_step(float current_depth_m, float *out_depth_m, bool *out_reached)
{
    float target_depth_m;
    float delta_m;
    float step_m;
    float next_depth_m;

    if (!s_debug_link.depth_goto_active || !out_depth_m)
    {
        return false;
    }
    if (out_reached)
    {
        *out_reached = false;
    }

    target_depth_m = s_debug_link.depth_goto_target_m;
    delta_m = target_depth_m - current_depth_m;
    if (delta_m > -DEBUG_DEPTH_GOTO_EPSILON_M && delta_m < DEBUG_DEPTH_GOTO_EPSILON_M)
    {
        *out_depth_m = target_depth_m;
        if (out_reached)
        {
            *out_reached = true;
        }
        debug_depth_goto_cancel();
        return true;
    }

    if (s_debug_link.depth_goto_rate_mpm > 0.0f)
    {
        step_m = s_debug_link.depth_goto_rate_mpm / 60.0f;
        if (delta_m < 0.0f)
        {
            step_m = -step_m;
        }
    }
    else
    {
        step_m = (delta_m > 0.0f) ? (DEBUG_DEPTH_GOTO_DESCENT_MPM / 60.0f) : -(DEBUG_DEPTH_GOTO_ASCENT_MPM / 60.0f);
    }
    next_depth_m = current_depth_m + step_m;
    if ((step_m > 0.0f && next_depth_m >= target_depth_m) ||
            (step_m < 0.0f && next_depth_m <= target_depth_m))
    {
        next_depth_m = target_depth_m;
        if (out_reached)
        {
            *out_reached = true;
        }
        debug_depth_goto_cancel();
    }
    if (next_depth_m < 0.0f)
    {
        next_depth_m = 0.0f;
    }

    *out_depth_m = next_depth_m;
    return true;
}

bool debug_link_pc_consume_connect_event(void)
{
    bool event = s_debug_link.connect_event;
    s_debug_link.connect_event = false;
    return event;
}

uint16_t debug_link_pc_time_scale(void)
{
    return s_debug_link.time_scale > 0U ? s_debug_link.time_scale : 1U;
}

#else

void debug_link_pc_start(void)
{
}

void debug_link_pc_set_rtc_offline_handler(debug_link_pc_rtc_offline_fn handler)
{
    (void)handler;
}

bool debug_link_pc_manual_mode(void)
{
    return false;
}

bool debug_link_pc_consume_connect_event(void)
{
    return false;
}

uint16_t debug_link_pc_time_scale(void)
{
    return 1U;
}

bool debug_link_pc_depth_goto_step(float current_depth_m, float *out_depth_m, bool *out_reached)
{
    (void)current_depth_m;
    (void)out_depth_m;
    (void)out_reached;
    return false;
}

#endif /* PC_SIMULATOR && _WIN32 */

#endif /* DEBUG_LINK_PC_IMPLEMENTATION */

#endif /* DEBUG_LINK_PC_H */
