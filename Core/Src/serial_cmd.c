#include "serial_cmd.h"
#include "config.h"
#include "ffb_wheel.h"
#include "ffb_engine.h"
#include "adc_input.h"
#include "failsafe.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

extern uint8_t CDC_Transmit_FS(uint8_t *Buf, uint16_t Len);

/* ── RX buffer (filled from USB interrupt) ───────────────────────────────── */
#define RX_BUF_SIZE  128
static char    s_rx[RX_BUF_SIZE];
static uint8_t s_rx_pos = 0;
static volatile uint8_t cmd_ready = 0;

/* ── TX buffer — single slot queue ──────────────────────────────────────── */
#define TX_BUF_SIZE  512
static char    tx_buf[TX_BUF_SIZE];
static volatile uint8_t tx_ready = 0;

void Serial_Send(const char *str)
{
    if (tx_ready) return;   /* previous message still pending — drop new one */

    strncpy(tx_buf, str, TX_BUF_SIZE - 1);
    tx_buf[TX_BUF_SIZE - 1] = '\0';
    tx_ready = 1;
}

/* ── GET_CONFIG ──────────────────────────────────────────────────────────── */
static void send_config(void)
{
    char buf[512];
    snprintf(buf, sizeof(buf),
        "{\"type\":\"config\","
        "\"range\":%u,\"ratio\":%.2f,\"pot_deg\":%.1f,\"invert\":%u,"
        "\"strength\":%u,\"spring\":%u,\"damper\":%u,"
        "\"friction\":%u,\"inertia\":%u,"
        "\"max_torque\":%u,\"deadzone\":%u,"
        "\"thr_min\":%u,\"thr_max\":%u,"
        "\"brake_min\":%u,\"brake_max\":%u,"
        "\"clutch_min\":%u,\"clutch_max\":%u,"
        "\"thr_curve\":%u,\"brake_curve\":%u,\"clutch_curve\":%u,"
        "\"sx_left\":%u,\"sx_right\":%u,"
        "\"sy_fwd\":%u,\"sy_rev\":%u,"
        "\"steer_min\":%u,\"steer_max\":%u,\"steer_center\":%u,"
        "\"fw_ver\":\"%u.%u\"}\n",
        g_cfg.wheel_range,
        (double)g_cfg.pulley_ratio, (double)g_cfg.pot_degrees,
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

/* ── GET_LIVE ────────────────────────────────────────────────────────────── */
void Serial_SendLive(void)
{
    char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"type\":\"live\","
        "\"angle\":%.1f,\"steer_raw\":%u,"
        "\"thr\":%u,\"brake\":%u,\"clutch\":%u,"
        "\"shft_x\":%u,\"shft_y\":%u,"
        "\"gear\":%u,\"ffb_en\":%u}\n",
        (double)ADC_GetWheelAngle(),
        g_adc[ADC_IDX_STEER],
        g_adc[ADC_IDX_THR], g_adc[ADC_IDX_BRAKE], g_adc[ADC_IDX_CLUTCH],
        g_adc[ADC_IDX_SHFT_X], g_adc[ADC_IDX_SHFT_Y],
        (uint8_t)ADC_GetGear(), g_ffb_enabled);
    Serial_Send(buf);
}

/* ── Command processor (called from Serial_Task only) ───────────────────── */
static void process_line(char *line)
{
    int len = strlen(line);
    while (len > 0 && (line[len-1]=='\r'||line[len-1]=='\n'||line[len-1]==' '))
        line[--len] = '\0';
    if (len == 0) return;

    if (!strcmp(line,"GET_CONFIG"))      { send_config(); return; }
    if (!strcmp(line,"GET_LIVE"))        { Serial_SendLive(); return; }
    if (!strcmp(line,"SAVE"))            { uint8_t ok = Config_Save(); Serial_Send(ok ? "{\"type\":\"ok\",\"msg\":\"Saved\"}\n" : "{\"type\":\"err\",\"msg\":\"Flash error\"}\n"); return; }
    if (!strcmp(line,"RESET"))           { Config_Defaults(); Serial_Send("{\"type\":\"ok\",\"msg\":\"Defaults\"}\n"); return; }
    if (!strcmp(line,"GET_STATUS")) {
        char buf[128];
        snprintf(buf, sizeof(buf),
            "{\"type\":\"status\",\"failsafe\":%u,\"reason\":\"%s\",\"ffb_en\":%u,\"uptime\":%lu}\n",
            Failsafe_IsTriggered(), Failsafe_Reason(),
            g_ffb_enabled, HAL_GetTick());
        Serial_Send(buf);
        return;
    }
    if (!strcmp(line,"CLEAR_FAILSAFE")) { Failsafe_Clear(); Serial_Send("{\"type\":\"ok\",\"msg\":\"Failsafe cleared\"}\n"); return; }
    if (!strcmp(line,"CALIBRATE_CENTER")){ g_cfg.steer_center=g_adc[ADC_IDX_STEER]; Serial_Send("{\"type\":\"ok\",\"msg\":\"Center saved\"}\n"); return; }
    if (!strcmp(line,"CALIBRATE_MIN"))   { g_cfg.steer_min=g_adc[ADC_IDX_STEER];    Serial_Send("{\"type\":\"ok\",\"msg\":\"Min saved\"}\n"); return; }
    if (!strcmp(line,"CALIBRATE_MAX"))   { g_cfg.steer_max=g_adc[ADC_IDX_STEER];    Serial_Send("{\"type\":\"ok\",\"msg\":\"Max saved\"}\n"); return; }
    if (!strcmp(line,"CAL_THR_MIN"))     { g_cfg.thr_min=g_adc[ADC_IDX_THR];        Serial_Send("{\"type\":\"ok\"}\n"); return; }
    if (!strcmp(line,"CAL_THR_MAX"))     { g_cfg.thr_max=g_adc[ADC_IDX_THR];        Serial_Send("{\"type\":\"ok\"}\n"); return; }
    if (!strcmp(line,"CAL_BRAKE_MIN"))   { g_cfg.brake_min=g_adc[ADC_IDX_BRAKE];    Serial_Send("{\"type\":\"ok\"}\n"); return; }
    if (!strcmp(line,"CAL_BRAKE_MAX"))   { g_cfg.brake_max=g_adc[ADC_IDX_BRAKE];    Serial_Send("{\"type\":\"ok\"}\n"); return; }
    if (!strcmp(line,"CAL_CLUTCH_MIN"))  { g_cfg.clutch_min=g_adc[ADC_IDX_CLUTCH];  Serial_Send("{\"type\":\"ok\"}\n"); return; }
    if (!strcmp(line,"CAL_CLUTCH_MAX"))  { g_cfg.clutch_max=g_adc[ADC_IDX_CLUTCH];  Serial_Send("{\"type\":\"ok\"}\n"); return; }
    if (!strcmp(line,"CAL_SHFT_LEFT"))   { g_cfg.shft_x_left=g_adc[ADC_IDX_SHFT_X]; Serial_Send("{\"type\":\"ok\"}\n"); return; }
    if (!strcmp(line,"CAL_SHFT_RIGHT"))  { g_cfg.shft_x_right=g_adc[ADC_IDX_SHFT_X];Serial_Send("{\"type\":\"ok\"}\n"); return; }
    if (!strcmp(line,"CAL_SHFT_FWD"))    { g_cfg.shft_y_fwd=g_adc[ADC_IDX_SHFT_Y];  Serial_Send("{\"type\":\"ok\"}\n"); return; }
    if (!strcmp(line,"CAL_SHFT_REV"))    { g_cfg.shft_y_rev=g_adc[ADC_IDX_SHFT_Y];  Serial_Send("{\"type\":\"ok\"}\n"); return; }

    if (!strncmp(line,"SET ",4)) {
        char *rest = line+4;
        char *sp = strchr(rest,' ');
        if (!sp) { Serial_Send("{\"type\":\"err\",\"msg\":\"No value\"}\n"); return; }
        *sp = '\0';
        char *key = rest;
        int   val  = atoi(sp+1);
        float fval = strtof(sp+1, NULL);
        if      (!strcmp(key,"range"))        g_cfg.wheel_range     = (uint16_t)val;
        else if (!strcmp(key,"ratio"))        g_cfg.pulley_ratio    = fval;
        else if (!strcmp(key,"pot_deg"))      g_cfg.pot_degrees     = fval;
        else if (!strcmp(key,"invert"))       g_cfg.steer_invert    = val ? 1 : 0;
        else if (!strcmp(key,"strength"))     g_cfg.ffb_strength    = (uint8_t)val;
        else if (!strcmp(key,"spring"))       g_cfg.spring_gain     = (uint8_t)val;
        else if (!strcmp(key,"damper"))       g_cfg.damper_gain     = (uint8_t)val;
        else if (!strcmp(key,"friction"))     g_cfg.friction_gain   = (uint8_t)val;
        else if (!strcmp(key,"inertia"))      g_cfg.inertia_gain    = (uint8_t)val;
        else if (!strcmp(key,"max_torque"))   g_cfg.max_torque      = (uint8_t)val;
        else if (!strcmp(key,"deadzone"))     g_cfg.center_deadzone = (uint8_t)val;
        else if (!strcmp(key,"thr_curve"))    g_cfg.thr_curve       = (uint8_t)val;
        else if (!strcmp(key,"brake_curve"))  g_cfg.brake_curve     = (uint8_t)val;
        else if (!strcmp(key,"clutch_curve")) g_cfg.clutch_curve    = (uint8_t)val;
        else { Serial_Send("{\"type\":\"err\",\"msg\":\"Unknown key\"}\n"); return; }
        Serial_Send("{\"type\":\"ok\"}\n");
        return;
    }

    Serial_Send("{\"type\":\"err\",\"msg\":\"Unknown cmd\"}\n");
}

/* ── Called from USB interrupt — ONLY buffers, NEVER transmits ───────────── */
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

/* ── Called from main loop — safe to transmit here ──────────────────────── */
void Serial_Task(void)
{
    /* ── Step 1: Try to flush pending TX ────────────────────────────────── */
    if (tx_ready) {
        uint16_t len = (uint16_t)strlen(tx_buf);
        if (len == 0) {
            tx_ready = 0;
        } else {
            uint8_t result = CDC_Transmit_FS((uint8_t*)tx_buf, len);
            if (result == 0) {
                /* USBD_OK — data is now in the USB hardware buffer */
                tx_ready = 0;
            }
            /* If USBD_BUSY (1): CDC still transmitting previous packet.
               Keep tx_ready=1 and retry on the very next main loop call.
               This is the key fix — do NOT drop on BUSY.
               NEVER drop on BUSY because this would permanently lose
               GET_CONFIG responses if live data happened to be sending. */

            /* If USBD_FAIL (5): USB not configured yet (device just
               connected). Keep tx_ready=1 to retry when ready.
               Reset after 200ms to avoid holding stale data forever. */
            else if (result == 5) {
                static uint32_t t_fail = 0;
                uint32_t now = HAL_GetTick();
                if (t_fail == 0) t_fail = now;
                if ((uint32_t)(now - t_fail) > 200) {
                    tx_ready = 0;   /* give up after 200ms of FAIL */
                    t_fail = 0;
                }
            }
        }
        return;  /* ← Process only TX this iteration; command waits for next */
    }

    /* ── Step 2: Process pending received command ────────────────────────── */
    /* Only runs when TX is idle (tx_ready == 0), so the response we queue
       here will be sent on the very next Serial_Task call.                  */
    if (!cmd_ready) return;
    cmd_ready = 0;
    char line[RX_BUF_SIZE];
    memcpy(line, s_rx, s_rx_pos + 1);   /* copy before resetting pos */
    s_rx_pos = 0;
    process_line(line);
}
