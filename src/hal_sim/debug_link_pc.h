#ifndef DEBUG_LINK_PC_H
#define DEBUG_LINK_PC_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void debug_link_pc_start(void);
bool debug_link_pc_manual_mode(void);
bool debug_link_pc_consume_connect_event(void);
uint16_t debug_link_pc_time_scale(void);

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
#include "lvgl/lvgl.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG_TCP_PORT 7623U
#define DEBUG_RX_BUF_SIZE 512U
#define DEBUG_TX_BUF_SIZE 768U

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

static void debug_apply_depth_sample(float depth)
{
    uint32_t sample_time_s;
    uint32_t now_ms;

    if (depth < 0.0f)
    {
        depth = 0.0f;
    }

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

    dive_log_append((float)sample_time_s, depth);
    bus_set_depth(depth);
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
        "AREX TCP debug commands:\r\n"
        "  <number> writes depth directly and appends one trajectory sample\r\n"
        "  help | state | back | manual on|off | auto on|off | speed <1..120>\r\n"
        "  depth <m> | sample <time_s> <depth_m> | rate <m_min> | time <s> | surface <s>\r\n"
        "  ndl <min> | tts <min> | stop <none|safety|deco> <ndl> <depth> <total_s> <left_s> <zone0|1>\r\n"
        "  pod <0|1> <bar> | batt <pct> | temp <c> | bat_temp <c> | prj_temp <c>\r\n"
        "  heading <deg> | ppo2 <slot> <bar> | gf <low> <high> | gf99 <pct> | surf_gf <pct>\r\n"
        "  last_deco <3|6> | final_stop <3|6>\r\n"
        "  cns <pct> | otu <value> | mod <m> | ceiling <m> | mix <o2> <he> | dens <g_l> | fio2 <pct>\r\n"
        "  gas_count <n> | gas <slot> [name] | gas_slot <slot> <o2> <he> <mod> [name]\r\n"
        "  alarm <info|warn|crit> <text>\r\n"
        "Slots are 0-based. TCP disables the auto depth script; the 1Hz clock keeps running.\r\n");
}

static void debug_send_state(void)
{
    debug_sendf(
        "STATE tcp=%u depth_manual=%u manual=%u speed=%u depth=%.1f rate=%+.1f time=%lu gas=%u:%s batt=%.0f temp=%.1f pod=%.0f/%.0f gf=%u/%u last_deco=%um\r\n",
        s_debug_link.client != INVALID_SOCKET ? 1U : 0U,
        (s_debug_link.manual_mode || s_debug_link.client != INVALID_SOCKET) ? 1U : 0U,
        s_debug_link.manual_mode ? 1U : 0U,
        (unsigned)debug_link_pc_time_scale(),
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

    if (debug_streq(cmd, "back") || debug_streq(cmd, "esc"))
    {
        ui_handle_back();
        debug_send_raw("OK back\r\n");
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
        bus_set_dive_time((uint32_t)time_s);
        dive_log_append((float)time_s, depth);
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
        bus_set_fio2((float)o2);
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
        bus_set_gas_slot((uint8_t)idx, name, (uint8_t)o2, (uint8_t)he, mod);
        debug_sendf("OK gas_slot %d %s %d/%d %.0f\r\n", idx, name, o2, he, (double)mod);
        return;
    }

    if (debug_streq(cmd, "alarm"))
    {
        char *level_text = debug_next_token(&cursor);
        char *text = debug_trim(cursor);
        alarm_level_t level = ALARM_INFO;

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
            debug_send_raw("ERR usage: alarm <info|warn|crit> <text>\r\n");
            return;
        }

        if (!text || text[0] == '\0')
        {
            text = "DEBUG ALARM";
        }
        bus_raise_alarm(level, text, COMP_EMPTY);
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
            s_debug_link.sample_time_s = g_sensor_data.dive_time_s;
            s_debug_link.depth_rate_valid = false;
            s_debug_link.depth_rate_last_m = g_sensor_data.depth;
            s_debug_link.depth_rate_last_tick_ms = lv_tick_get();
            bus_set_ascent_rate(0.0f);
            printf("[DBG] TCP debug client connected\r\n");
            debug_send_raw("AREX debug TCP ready on 127.0.0.1:7623\r\n");
            debug_send_raw("TCP connected: debug data will reset, auto depth is disabled, Buhlmann debug algorithm is active.\r\n");
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

bool debug_link_pc_manual_mode(void)
{
    return s_debug_link.manual_mode || s_debug_link.client != INVALID_SOCKET;
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

#endif /* PC_SIMULATOR && _WIN32 */

#endif /* DEBUG_LINK_PC_IMPLEMENTATION */

#endif /* DEBUG_LINK_PC_H */
