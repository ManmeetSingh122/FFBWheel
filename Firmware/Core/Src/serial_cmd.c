#include "serial_cmd.h"
#include "config.h"
#include "ffb_wheel.h"
#include "ffb_engine.h"
#include "adc_input.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ── USB CDC send function (implemented in usbd_cdc_if.c by CubeMX) ─────── */
extern uint8_t CDC_Transmit_FS(uint8_t *Buf, uint16_t Len);

/* ── RX line buffer ──────────────────────────────────────────────────────── */
#define RX_BUF_SIZE 128
static char s_rx[RX_BUF_SIZE];
static uint8_t s_rx_pos = 0;

/* ── Helper: send string ─────────────────────────────────────────────────── */
void Serial_Send(const char *str)
{
    CDC_Transmit_FS((uint8_t*)str, (uint16_t)strlen(str));
}

/* ── Build and send GET_CONFIG response ──────────────────────────────────── */
static void send_config(void)
{
    char buf[512];
    snprintf(buf, sizeof(buf),
        "{\"type\":\"config\","
        "\"range\":%u,"
        "\"ratio\":%.2f,"
        "\"pot_deg\":%.1f,"
        "\"invert\":%u,"
        "\"strength\":%u,"
        "\"spring\":%u,"
        "\"damper\":%u,"
        "\"friction\":%u,"
        "\"inertia\":%u,"
        "\"max_torque\":%u,"
        "\"deadzone\":%u,"
        "\"thr_min\":%u,\"thr_max\":%u,"
        "\"brake_min\":%u,\"brake_max\":%u,"
        "\"clutch_min\":%u,\"clutch_max\":%u,"
        "\"thr_curve\":%u,\"brake_curve\":%u,\"clutch_curve\":%u,"
        "\"sx_left\":%u,\"sx_right\":%u,"
        "\"sy_fwd\":%u,\"sy_rev\":%u,"
        "\"steer_min\":%u,\"steer_max\":%u,\"steer_center\":%u,"
        "\"fw_ver\":\"%u.%u\"}\n",
        g_cfg.wheel_range,
        (double)g_cfg.pulley_ratio,
        (double)g_cfg.pot_degrees,
        g_cfg.steer_invert,
        g_cfg.ffb_strength, g_cfg.spring_gain,
        g_cfg.damper_gain, g_cfg.friction_gain, g_cfg.inertia_gain,
        g_cfg.max_torque, g_cfg.center_deadzone,
        g_cfg.thr_min, g_cfg.thr_max,
        g_cfg.brake_min, g_cfg.brake_max,
        g_cfg.clutch_min, g_cfg.clutch_max,
        g_cfg.thr_curve, g_cfg.brake_curve, g_cfg.clutch_curve,
        g_cfg.shft_x_left, g_cfg.shft_x_right,
        g_cfg.shft_y_fwd, g_cfg.shft_y_rev,
        g_cfg.steer_min, g_cfg.steer_max, g_cfg.steer_center,
        FW_VERSION_MAJOR, FW_VERSION_MINOR);
    Serial_Send(buf);
}

/* ── Build and send GET_LIVE response ────────────────────────────────────── */
void Serial_SendLive(void)
{
    char buf[256];
    float angle = ADC_GetWheelAngle();
    uint8_t gear = (uint8_t)ADC_GetGear();
    snprintf(buf, sizeof(buf),
        "{\"type\":\"live\","
        "\"angle\":%.1f,"
        "\"steer_raw\":%u,"
        "\"thr\":%u,\"brake\":%u,\"clutch\":%u,"
        "\"shft_x\":%u,\"shft_y\":%u,"
        "\"gear\":%u,"
        "\"ffb_en\":%u}\n",
        (double)angle,
        g_adc[ADC_IDX_STEER],
        g_adc[ADC_IDX_THR],
        g_adc[ADC_IDX_BRAKE],
        g_adc[ADC_IDX_CLUTCH],
        g_adc[ADC_IDX_SHFT_X],
        g_adc[ADC_IDX_SHFT_Y],
        gear,
        g_ffb_enabled);
    Serial_Send(buf);
}

/* ── Parse and execute one command line ──────────────────────────────────── */
static void process_line(char *line)
{
    /* Trim trailing whitespace */
    int len = strlen(line);
    while (len > 0 && (line[len-1] == '\r' || line[len-1] == '\n'
                       || line[len-1] == ' '))
        line[--len] = '\0';

    if (len == 0) return;

    /* ── GET_CONFIG ─────────────────────────────────────────────────────── */
    if (strcmp(line, "GET_CONFIG") == 0) {
        send_config();
        return;
    }

    /* ── GET_LIVE ───────────────────────────────────────────────────────── */
    if (strcmp(line, "GET_LIVE") == 0) {
        Serial_SendLive();
        return;
    }

    /* ── SAVE ───────────────────────────────────────────────────────────── */
    if (strcmp(line, "SAVE") == 0) {
        Config_Save();
        Serial_Send("{\"type\":\"ok\",\"msg\":\"Saved to flash\"}\n");
        return;
    }

    /* ── RESET ──────────────────────────────────────────────────────────── */
    if (strcmp(line, "RESET") == 0) {
        Config_Defaults();
        Serial_Send("{\"type\":\"ok\",\"msg\":\"Defaults restored\"}\n");
        return;
    }

    /* ── CALIBRATE_CENTER ───────────────────────────────────────────────── */
    if (strcmp(line, "CALIBRATE_CENTER") == 0) {
        g_cfg.steer_center = g_adc[ADC_IDX_STEER];
        Serial_Send("{\"type\":\"ok\",\"msg\":\"Center saved\"}\n");
        return;
    }

    /* ── CALIBRATE_MIN ──────────────────────────────────────────────────── */
    if (strcmp(line, "CALIBRATE_MIN") == 0) {
        g_cfg.steer_min = g_adc[ADC_IDX_STEER];
        Serial_Send("{\"type\":\"ok\",\"msg\":\"Steer min saved\"}\n");
        return;
    }

    /* ── CALIBRATE_MAX ──────────────────────────────────────────────────── */
    if (strcmp(line, "CALIBRATE_MAX") == 0) {
        g_cfg.steer_max = g_adc[ADC_IDX_STEER];
        Serial_Send("{\"type\":\"ok\",\"msg\":\"Steer max saved\"}\n");
        return;
    }

    /* ── CALIBRATE_PEDALS ───────────────────────────────────────────────── */
    if (strcmp(line, "CAL_THR_MIN")    == 0) { g_cfg.thr_min    = g_adc[ADC_IDX_THR];    Serial_Send("{\"type\":\"ok\"}\n"); return; }
    if (strcmp(line, "CAL_THR_MAX")    == 0) { g_cfg.thr_max    = g_adc[ADC_IDX_THR];    Serial_Send("{\"type\":\"ok\"}\n"); return; }
    if (strcmp(line, "CAL_BRAKE_MIN")  == 0) { g_cfg.brake_min  = g_adc[ADC_IDX_BRAKE];  Serial_Send("{\"type\":\"ok\"}\n"); return; }
    if (strcmp(line, "CAL_BRAKE_MAX")  == 0) { g_cfg.brake_max  = g_adc[ADC_IDX_BRAKE];  Serial_Send("{\"type\":\"ok\"}\n"); return; }
    if (strcmp(line, "CAL_CLUTCH_MIN") == 0) { g_cfg.clutch_min = g_adc[ADC_IDX_CLUTCH]; Serial_Send("{\"type\":\"ok\"}\n"); return; }
    if (strcmp(line, "CAL_CLUTCH_MAX") == 0) { g_cfg.clutch_max = g_adc[ADC_IDX_CLUTCH]; Serial_Send("{\"type\":\"ok\"}\n"); return; }

    /* ── CALIBRATE SHIFTER ZONES ────────────────────────────────────────── */
    if (strcmp(line, "CAL_SHFT_LEFT")  == 0) { g_cfg.shft_x_left  = g_adc[ADC_IDX_SHFT_X]; Serial_Send("{\"type\":\"ok\"}\n"); return; }
    if (strcmp(line, "CAL_SHFT_RIGHT") == 0) { g_cfg.shft_x_right = g_adc[ADC_IDX_SHFT_X]; Serial_Send("{\"type\":\"ok\"}\n"); return; }
    if (strcmp(line, "CAL_SHFT_FWD")   == 0) { g_cfg.shft_y_fwd   = g_adc[ADC_IDX_SHFT_Y]; Serial_Send("{\"type\":\"ok\"}\n"); return; }
    if (strcmp(line, "CAL_SHFT_REV")   == 0) { g_cfg.shft_y_rev   = g_adc[ADC_IDX_SHFT_Y]; Serial_Send("{\"type\":\"ok\"}\n"); return; }

    /* ── SET key value ──────────────────────────────────────────────────── */
    if (strncmp(line, "SET ", 4) == 0) {
        char *rest = line + 4;
        char *sp = strchr(rest, ' ');
        if (!sp) { Serial_Send("{\"type\":\"err\",\"msg\":\"No value\"}\n"); return; }
        *sp = '\0';
        char *key = rest;
        int   val = atoi(sp + 1);
        float fval = strtof(sp + 1, NULL);

        if      (strcmp(key,"range")    ==0) g_cfg.wheel_range     = (uint16_t)val;
        else if (strcmp(key,"ratio")    ==0) g_cfg.pulley_ratio    = fval;
        else if (strcmp(key,"pot_deg")  ==0) g_cfg.pot_degrees     = fval;
        else if (strcmp(key,"invert")   ==0) g_cfg.steer_invert    = val ? 1 : 0;
        else if (strcmp(key,"strength") ==0) g_cfg.ffb_strength    = (uint8_t)val;
        else if (strcmp(key,"spring")   ==0) g_cfg.spring_gain     = (uint8_t)val;
        else if (strcmp(key,"damper")   ==0) g_cfg.damper_gain     = (uint8_t)val;
        else if (strcmp(key,"friction") ==0) g_cfg.friction_gain   = (uint8_t)val;
        else if (strcmp(key,"inertia")  ==0) g_cfg.inertia_gain    = (uint8_t)val;
        else if (strcmp(key,"max_torque")==0)g_cfg.max_torque      = (uint8_t)val;
        else if (strcmp(key,"deadzone") ==0) g_cfg.center_deadzone = (uint8_t)val;
        else if (strcmp(key,"thr_curve")   ==0) g_cfg.thr_curve    = (uint8_t)val;
        else if (strcmp(key,"brake_curve") ==0) g_cfg.brake_curve  = (uint8_t)val;
        else if (strcmp(key,"clutch_curve")==0) g_cfg.clutch_curve = (uint8_t)val;
        else { Serial_Send("{\"type\":\"err\",\"msg\":\"Unknown key\"}\n"); return; }

        Serial_Send("{\"type\":\"ok\"}\n");
        return;
    }

    Serial_Send("{\"type\":\"err\",\"msg\":\"Unknown cmd\"}\n");
}

/* ── Byte-by-byte input from USB CDC ─────────────────────────────────────── */
void Serial_ProcessByte(uint8_t byte)
{
    if (byte == '\n' || byte == '\r') {
        if (s_rx_pos > 0) {
            s_rx[s_rx_pos] = '\0';
            process_line(s_rx);
            s_rx_pos = 0;
        }
    } else {
        if (s_rx_pos < RX_BUF_SIZE - 1)
            s_rx[s_rx_pos++] = (char)byte;
    }
}
