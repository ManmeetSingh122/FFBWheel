#include "serial_cmd.h"
#include "config.h"
#include "ffb_wheel.h"
#include "ffb_engine.h"
#include "adc_input.h"
#include "encoder.h"
#include "failsafe.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

extern uint8_t CDC_Transmit_FS(uint8_t *Buf, uint16_t Len);

#define RX_BUF_SIZE  256
static char    s_rx[RX_BUF_SIZE];
static uint8_t s_rx_pos = 0;
static volatile uint8_t cmd_ready = 0;

#define TX_BUF_SIZE  768
static char    tx_buf[TX_BUF_SIZE];
static volatile uint8_t tx_ready = 0;

void Serial_Send(const char *str)
{
    if (tx_ready) return;
    strncpy(tx_buf, str, TX_BUF_SIZE - 1);
    tx_buf[TX_BUF_SIZE - 1] = '\0';
    tx_ready = 1;
}

/* ── GET_CONFIG ──────────────────────────────────────────────────────────── */
static void send_config(void)
{
    char buf[TX_BUF_SIZE];
    int n = snprintf(buf, sizeof(buf),
        "{\"type\":\"config\","
        "\"range\":%u,\"ratio\":%.2f,\"pot_deg\":%.1f,\"invert\":%u,"
        "\"input_mode\":%u,\"encoder_ppr\":%u,"
        "\"strength\":%u,\"spring\":%u,\"damper\":%u,"
        "\"friction\":%u,\"inertia\":%u,\"max_torque\":%u,"
        "\"filter\":%u,\"bumpstop\":%u,\"center_spring\":%u,\"torque_smooth\":%u,"
        "\"deadzone\":%u,"
        "\"thr_min\":%u,\"thr_max\":%u,"
        "\"brake_min\":%u,\"brake_max\":%u,"
        "\"clutch_min\":%u,\"clutch_max\":%u,"
        "\"thr_curve\":%u,\"brake_curve\":%u,\"clutch_curve\":%u,"
        "\"thr_pts\":[%u,%u,%u,%u,%u],"
        "\"brake_pts\":[%u,%u,%u,%u,%u],"
        "\"clutch_pts\":[%u,%u,%u,%u,%u],"
        "\"sx_left\":%u,\"sx_right\":%u,"
        "\"sy_fwd\":%u,\"sy_rev\":%u,"
        "\"steer_min\":%u,\"steer_max\":%u,\"steer_center\":%u,"
        "\"fw_ver\":\"%u.%u\"}\n",
        g_cfg.wheel_range,
        (double)g_cfg.pulley_ratio, (double)g_cfg.pot_degrees,
        g_cfg.steer_invert,
        g_cfg.input_mode, g_cfg.encoder_ppr,
        g_cfg.ffb_strength, g_cfg.spring_gain, g_cfg.damper_gain,
        g_cfg.friction_gain, g_cfg.inertia_gain, g_cfg.max_torque,
        g_cfg.filter_strength, g_cfg.bumpstop_strength,
        g_cfg.center_spring, g_cfg.torque_smooth,
        g_cfg.center_deadzone,
        g_cfg.thr_min, g_cfg.thr_max,
        g_cfg.brake_min, g_cfg.brake_max,
        g_cfg.clutch_min, g_cfg.clutch_max,
        g_cfg.thr_curve, g_cfg.brake_curve, g_cfg.clutch_curve,
        g_cfg.thr_pts[0], g_cfg.thr_pts[1], g_cfg.thr_pts[2],
        g_cfg.thr_pts[3], g_cfg.thr_pts[4],
        g_cfg.brake_pts[0], g_cfg.brake_pts[1], g_cfg.brake_pts[2],
        g_cfg.brake_pts[3], g_cfg.brake_pts[4],
        g_cfg.clutch_pts[0], g_cfg.clutch_pts[1], g_cfg.clutch_pts[2],
        g_cfg.clutch_pts[3], g_cfg.clutch_pts[4],
        g_cfg.shft_x_left, g_cfg.shft_x_right,
        g_cfg.shft_y_fwd, g_cfg.shft_y_rev,
        g_cfg.steer_min, g_cfg.steer_max, g_cfg.steer_center,
        FW_VERSION_MAJOR, FW_VERSION_MINOR);
    (void)n;
    Serial_Send(buf);
}

/* ── GET_LIVE ────────────────────────────────────────────────────────────── */
void Serial_SendLive(void)
{
    float angle = (g_cfg.input_mode == INPUT_MODE_ENCODER)
                  ? Encoder_GetAngle() : ADC_GetWheelAngle();
    uint32_t steer_raw = (g_cfg.input_mode == INPUT_MODE_ENCODER)
                         ? (uint32_t)(Encoder_GetCount() + 32768)
                         : ADC_GetRawFiltered(ADC_IDX_STEER);

    char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"type\":\"live\","
        "\"angle\":%.1f,\"steer_raw\":%lu,"
        "\"thr\":%u,\"brake\":%u,\"clutch\":%u,"
        "\"shft_x\":%u,\"shft_y\":%u,"
        "\"gear\":%u,\"ffb_en\":%u,\"input\":%u}\n",
        (double)angle, steer_raw,
        ADC_GetRawFiltered(ADC_IDX_THR),
        ADC_GetRawFiltered(ADC_IDX_BRAKE),
        ADC_GetRawFiltered(ADC_IDX_CLUTCH),
        ADC_GetRawFiltered(ADC_IDX_SHFT_X),
        ADC_GetRawFiltered(ADC_IDX_SHFT_Y),
        (uint8_t)ADC_GetGear(), g_ffb_enabled,
        g_cfg.input_mode);
    Serial_Send(buf);
}

/* ── Parse 5 comma-separated uint8 values ───────────────────────────────── */
static uint8_t parse_pts(const char *str, uint8_t *pts)
{
    char tmp[32];
    strncpy(tmp, str, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    char *tok = strtok(tmp, ",");
    for (uint8_t i = 0; i < CURVE_POINTS; i++) {
        if (!tok) return 0;
        int v = atoi(tok);
        if (v < 0) v = 0;
        if (v > 100) v = 100;
        pts[i] = (uint8_t)v;
        tok = strtok(NULL, ",");
    }
    return 1;
}

/* ── Command processor ───────────────────────────────────────────────────── */
static void process_line(char *line)
{
    int len = strlen(line);
    while (len > 0 && (line[len-1]=='\r'||line[len-1]=='\n'||line[len-1]==' '))
        line[--len] = '\0';
    if (len == 0) return;

    /* ── Read commands ───────────────────────────────────────────────────── */
    if (!strcmp(line,"GET_CONFIG"))  { send_config(); return; }
    if (!strcmp(line,"GET_LIVE"))    { Serial_SendLive(); return; }

    if (!strcmp(line,"GET_STATUS")) {
        char buf[160];
        snprintf(buf, sizeof(buf),
            "{\"type\":\"status\",\"failsafe\":%u,\"reason\":\"%s\","
            "\"ffb_en\":%u,\"input\":%u,\"uptime\":%lu}\n",
            Failsafe_IsTriggered(), Failsafe_Reason(),
            g_ffb_enabled, g_cfg.input_mode, HAL_GetTick());
        Serial_Send(buf);
        return;
    }

    /* ── Write commands ──────────────────────────────────────────────────── */
    if (!strcmp(line,"SAVE")) {
        uint8_t ok = Config_Save();
        Serial_Send(ok ? "{\"type\":\"ok\",\"msg\":\"Saved\"}\n"
                       : "{\"type\":\"err\",\"msg\":\"Flash error\"}\n");
        return;
    }
    if (!strcmp(line,"RESET")) {
        Config_Defaults();
        Serial_Send("{\"type\":\"ok\",\"msg\":\"Defaults loaded\"}\n");
        return;
    }
    if (!strcmp(line,"CLEAR_FAILSAFE")) {
        Failsafe_Clear();
        Serial_Send("{\"type\":\"ok\",\"msg\":\"Failsafe cleared\"}\n");
        return;
    }

    /* ── Steering calibration ────────────────────────────────────────────── */
    if (!strcmp(line,"CALIBRATE_CENTER")) {
        if (g_cfg.input_mode == INPUT_MODE_ENCODER) {
            Encoder_SetCenter();
        } else {
            g_cfg.steer_center = g_adc[ADC_IDX_STEER];
        }
        Serial_Send("{\"type\":\"ok\",\"msg\":\"Center saved\"}\n");
        return;
    }
    if (!strcmp(line,"CALIBRATE_MIN"))   { g_cfg.steer_min=g_adc[ADC_IDX_STEER]; Serial_Send("{\"type\":\"ok\",\"msg\":\"Min saved\"}\n"); return; }
    if (!strcmp(line,"CALIBRATE_MAX"))   { g_cfg.steer_max=g_adc[ADC_IDX_STEER]; Serial_Send("{\"type\":\"ok\",\"msg\":\"Max saved\"}\n"); return; }

    /* ── Pedal calibration ───────────────────────────────────────────────── */
    if (!strcmp(line,"CAL_THR_MIN"))    { g_cfg.thr_min=g_adc[ADC_IDX_THR];        Serial_Send("{\"type\":\"ok\"}\n"); return; }
    if (!strcmp(line,"CAL_THR_MAX"))    { g_cfg.thr_max=g_adc[ADC_IDX_THR];        Serial_Send("{\"type\":\"ok\"}\n"); return; }
    if (!strcmp(line,"CAL_BRAKE_MIN"))  { g_cfg.brake_min=g_adc[ADC_IDX_BRAKE];    Serial_Send("{\"type\":\"ok\"}\n"); return; }
    if (!strcmp(line,"CAL_BRAKE_MAX"))  { g_cfg.brake_max=g_adc[ADC_IDX_BRAKE];    Serial_Send("{\"type\":\"ok\"}\n"); return; }
    if (!strcmp(line,"CAL_CLUTCH_MIN")) { g_cfg.clutch_min=g_adc[ADC_IDX_CLUTCH];  Serial_Send("{\"type\":\"ok\"}\n"); return; }
    if (!strcmp(line,"CAL_CLUTCH_MAX")) { g_cfg.clutch_max=g_adc[ADC_IDX_CLUTCH];  Serial_Send("{\"type\":\"ok\"}\n"); return; }

    /* ── Shifter calibration ─────────────────────────────────────────────── */
    if (!strcmp(line,"CAL_SHFT_LEFT"))  { g_cfg.shft_x_left=g_adc[ADC_IDX_SHFT_X];  Serial_Send("{\"type\":\"ok\"}\n"); return; }
    if (!strcmp(line,"CAL_SHFT_RIGHT")) { g_cfg.shft_x_right=g_adc[ADC_IDX_SHFT_X]; Serial_Send("{\"type\":\"ok\"}\n"); return; }
    if (!strcmp(line,"CAL_SHFT_FWD"))   { g_cfg.shft_y_fwd=g_adc[ADC_IDX_SHFT_Y];   Serial_Send("{\"type\":\"ok\"}\n"); return; }
    if (!strcmp(line,"CAL_SHFT_REV"))   { g_cfg.shft_y_rev=g_adc[ADC_IDX_SHFT_Y];   Serial_Send("{\"type\":\"ok\"}\n"); return; }

    /* ── SET key value ───────────────────────────────────────────────────── */
    if (!strncmp(line,"SET ",4)) {
        char *rest = line + 4;
        char *sp   = strchr(rest, ' ');
        if (!sp) { Serial_Send("{\"type\":\"err\",\"msg\":\"No value\"}\n"); return; }
        *sp = '\0';
        char  *key  = rest;
        char  *vstr = sp + 1;
        int    val  = atoi(vstr);
        float  fval = strtof(vstr, NULL);

        /* Wheel geometry */
        if      (!strcmp(key,"range"))        g_cfg.wheel_range     = (uint16_t)val;
        else if (!strcmp(key,"ratio"))        g_cfg.pulley_ratio    = fval;
        else if (!strcmp(key,"pot_deg"))      g_cfg.pot_degrees     = fval;
        else if (!strcmp(key,"invert"))       g_cfg.steer_invert    = val ? 1 : 0;
        /* Input source */
        else if (!strcmp(key,"input_mode"))   g_cfg.input_mode      = (uint8_t)(val & 1);
        else if (!strcmp(key,"encoder_ppr"))  g_cfg.encoder_ppr     = (uint16_t)val;
        /* FFB gains */
        else if (!strcmp(key,"strength"))     g_cfg.ffb_strength    = (uint8_t)val;
        else if (!strcmp(key,"spring"))       g_cfg.spring_gain     = (uint8_t)val;
        else if (!strcmp(key,"damper"))       g_cfg.damper_gain     = (uint8_t)val;
        else if (!strcmp(key,"friction"))     g_cfg.friction_gain   = (uint8_t)val;
        else if (!strcmp(key,"inertia"))      g_cfg.inertia_gain    = (uint8_t)val;
        else if (!strcmp(key,"max_torque"))   g_cfg.max_torque      = (uint8_t)val;
        /* Advanced FFB */
        else if (!strcmp(key,"filter"))       g_cfg.filter_strength   = (uint8_t)(val > 3 ? 3 : val);
        else if (!strcmp(key,"bumpstop"))     g_cfg.bumpstop_strength = (uint8_t)val;
        else if (!strcmp(key,"center_spring"))g_cfg.center_spring     = (uint8_t)val;
        else if (!strcmp(key,"torque_smooth"))g_cfg.torque_smooth     = (uint8_t)val;
        /* Misc */
        else if (!strcmp(key,"deadzone"))     g_cfg.center_deadzone = (uint8_t)val;
        /* Pedal curves */
        else if (!strcmp(key,"thr_curve"))    g_cfg.thr_curve       = (uint8_t)val;
        else if (!strcmp(key,"brake_curve"))  g_cfg.brake_curve     = (uint8_t)val;
        else if (!strcmp(key,"clutch_curve")) g_cfg.clutch_curve    = (uint8_t)val;
        /* 5-point custom curves: "SET thr_pts 0,25,50,75,100"            */
        else if (!strcmp(key,"thr_pts")) {
            if (!parse_pts(vstr, g_cfg.thr_pts)) {
                Serial_Send("{\"type\":\"err\",\"msg\":\"Need 5 values\"}\n");
                return;
            }
        }
        else if (!strcmp(key,"brake_pts")) {
            if (!parse_pts(vstr, g_cfg.brake_pts)) {
                Serial_Send("{\"type\":\"err\",\"msg\":\"Need 5 values\"}\n");
                return;
            }
        }
        else if (!strcmp(key,"clutch_pts")) {
            if (!parse_pts(vstr, g_cfg.clutch_pts)) {
                Serial_Send("{\"type\":\"err\",\"msg\":\"Need 5 values\"}\n");
                return;
            }
        }
        else {
            Serial_Send("{\"type\":\"err\",\"msg\":\"Unknown key\"}\n");
            return;
        }
        Serial_Send("{\"type\":\"ok\"}\n");
        return;
    }

    Serial_Send("{\"type\":\"err\",\"msg\":\"Unknown cmd\"}\n");
}

/* ── USB interrupt — buffer only ────────────────────────────────────────── */
void Serial_ProcessByte(uint8_t byte)
{
    if (byte == '\n' || byte == '\r') {
        if (s_rx_pos > 0) {
            s_rx[s_rx_pos] = '\0';
            cmd_ready = 1;
        }
    } else {
        if (s_rx_pos < RX_BUF_SIZE - 1)
            s_rx[s_rx_pos++] = (char)byte;
    }
}

/* ── Main loop — transmit and process ───────────────────────────────────── */
void Serial_Task(void)
{
    if (tx_ready) {
        uint16_t len = (uint16_t)strlen(tx_buf);
        if (len == 0) {
            tx_ready = 0;
        } else {
            uint8_t result = CDC_Transmit_FS((uint8_t*)tx_buf, len);
            if (result == 0) {
                tx_ready = 0;
            } else if (result == 5) {
                static uint32_t t_fail = 0;
                uint32_t now = HAL_GetTick();
                if (t_fail == 0) t_fail = now;
                if ((uint32_t)(now - t_fail) > 200) {
                    tx_ready = 0;
                    t_fail = 0;
                }
            }
        }
        return;
    }

    if (!cmd_ready) return;
    cmd_ready = 0;
    char line[RX_BUF_SIZE];
    memcpy(line, s_rx, s_rx_pos + 1);
    s_rx_pos = 0;
    process_line(line);
}
