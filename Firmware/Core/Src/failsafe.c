#include "failsafe.h"
#include "motor.h"
#include "stm32f4xx_hal.h"
#include <string.h>

/* ── Tunable thresholds ──────────────────────────────────────────────────── */
#define USB_TIMEOUT_MS       200   /* motor stops if no USB FFB for 200ms  */
#define HID_TIMEOUT_MS       500   /* motor stops if no HID IN for 500ms   */
#define STUCK_TORQUE_PCT      80   /* % above which stuck detection active  */
#define STUCK_TIME_MS       2000   /* ms at high torque before stuck trip   */
#define STUCK_MOVE_DEG       2.0f  /* minimum movement to prove not stuck   */
#define WDT_TIMEOUT_MS       500   /* IWDG reset after 500ms no kick        */

/* ── State ───────────────────────────────────────────────────────────────── */
static volatile uint32_t s_usb_last_ms   = 0;
static volatile uint32_t s_hid_last_ms   = 0;
static volatile uint32_t s_stuck_start   = 0;
static volatile float    s_stuck_ref_ang = 0.0f;
static volatile uint8_t  s_triggered     = 0;
static volatile uint8_t  s_usb_ever_seen = 0; /* don't trip before first USB */
static const char       *s_reason        = "none";

static IWDG_HandleTypeDef hiwdg;

/* ── IWDG watchdog init ──────────────────────────────────────────────────── */
static void init_watchdog(void)
{
    /* IWDG clock = LSI ~32 kHz
       Prescaler = 32 → 32000/32 = 1000 Hz
       Reload    = 499 → 499/1000 = ~500ms timeout                          */
    hiwdg.Instance       = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_32;
    hiwdg.Init.Reload    = 499;
    HAL_IWDG_Init(&hiwdg);
}

/* ── Public API ──────────────────────────────────────────────────────────── */
void Failsafe_Init(void)
{
    s_usb_last_ms   = HAL_GetTick();
    s_hid_last_ms   = HAL_GetTick();
    s_stuck_start   = 0;
    s_triggered     = 0;
    s_usb_ever_seen = 0;
    s_reason        = "none";
    init_watchdog();
}

void Failsafe_KickUSB(void)
{
    s_usb_last_ms   = HAL_GetTick();
    s_usb_ever_seen = 1;
    /* If we were triggered by USB timeout, clear it now */
    if (s_triggered && strcmp(s_reason, "USB timeout") == 0) {
        s_triggered = 0;
        s_reason    = "none";
    }
}

void Failsafe_KickHID(void)
{
    s_hid_last_ms = HAL_GetTick();
}

void Failsafe_KickWDT(void)
{
    HAL_IWDG_Refresh(&hiwdg);
}

void Failsafe_Update(int32_t torque, float angle)
{
    /* Pet the watchdog — called every 1ms from TIM2 tick                   */
    Failsafe_KickWDT();

    /* Already triggered — just keep motor stopped, don't re-evaluate       */
    if (s_triggered) return;

    uint32_t now = HAL_GetTick();

    /* ── 1. USB FFB timeout ─────────────────────────────────────────────── */
    if (s_usb_ever_seen) {
        if ((now - s_usb_last_ms) > USB_TIMEOUT_MS) {
            s_triggered = 1;
            s_reason    = "USB timeout";
            Motor_Stop();
            return;
        }
    }

    /* ── 2. HID IN timeout (USB physically unplugged) ───────────────────── */
    if ((now - s_hid_last_ms) > HID_TIMEOUT_MS) {
        /* Only trigger if we've seen some HID traffic before              */
        if (s_hid_last_ms != 0) {
            s_triggered = 1;
            s_reason    = "HID disconnect";
            Motor_Stop();
            return;
        }
    }

    /* ── 3. Stuck motor detection ───────────────────────────────────────── */
    /* Convert torque (-1000 to +1000) to percentage                        */
    int32_t torque_pct = (torque < 0) ? -torque : torque;
    torque_pct = (torque_pct * 100) / 1000;

    if (torque_pct >= STUCK_TORQUE_PCT) {
        if (s_stuck_start == 0) {
            /* Start stuck timer and record reference angle                 */
            s_stuck_start   = now;
            s_stuck_ref_ang = angle;
        } else {
            /* Check if wheel has moved enough to prove not stuck           */
            float moved = angle - s_stuck_ref_ang;
            if (moved < 0.0f) moved = -moved;

            if (moved >= STUCK_MOVE_DEG) {
                /* Wheel is moving — reset stuck timer                      */
                s_stuck_start   = 0;
                s_stuck_ref_ang = angle;
            } else if ((now - s_stuck_start) >= STUCK_TIME_MS) {
                /* High torque, no movement, for too long — STUCK          */
                s_triggered = 1;
                s_reason    = "Stuck motor";
                Motor_Stop();
                return;
            }
        }
    } else {
        /* Torque below threshold — reset stuck timer                       */
        s_stuck_start = 0;
    }
}

uint8_t Failsafe_IsTriggered(void)
{
    return s_triggered;
}

void Failsafe_Clear(void)
{
    s_triggered     = 0;
    s_reason        = "none";
    s_stuck_start   = 0;
    s_usb_last_ms   = HAL_GetTick();
    s_hid_last_ms   = HAL_GetTick();
}

const char* Failsafe_Reason(void)
{
    return s_reason;
}
