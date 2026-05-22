#ifndef AREX_DEBUG_LINK_PC_H
#define AREX_DEBUG_LINK_PC_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void arex_debug_link_pc_start(void);
bool arex_debug_link_pc_manual_mode(void);

#ifdef __cplusplus
}
#endif

#ifdef AREX_DEBUG_LINK_PC_IMPLEMENTATION

#if defined(PC_SIMULATOR) && defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>

#include "../arex_ui/alarm/alarm.h"
#include "../arex_ui/core/data.h"
#include "../arex_ui/core/ui_engine.h"
#include "lvgl/lvgl.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AREX_DEBUG_TCP_PORT 7623U
#define AREX_DEBUG_RX_BUF_SIZE 512U
#define AREX_DEBUG_TX_BUF_SIZE 768U

typedef int (WSAAPI *arex_wsa_startup_fn)(WORD, LPWSADATA);
typedef int (WSAAPI *arex_wsa_cleanup_fn)(void);
typedef int (WSAAPI *arex_wsa_get_last_error_fn)(void);
typedef SOCKET (WSAAPI *arex_socket_fn)(int, int, int);
typedef int (WSAAPI *arex_bind_fn)(SOCKET, const struct sockaddr *, int);
typedef int (WSAAPI *arex_listen_fn)(SOCKET, int);
typedef SOCKET (WSAAPI *arex_accept_fn)(SOCKET, struct sockaddr *, int *);
typedef int (WSAAPI *arex_recv_fn)(SOCKET, char *, int, int);
typedef int (WSAAPI *arex_send_fn)(SOCKET, const char *, int, int);
typedef int (WSAAPI *arex_closesocket_fn)(SOCKET);
typedef int (WSAAPI *arex_ioctlsocket_fn)(SOCKET, long, u_long *);

typedef struct
{
    HMODULE dll;
    bool loaded;
    bool started;
    bool manual_mode;
    SOCKET listener;
    SOCKET client;
    lv_timer_t *timer;
    char rx_buf[AREX_DEBUG_RX_BUF_SIZE];
    uint16_t rx_len;
    uint32_t sample_time_s;

    arex_wsa_startup_fn WSAStartup_;
    arex_wsa_cleanup_fn WSACleanup_;
    arex_wsa_get_last_error_fn WSAGetLastError_;
    arex_socket_fn socket_;
    arex_bind_fn bind_;
    arex_listen_fn listen_;
    arex_accept_fn accept_;
    arex_recv_fn recv_;
    arex_send_fn send_;
    arex_closesocket_fn closesocket_;
    arex_ioctlsocket_fn ioctlsocket_;
} arex_debug_link_pc_t;

static arex_debug_link_pc_t s_debug_link =
{
    .listener = INVALID_SOCKET,
    .client = INVALID_SOCKET,
};

static uint16_t arex_debug_swap16(uint16_t value)
{
    return (uint16_t)(((value & 0x00FFU) << 8) | ((value & 0xFF00U) >> 8));
}

static bool arex_debug_load_winsock(void)
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

#define AREX_DBG_LOAD_PROC(field, name, type)                                      \
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

    AREX_DBG_LOAD_PROC(WSAStartup_, "WSAStartup", arex_wsa_startup_fn);
    AREX_DBG_LOAD_PROC(WSACleanup_, "WSACleanup", arex_wsa_cleanup_fn);
    AREX_DBG_LOAD_PROC(WSAGetLastError_, "WSAGetLastError", arex_wsa_get_last_error_fn);
    AREX_DBG_LOAD_PROC(socket_, "socket", arex_socket_fn);
    AREX_DBG_LOAD_PROC(bind_, "bind", arex_bind_fn);
    AREX_DBG_LOAD_PROC(listen_, "listen", arex_listen_fn);
    AREX_DBG_LOAD_PROC(accept_, "accept", arex_accept_fn);
    AREX_DBG_LOAD_PROC(recv_, "recv", arex_recv_fn);
    AREX_DBG_LOAD_PROC(send_, "send", arex_send_fn);
    AREX_DBG_LOAD_PROC(closesocket_, "closesocket", arex_closesocket_fn);
    AREX_DBG_LOAD_PROC(ioctlsocket_, "ioctlsocket", arex_ioctlsocket_fn);

#undef AREX_DBG_LOAD_PROC

    s_debug_link.loaded = true;
    return true;
}

static int arex_debug_last_error(void)
{
    return s_debug_link.WSAGetLastError_ ? s_debug_link.WSAGetLastError_() : 0;
}

static bool arex_debug_is_would_block(int err)
{
    return err == WSAEWOULDBLOCK || err == WSAEINPROGRESS || err == WSAEALREADY;
}

static void arex_debug_close_socket(SOCKET *sock)
{
    if (sock && *sock != INVALID_SOCKET && s_debug_link.closesocket_)
    {
        s_debug_link.closesocket_(*sock);
        *sock = INVALID_SOCKET;
    }
}

static bool arex_debug_set_nonblocking(SOCKET sock)
{
    u_long mode = 1;
    return s_debug_link.ioctlsocket_(sock, FIONBIO, &mode) == 0;
}

static void arex_debug_send_raw(const char *text)
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

static void arex_debug_sendf(const char *fmt, ...)
{
    char out[AREX_DEBUG_TX_BUF_SIZE];
    va_list ap;

    if (s_debug_link.client == INVALID_SOCKET)
    {
        return;
    }

    va_start(ap, fmt);
    vsnprintf(out, sizeof(out), fmt, ap);
    va_end(ap);
    arex_debug_send_raw(out);
}

static char *arex_debug_trim(char *text)
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

static char *arex_debug_next_token(char **cursor)
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

static bool arex_debug_streq(const char *a, const char *b)
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

static bool arex_debug_parse_float(const char *text, float *out)
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

static bool arex_debug_parse_int(const char *text, int *out)
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

static bool arex_debug_parse_bool(const char *text, bool *out)
{
    if (!text || !out)
    {
        return false;
    }
    if (arex_debug_streq(text, "1") ||
            arex_debug_streq(text, "on") ||
            arex_debug_streq(text, "true") ||
            arex_debug_streq(text, "yes") ||
            arex_debug_streq(text, "manual"))
    {
        *out = true;
        return true;
    }
    if (arex_debug_streq(text, "0") ||
            arex_debug_streq(text, "off") ||
            arex_debug_streq(text, "false") ||
            arex_debug_streq(text, "no") ||
            arex_debug_streq(text, "auto"))
    {
        *out = false;
        return true;
    }
    return false;
}

static void arex_debug_format_gas_name(char *out, size_t out_size, int o2, int he)
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

static void arex_debug_exec_line(char *line);

static void arex_debug_apply_depth_sample(float depth)
{
    uint32_t sample_time_s;

    if (depth < 0.0f)
    {
        depth = 0.0f;
    }

    sample_time_s = g_sensor_data.dive_time_s;
    s_debug_link.sample_time_s = sample_time_s;

    arex_dive_log_append((float)sample_time_s, depth);
    arex_bus_set_depth(depth);
}

static bool arex_debug_try_packet_line_rx(const char *data, int len)
{
    char text[AREX_DEBUG_RX_BUF_SIZE];
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
        arex_debug_send_raw("ERR line too long\r\n");
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

    trimmed = arex_debug_trim(text);
    if (!trimmed || trimmed[0] == '\0')
    {
        return true;
    }

    arex_debug_exec_line(trimmed);
    return true;
}

static void arex_debug_send_help(void)
{
    arex_debug_send_raw(
        "AREX TCP debug commands:\r\n"
        "  <number> writes depth directly and appends one trajectory sample\r\n"
        "  help | state | manual on|off | auto on|off\r\n"
        "  depth <m> | sample <time_s> <depth_m> | rate <m_min> | time <s> | surface <s>\r\n"
        "  ndl <min> | tts <min> | stop <none|safety|deco> <ndl> <depth> <total_s> <left_s> <zone0|1>\r\n"
        "  pod <0|1> <bar> | batt <pct> | temp <c> | bat_temp <c> | prj_temp <c>\r\n"
        "  heading <deg> | ppo2 <slot> <bar> | gf <low> <high> | gf99 <pct> | surf_gf <pct>\r\n"
        "  cns <pct> | otu <value> | mod <m> | ceiling <m> | mix <o2> <he> | dens <g_l> | fio2 <pct>\r\n"
        "  gas_count <n> | gas <slot> [name] | gas_slot <slot> <o2> <he> <mod> [name]\r\n"
        "  alarm <info|warn|crit> <text>\r\n"
        "Slots are 0-based. TCP disables the auto depth script; the 1Hz clock keeps running.\r\n");
}

static void arex_debug_send_state(void)
{
    arex_debug_sendf(
        "STATE tcp=%u depth_manual=%u manual=%u depth=%.1f rate=%+.1f time=%lu gas=%u:%s batt=%.0f temp=%.1f pod=%.0f/%.0f gf=%u/%u\r\n",
        s_debug_link.client != INVALID_SOCKET ? 1U : 0U,
        (s_debug_link.manual_mode || s_debug_link.client != INVALID_SOCKET) ? 1U : 0U,
        s_debug_link.manual_mode ? 1U : 0U,
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
        (unsigned)g_sensor_data.gf_high);
}

static void arex_debug_exec_line(char *line)
{
    char *cursor;
    char *cmd;
    char *arg;

    line = arex_debug_trim(line);
    if (!line || line[0] == '\0')
    {
        return;
    }

    cursor = line;

    {
        float direct_depth;
        if (arex_debug_parse_float(line, &direct_depth))
        {
            arex_debug_apply_depth_sample(direct_depth);
            arex_debug_sendf("OK depth %.1f\r\n", (double)g_sensor_data.depth);
            return;
        }
    }

    cmd = arex_debug_next_token(&cursor);
    if (!cmd)
    {
        return;
    }

    if (arex_debug_streq(cmd, "help") || arex_debug_streq(cmd, "?"))
    {
        arex_debug_send_help();
        return;
    }

    if (arex_debug_streq(cmd, "state"))
    {
        arex_debug_send_state();
        return;
    }

    if (arex_debug_streq(cmd, "manual") || arex_debug_streq(cmd, "mode"))
    {
        bool enabled;
        arg = arex_debug_next_token(&cursor);
        if (!arex_debug_parse_bool(arg, &enabled))
        {
            arex_debug_send_raw("ERR usage: manual on|off\r\n");
            return;
        }
        s_debug_link.manual_mode = enabled;
        if (enabled)
        {
            arex_bus_set_ascent_rate(0.0f);
        }
        arex_debug_sendf("OK manual %s\r\n", enabled ? "on" : "off");
        return;
    }

    if (arex_debug_streq(cmd, "auto"))
    {
        bool enabled;
        arg = arex_debug_next_token(&cursor);
        if (!arex_debug_parse_bool(arg, &enabled))
        {
            arex_debug_send_raw("ERR usage: auto on|off\r\n");
            return;
        }
        s_debug_link.manual_mode = !enabled;
        arex_debug_sendf("OK auto %s\r\n", enabled ? "on" : "off");
        return;
    }

    if (arex_debug_streq(cmd, "depth"))
    {
        float depth;
        if (!arex_debug_parse_float(arex_debug_next_token(&cursor), &depth))
        {
            arex_debug_send_raw("ERR usage: depth <m>\r\n");
            return;
        }
        arex_debug_apply_depth_sample(depth);
        arex_debug_sendf("OK depth %.1f\r\n", (double)g_sensor_data.depth);
        return;
    }

    if (arex_debug_streq(cmd, "sample"))
    {
        int time_s;
        float depth;
        if (!arex_debug_parse_int(arex_debug_next_token(&cursor), &time_s) ||
                !arex_debug_parse_float(arex_debug_next_token(&cursor), &depth) ||
                time_s < 0)
        {
            arex_debug_send_raw("ERR usage: sample <time_s> <depth_m>\r\n");
            return;
        }
        arex_bus_set_dive_time((uint32_t)time_s);
        arex_dive_log_append((float)time_s, depth);
        arex_bus_set_depth(depth);
        s_debug_link.sample_time_s = (uint32_t)time_s;
        arex_debug_sendf("OK sample %d %.1f\r\n", time_s, (double)depth);
        return;
    }

    if (arex_debug_streq(cmd, "rate") || arex_debug_streq(cmd, "ascent"))
    {
        float rate_mpm;
        if (!arex_debug_parse_float(arex_debug_next_token(&cursor), &rate_mpm))
        {
            arex_debug_send_raw("ERR usage: rate <m_min>\r\n");
            return;
        }
        arex_bus_set_ascent_rate(rate_mpm);
        arex_debug_sendf("OK rate %+.1f\r\n", (double)g_sensor_data.ascent_rate);
        return;
    }

    if (arex_debug_streq(cmd, "time"))
    {
        int time_s;
        if (!arex_debug_parse_int(arex_debug_next_token(&cursor), &time_s) || time_s < 0)
        {
            arex_debug_send_raw("ERR usage: time <seconds>\r\n");
            return;
        }
        arex_bus_set_dive_time((uint32_t)time_s);
        s_debug_link.sample_time_s = (uint32_t)time_s;
        arex_debug_sendf("OK time %d\r\n", time_s);
        return;
    }

    if (arex_debug_streq(cmd, "surface"))
    {
        int time_s;
        if (!arex_debug_parse_int(arex_debug_next_token(&cursor), &time_s) || time_s < 0)
        {
            arex_debug_send_raw("ERR usage: surface <seconds>\r\n");
            return;
        }
        arex_bus_set_surface_time((uint32_t)time_s);
        arex_debug_sendf("OK surface %d\r\n", time_s);
        return;
    }

    if (arex_debug_streq(cmd, "ndl"))
    {
        int ndl;
        if (!arex_debug_parse_int(arex_debug_next_token(&cursor), &ndl))
        {
            arex_debug_send_raw("ERR usage: ndl <minutes>\r\n");
            return;
        }
        arex_bus_set_ndl((int16_t)ndl);
        arex_debug_sendf("OK ndl %d\r\n", ndl);
        return;
    }

    if (arex_debug_streq(cmd, "tts"))
    {
        int tts;
        if (!arex_debug_parse_int(arex_debug_next_token(&cursor), &tts) || tts < 0)
        {
            arex_debug_send_raw("ERR usage: tts <minutes>\r\n");
            return;
        }
        arex_bus_set_tts((uint16_t)tts);
        arex_debug_sendf("OK tts %d\r\n", tts);
        return;
    }

    if (arex_debug_streq(cmd, "stop"))
    {
        char *type_text = arex_debug_next_token(&cursor);
        int ndl;
        float depth;
        int total_s;
        int left_s;
        int zone;
        arex_stop_type_t type = STOP_NONE;

        if (arex_debug_streq(type_text, "safety"))
        {
            type = STOP_SAFETY;
        }
        else if (arex_debug_streq(type_text, "deco"))
        {
            type = STOP_DECO;
        }
        else if (!arex_debug_streq(type_text, "none"))
        {
            arex_debug_send_raw("ERR usage: stop <none|safety|deco> <ndl> <depth> <total_s> <left_s> <zone0|1>\r\n");
            return;
        }

        if (!arex_debug_parse_int(arex_debug_next_token(&cursor), &ndl) ||
                !arex_debug_parse_float(arex_debug_next_token(&cursor), &depth) ||
                !arex_debug_parse_int(arex_debug_next_token(&cursor), &total_s) ||
                !arex_debug_parse_int(arex_debug_next_token(&cursor), &left_s) ||
                !arex_debug_parse_int(arex_debug_next_token(&cursor), &zone) ||
                total_s < 0 || left_s < 0)
        {
            arex_debug_send_raw("ERR usage: stop <none|safety|deco> <ndl> <depth> <total_s> <left_s> <zone0|1>\r\n");
            return;
        }

        arex_bus_update_deco((int16_t)ndl,
                             type,
                             depth,
                             (uint16_t)total_s,
                             (uint16_t)left_s,
                             zone != 0);
        arex_debug_send_raw("OK stop\r\n");
        return;
    }

    if (arex_debug_streq(cmd, "pod"))
    {
        int idx;
        float bar;
        if (!arex_debug_parse_int(arex_debug_next_token(&cursor), &idx) ||
                !arex_debug_parse_float(arex_debug_next_token(&cursor), &bar) ||
                idx < 0 || idx > 1)
        {
            arex_debug_send_raw("ERR usage: pod <0|1> <bar>\r\n");
            return;
        }
        arex_bus_set_pod((uint8_t)idx, bar);
        arex_debug_sendf("OK pod %d %.0f\r\n", idx, (double)bar);
        return;
    }

    if (arex_debug_streq(cmd, "batt") || arex_debug_streq(cmd, "battery"))
    {
        float pct;
        if (!arex_debug_parse_float(arex_debug_next_token(&cursor), &pct))
        {
            arex_debug_send_raw("ERR usage: batt <pct>\r\n");
            return;
        }
        arex_bus_set_battery(pct);
        arex_debug_sendf("OK batt %.0f\r\n", (double)pct);
        return;
    }

    if (arex_debug_streq(cmd, "temp") ||
            arex_debug_streq(cmd, "bat_temp") ||
            arex_debug_streq(cmd, "prj_temp"))
    {
        float temp;
        if (!arex_debug_parse_float(arex_debug_next_token(&cursor), &temp))
        {
            arex_debug_send_raw("ERR usage: temp <c> | bat_temp <c> | prj_temp <c>\r\n");
            return;
        }
        if (arex_debug_streq(cmd, "bat_temp"))
        {
            arex_bus_set_bat_temperature(temp);
        }
        else if (arex_debug_streq(cmd, "prj_temp"))
        {
            arex_bus_set_prj_temperature(temp);
        }
        else
        {
            arex_bus_set_temperature(temp);
        }
        arex_debug_sendf("OK %s %.1f\r\n", cmd, (double)temp);
        return;
    }

    if (arex_debug_streq(cmd, "heading"))
    {
        int heading;
        if (!arex_debug_parse_int(arex_debug_next_token(&cursor), &heading))
        {
            arex_debug_send_raw("ERR usage: heading <deg>\r\n");
            return;
        }
        if (heading < 0)
        {
            heading = 0;
        }
        arex_bus_set_heading((uint16_t)(heading % 360));
        arex_debug_sendf("OK heading %d\r\n", heading % 360);
        return;
    }

    if (arex_debug_streq(cmd, "ppo2"))
    {
        int idx;
        float value;
        if (!arex_debug_parse_int(arex_debug_next_token(&cursor), &idx) ||
                !arex_debug_parse_float(arex_debug_next_token(&cursor), &value) ||
                idx < 0 || idx >= GAS_COUNT)
        {
            arex_debug_send_raw("ERR usage: ppo2 <slot> <bar>\r\n");
            return;
        }
        arex_bus_set_ppo2((uint8_t)idx, value);
        arex_debug_sendf("OK ppo2 %d %.2f\r\n", idx, (double)value);
        return;
    }

    if (arex_debug_streq(cmd, "gf"))
    {
        int low;
        int high;
        if (!arex_debug_parse_int(arex_debug_next_token(&cursor), &low) ||
                !arex_debug_parse_int(arex_debug_next_token(&cursor), &high) ||
                low < 0 || low > 100 || high < 0 || high > 100)
        {
            arex_debug_send_raw("ERR usage: gf <low> <high>\r\n");
            return;
        }
        arex_bus_set_gf_setting((uint8_t)low, (uint8_t)high);
        arex_debug_sendf("OK gf %d/%d\r\n", low, high);
        return;
    }

    if (arex_debug_streq(cmd, "gf99") || arex_debug_streq(cmd, "surf_gf") ||
            arex_debug_streq(cmd, "cns") || arex_debug_streq(cmd, "otu") ||
            arex_debug_streq(cmd, "mod") || arex_debug_streq(cmd, "ceiling") ||
            arex_debug_streq(cmd, "dens") || arex_debug_streq(cmd, "fio2"))
    {
        float value;
        if (!arex_debug_parse_float(arex_debug_next_token(&cursor), &value))
        {
            arex_debug_send_raw("ERR numeric value required\r\n");
            return;
        }
        if (arex_debug_streq(cmd, "gf99"))
        {
            arex_bus_set_gf99(value);
        }
        else if (arex_debug_streq(cmd, "surf_gf"))
        {
            arex_bus_set_surf_gf(value);
        }
        else if (arex_debug_streq(cmd, "cns"))
        {
            arex_bus_set_cns((uint8_t)value);
        }
        else if (arex_debug_streq(cmd, "otu"))
        {
            arex_bus_set_otu((uint16_t)value);
        }
        else if (arex_debug_streq(cmd, "mod"))
        {
            arex_bus_set_mod(value);
        }
        else if (arex_debug_streq(cmd, "ceiling"))
        {
            arex_bus_set_ceiling(value);
        }
        else if (arex_debug_streq(cmd, "dens"))
        {
            arex_bus_set_gas_density(value);
        }
        else
        {
            arex_bus_set_fio2(value);
        }
        arex_debug_sendf("OK %s %.1f\r\n", cmd, (double)value);
        return;
    }

    if (arex_debug_streq(cmd, "mix"))
    {
        int o2;
        int he;
        if (!arex_debug_parse_int(arex_debug_next_token(&cursor), &o2) ||
                !arex_debug_parse_int(arex_debug_next_token(&cursor), &he) ||
                o2 < 0 || o2 > 100 || he < 0 || he > 100 || o2 + he > 100)
        {
            arex_debug_send_raw("ERR usage: mix <o2_pct> <he_pct>\r\n");
            return;
        }
        arex_bus_set_gas_mix((uint8_t)o2, (uint8_t)he);
        arex_bus_set_fio2((float)o2);
        arex_debug_sendf("OK mix %d/%d\r\n", o2, he);
        return;
    }

    if (arex_debug_streq(cmd, "gas_count"))
    {
        int count;
        if (!arex_debug_parse_int(arex_debug_next_token(&cursor), &count) ||
                count < 0 || count > GAS_COUNT)
        {
            arex_debug_send_raw("ERR usage: gas_count <0..5>\r\n");
            return;
        }
        arex_bus_set_gas_slot_count((uint8_t)count);
        arex_debug_sendf("OK gas_count %d\r\n", count);
        return;
    }

    if (arex_debug_streq(cmd, "gas"))
    {
        int idx;
        char *name;
        if (!arex_debug_parse_int(arex_debug_next_token(&cursor), &idx) ||
                idx < 0 || idx >= GAS_COUNT)
        {
            arex_debug_send_raw("ERR usage: gas <slot> [name]\r\n");
            return;
        }
        name = arex_debug_trim(cursor);
        if (!name || name[0] == '\0')
        {
            name = g_sensor_data.gas_slot_name[idx][0] ? g_sensor_data.gas_slot_name[idx] : g_sensor_data.gas_name;
        }
        arex_bus_set_gas((uint8_t)idx, name);
        arex_debug_sendf("OK gas %d %s\r\n", idx, name);
        return;
    }

    if (arex_debug_streq(cmd, "gas_slot"))
    {
        int idx;
        int o2;
        int he;
        float mod;
        char name[16];
        char *name_arg;

        if (!arex_debug_parse_int(arex_debug_next_token(&cursor), &idx) ||
                !arex_debug_parse_int(arex_debug_next_token(&cursor), &o2) ||
                !arex_debug_parse_int(arex_debug_next_token(&cursor), &he) ||
                !arex_debug_parse_float(arex_debug_next_token(&cursor), &mod) ||
                idx < 0 || idx >= GAS_COUNT ||
                o2 < 0 || o2 > 100 || he < 0 || he > 100 || o2 + he > 100)
        {
            arex_debug_send_raw("ERR usage: gas_slot <slot> <o2> <he> <mod> [name]\r\n");
            return;
        }

        name_arg = arex_debug_trim(cursor);
        if (name_arg && name_arg[0])
        {
            snprintf(name, sizeof(name), "%s", name_arg);
        }
        else
        {
            arex_debug_format_gas_name(name, sizeof(name), o2, he);
        }
        arex_bus_set_gas_slot((uint8_t)idx, name, (uint8_t)o2, (uint8_t)he, mod);
        arex_debug_sendf("OK gas_slot %d %s %d/%d %.0f\r\n", idx, name, o2, he, (double)mod);
        return;
    }

    if (arex_debug_streq(cmd, "alarm"))
    {
        char *level_text = arex_debug_next_token(&cursor);
        char *text = arex_debug_trim(cursor);
        arex_alarm_level_t level = ALARM_INFO;

        if (arex_debug_streq(level_text, "warn"))
        {
            level = ALARM_WARN;
        }
        else if (arex_debug_streq(level_text, "crit") || arex_debug_streq(level_text, "critical"))
        {
            level = ALARM_CRIT;
        }
        else if (!arex_debug_streq(level_text, "info"))
        {
            arex_debug_send_raw("ERR usage: alarm <info|warn|crit> <text>\r\n");
            return;
        }

        if (!text || text[0] == '\0')
        {
            text = "DEBUG ALARM";
        }
        arex_bus_raise_alarm(level, text, COMP_EMPTY);
        arex_debug_sendf("OK alarm %s %s\r\n", level_text, text);
        return;
    }

    arex_debug_sendf("ERR unknown command: %s\r\n", cmd);
}

static void arex_debug_process_rx_bytes(const char *data, int len)
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
            arex_debug_exec_line(s_debug_link.rx_buf);
            s_debug_link.rx_len = 0;
            continue;
        }
        if (s_debug_link.rx_len + 1U >= AREX_DEBUG_RX_BUF_SIZE)
        {
            s_debug_link.rx_len = 0;
            arex_debug_send_raw("ERR line too long\r\n");
            continue;
        }
        s_debug_link.rx_buf[s_debug_link.rx_len++] = ch;
    }
}

static void arex_debug_disconnect_client(void)
{
    arex_debug_close_socket(&s_debug_link.client);
    s_debug_link.rx_len = 0;
    printf("[DBG] TCP debug client disconnected\r\n");
}

static void arex_debug_poll_cb(lv_timer_t *timer)
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
            arex_debug_set_nonblocking(client);
            s_debug_link.client = client;
            s_debug_link.rx_len = 0;
            s_debug_link.sample_time_s = g_sensor_data.dive_time_s;
            arex_bus_set_ascent_rate(0.0f);
            printf("[DBG] TCP debug client connected\r\n");
            arex_debug_send_raw("AREX debug TCP ready on 127.0.0.1:7623\r\n");
            arex_debug_send_raw("TCP disables the auto depth script; time and rate sampling keep running.\r\n");
            arex_debug_send_raw("Type help for commands.\r\n");
        }
        else if (!arex_debug_is_would_block(arex_debug_last_error()))
        {
            printf("[DBG] accept failed: %d\r\n", arex_debug_last_error());
        }
        return;
    }

    for (;;)
    {
        char buf[160];
        int n = s_debug_link.recv_(s_debug_link.client, buf, sizeof(buf), 0);
        if (n > 0)
        {
            if (arex_debug_try_packet_line_rx(buf, n))
            {
                continue;
            }
            arex_debug_process_rx_bytes(buf, n);
            continue;
        }
        if (n == 0)
        {
            arex_debug_disconnect_client();
            return;
        }

        if (!arex_debug_is_would_block(arex_debug_last_error()))
        {
            printf("[DBG] recv failed: %d\r\n", arex_debug_last_error());
            arex_debug_disconnect_client();
        }
        return;
    }
}

void arex_debug_link_pc_start(void)
{
    WSADATA wsa;
    struct sockaddr_in addr;

    if (s_debug_link.started)
    {
        return;
    }
    if (!arex_debug_load_winsock())
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
        printf("[DBG] socket failed: %d\r\n", arex_debug_last_error());
        s_debug_link.WSACleanup_();
        return;
    }
    if (!arex_debug_set_nonblocking(s_debug_link.listener))
    {
        printf("[DBG] ioctlsocket failed: %d\r\n", arex_debug_last_error());
        arex_debug_close_socket(&s_debug_link.listener);
        s_debug_link.WSACleanup_();
        return;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = arex_debug_swap16((uint16_t)AREX_DEBUG_TCP_PORT);
    addr.sin_addr.s_addr = 0x0100007FUL;  /* 127.0.0.1 in network byte order on little-endian Windows. */

    if (s_debug_link.bind_(s_debug_link.listener, (const struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        printf("[DBG] bind 127.0.0.1:%u failed: %d\r\n",
               (unsigned)AREX_DEBUG_TCP_PORT,
               arex_debug_last_error());
        arex_debug_close_socket(&s_debug_link.listener);
        s_debug_link.WSACleanup_();
        return;
    }
    if (s_debug_link.listen_(s_debug_link.listener, 1) != 0)
    {
        printf("[DBG] listen failed: %d\r\n", arex_debug_last_error());
        arex_debug_close_socket(&s_debug_link.listener);
        s_debug_link.WSACleanup_();
        return;
    }

    s_debug_link.timer = lv_timer_create(arex_debug_poll_cb, 50, NULL);
    s_debug_link.started = true;
    printf("[DBG] TCP debug link listening on 127.0.0.1:%u\r\n",
           (unsigned)AREX_DEBUG_TCP_PORT);
}

bool arex_debug_link_pc_manual_mode(void)
{
    return s_debug_link.manual_mode || s_debug_link.client != INVALID_SOCKET;
}

#else

void arex_debug_link_pc_start(void)
{
}

bool arex_debug_link_pc_manual_mode(void)
{
    return false;
}

#endif /* PC_SIMULATOR && _WIN32 */

#endif /* AREX_DEBUG_LINK_PC_IMPLEMENTATION */

#endif /* AREX_DEBUG_LINK_PC_H */
