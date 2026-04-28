#include "ffb_engine.h"
#include "ffb_wheel.h"
#include "usb_hid_desc.h"
#include "config.h"
#include "stm32f4xx_hal.h"
#include <math.h>
#include <string.h>

/* ── Globals ─────────────────────────────────────────────────────────────── */
uint8_t g_ffb_device_gain = 255;
uint8_t g_ffb_enabled     = 1;

static FFB_Effect_t s_effects[FFB_MAX_EFFECTS];

static PID_BlockLoad_t s_block_load = {
    .report_id      = REPORT_ID_BLOCK_LOAD,
    .load_status    = 1,
    .ram_pool_avail = 0xFF
};

/* ── Init ────────────────────────────────────────────────────────────────── */
void FFB_Init(void)
{
    memset(s_effects, 0, sizeof(s_effects));
}

/* ── Slot management ─────────────────────────────────────────────────────── */
int8_t FFB_AllocEffect(void)
{
    uint32_t now = HAL_GetTick();

    for (int i = 0; i < FFB_MAX_EFFECTS; i++) {
        /* Accept a FREE slot immediately */
        if (s_effects[i].state == FFB_STATE_FREE) {
            memset(&s_effects[i], 0, sizeof(FFB_Effect_t));
            s_effects[i].state = FFB_STATE_ALLOCATED;
            return (int8_t)i;
        }

        /* Also reclaim an ALLOCATED-but-expired slot.
           Without this, a game that allocates all 8 effects and lets them
           expire naturally will get "effect table full" on the next alloc. */
        if ((s_effects[i].state & FFB_STATE_PLAYING) &&
            s_effects[i].duration_ms != 0xFFFFFFFF &&
            (now - s_effects[i].start_ms) >= s_effects[i].duration_ms) {
            memset(&s_effects[i], 0, sizeof(FFB_Effect_t));
            s_effects[i].state = FFB_STATE_ALLOCATED;
            return (int8_t)i;
        }
    }
    return -1;   /* all 8 slots genuinely in use */
}

void FFB_FreeEffect(uint8_t idx)
{
    if (idx < FFB_MAX_EFFECTS)
        s_effects[idx].state = FFB_STATE_FREE;
}

static uint8_t map_et(uint8_t pid_et)
{
    switch (pid_et) {
        case 0x26: return FFB_ET_SPRING;
        case 0x27: return FFB_ET_DAMPER;
        case 0x28: return FFB_ET_INERTIA;
        case 0x29: return FFB_ET_FRICTION;
        case 0x24: return FFB_ET_CONSTANT;
        case 0x2F: return FFB_ET_SQUARE;
        case 0x30: return FFB_ET_SINE;
        case 0x31: return FFB_ET_TRIANGLE;
        default:   return FFB_ET_NONE;
    }
}

/* ── Process incoming PID report ─────────────────────────────────────────── */
void FFB_ProcessReport(uint8_t report_id, uint8_t *data, uint16_t len)
{
    (void)len;
    switch (report_id) {

    case REPORT_ID_CREATE_EFFECT: {
        int8_t idx = FFB_AllocEffect();
        if (idx >= 0) {
            s_effects[idx].type = map_et(data[1]);
            s_block_load.effect_block_index = (uint8_t)(idx + 1);
            s_block_load.load_status = 1;
        } else {
            s_block_load.load_status = 2;
        }
        break;
    }
    case REPORT_ID_SET_EFFECT: {
        if (len < sizeof(PID_SetEffect_t)) break;
        PID_SetEffect_t *r = (PID_SetEffect_t*)data;
        uint8_t idx = r->effect_block_index - 1;
        if (idx >= FFB_MAX_EFFECTS) break;
        s_effects[idx].type        = map_et(r->effect_type);
        s_effects[idx].duration_ms = (r->duration == 0xFFFF) ? 0xFFFFFFFF
                                     : (uint32_t)r->duration;
        break;
    }
    case REPORT_ID_SET_CONDITION: {
        if (len < sizeof(PID_SetCondition_t)) break;
        PID_SetCondition_t *r = (PID_SetCondition_t*)data;
        uint8_t idx = r->effect_block_index - 1;
        if (idx >= FFB_MAX_EFFECTS) break;
        s_effects[idx].offset         = r->cp_offset;
        s_effects[idx].positive_coeff = r->positive_coeff;
        s_effects[idx].negative_coeff = r->negative_coeff;
        s_effects[idx].deadband       = r->dead_band;
        break;
    }
    case REPORT_ID_SET_CONSTANT: {
        if (len < sizeof(PID_SetConstant_t)) break;
        PID_SetConstant_t *r = (PID_SetConstant_t*)data;
        uint8_t idx = r->effect_block_index - 1;
        if (idx >= FFB_MAX_EFFECTS) break;
        s_effects[idx].magnitude = r->magnitude;
        break;
    }
    case REPORT_ID_SET_PERIODIC: {
        if (len < sizeof(PID_SetPeriodic_t)) break;
        PID_SetPeriodic_t *r = (PID_SetPeriodic_t*)data;
        uint8_t idx = r->effect_block_index - 1;
        if (idx >= FFB_MAX_EFFECTS) break;
        s_effects[idx].magnitude = r->magnitude;
        s_effects[idx].offset    = r->offset;
        s_effects[idx].phase     = r->phase;
        s_effects[idx].period_ms = r->period;
        break;
    }
    case REPORT_ID_SET_ENVELOPE: {
        if (len < sizeof(PID_SetEnvelope_t)) break;
        PID_SetEnvelope_t *r = (PID_SetEnvelope_t*)data;
        uint8_t idx = r->effect_block_index - 1;
        if (idx >= FFB_MAX_EFFECTS) break;
        s_effects[idx].attack_level = r->attack_level;
        s_effects[idx].fade_level   = r->fade_level;
        s_effects[idx].attack_time  = r->attack_time;
        s_effects[idx].fade_time    = r->fade_time;
        break;
    }
    case REPORT_ID_OPERATION: {
        if (len < sizeof(PID_EffectOperation_t)) break;
        PID_EffectOperation_t *r = (PID_EffectOperation_t*)data;
        uint8_t idx = r->effect_block_index - 1;
        if (idx >= FFB_MAX_EFFECTS) break;
        switch (r->operation) {
            case 1: case 2:
                if (r->operation == 2)
                    for (int i = 0; i < FFB_MAX_EFFECTS; i++)
                        if (i != idx) s_effects[i].state &= ~FFB_STATE_PLAYING;
                s_effects[idx].state   |= FFB_STATE_PLAYING;
                s_effects[idx].start_ms = HAL_GetTick();
                break;
            case 3:
                s_effects[idx].state &= ~FFB_STATE_PLAYING;
                break;
        }
        break;
    }
    case REPORT_ID_DEVICE_CTRL: {
        if (len < 2) break;
        uint8_t ctrl = data[1];
        if (ctrl & 0x01) g_ffb_enabled = 1;
        if (ctrl & 0x02) g_ffb_enabled = 0;
        if (ctrl & 0x04)
            for (int i = 0; i < FFB_MAX_EFFECTS; i++)
                s_effects[i].state &= ~FFB_STATE_PLAYING;
        if (ctrl & 0x08) { FFB_Init(); g_ffb_enabled = 1; }
        break;
    }
    case REPORT_ID_DEVICE_GAIN:
        if (len >= 2) g_ffb_device_gain = data[1];
        break;
    }
}

/* ══════════════════════════════════════════════════════════════════════════
   EFFECT MIXING — VALIDATED
   ══════════════════════════════════════════════════════════════════════════

   Rules applied here:
   1. Effects are split into two buckets: POSITION effects (spring) and
      DYNAMIC effects (damper, friction, inertia, constant, periodic).
   2. Position bucket: all springs are summed then clamped to ±SPRING_MAX.
      This prevents two spring effects fighting each other to ±infinity.
   3. Dynamic bucket: summed separately, clamped to ±DYNAMIC_MAX.
   4. Combined: position + dynamic, final clamp to ±1000.
   5. Spring always has priority — if spring is near its ceiling it is not
      reduced to make room for dynamic effects.
   6. Envelope scaling applied per-effect before mixing.
   7. Opposing constant forces are allowed to cancel (this is correct
      physics — the game may send opposing forces intentionally).          */

#define SPRING_MAX   800   /* spring bucket ceiling before global clamp    */
#define DYNAMIC_MAX  600   /* dynamic bucket ceiling                       */

static inline int32_t clamp(int32_t v, int32_t lo, int32_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* Apply envelope to a raw torque value.
   Returns scaled torque accounting for attack and fade phases.            */
static int32_t apply_envelope(FFB_Effect_t *e, int32_t torque, uint32_t now)
{
    /* If no envelope defined (all zeros) return torque unchanged */
    if (e->attack_time == 0 && e->fade_time == 0) return torque;

    uint32_t elapsed = now - e->start_ms;
    float scale = 1.0f;

    if (elapsed < e->attack_time && e->attack_time > 0) {
        /* Attack phase: ramp from attack_level to full magnitude */
        float t = (float)elapsed / (float)e->attack_time;
        float a = (float)e->attack_level / 10000.0f;
        scale = a + (1.0f - a) * t;
    } else if (e->duration_ms != 0xFFFFFFFF) {
        uint32_t fade_start = e->duration_ms > e->fade_time
                              ? e->duration_ms - e->fade_time : 0;
        if (elapsed >= fade_start && e->fade_time > 0) {
            /* Fade phase: ramp from full to fade_level */
            float t = (float)(elapsed - fade_start) / (float)e->fade_time;
            if (t > 1.0f) t = 1.0f;
            float f = (float)e->fade_level / 10000.0f;
            scale = 1.0f - (1.0f - f) * t;
        }
    }

    return (int32_t)((float)torque * scale);
}

/* ── Per-effect torque calculation ──────────────────────────────────────── */
static int32_t calc_effect(FFB_Effect_t *e, float angle,
                            float velocity, uint32_t now)
{
    const float SCALE = 0.1f; /* PID range ±10000 → our range ±1000        */
    int32_t torque = 0;

    switch (e->type) {

    case FFB_ET_SPRING: {
        float error = angle - (float)e->offset * SCALE;
        float dz    = (float)e->deadband * SCALE;
        if      (error >  dz) error -= dz;
        else if (error < -dz) error += dz;
        else                  error  = 0.0f;

        int16_t coeff = (error >= 0) ? e->positive_coeff : e->negative_coeff;
        torque = (int32_t)(error * (float)coeff * SCALE * 0.01f);
        torque = -torque; /* spring opposes displacement                    */
        torque = (torque * g_cfg.spring_gain) / 100;
        break;
    }

    case FFB_ET_DAMPER: {
        int16_t coeff = (velocity >= 0) ? e->positive_coeff : e->negative_coeff;
        torque = (int32_t)(velocity * (float)coeff * SCALE * 0.001f);
        torque = -torque; /* damper opposes velocity                        */
        torque = (torque * g_cfg.damper_gain) / 100;
        break;
    }

    case FFB_ET_FRICTION: {
        /* Friction: constant force opposing any motion above threshold     */
        const float VEL_THRESHOLD = 2.0f; /* deg/s — ignore micro-jitter   */
        float sign = 0.0f;
        if      (velocity >  VEL_THRESHOLD) sign = -1.0f;
        else if (velocity < -VEL_THRESHOLD) sign =  1.0f;
        torque = (int32_t)(sign * (float)e->positive_coeff * SCALE * 0.1f);
        torque = (torque * g_cfg.friction_gain) / 100;
        break;
    }

    case FFB_ET_INERTIA: {
        /* Inertia: resist acceleration — approximate from velocity delta   */
        /* Uses a simple first-order model                                  */
        static float prev_vel = 0.0f;
        float accel = velocity - prev_vel;
        prev_vel = velocity;
        torque = (int32_t)(-accel * (float)e->positive_coeff * SCALE * 0.005f);
        torque = (torque * g_cfg.inertia_gain) / 100;
        break;
    }

    case FFB_ET_CONSTANT: {
        torque = (int32_t)((float)e->magnitude * SCALE);
        torque = (torque * g_cfg.ffb_strength) / 100;
        break;
    }

    case FFB_ET_SINE: {
        if (e->period_ms == 0) break;
        float t = (float)((now - e->start_ms) % e->period_ms)
                  / (float)e->period_ms;
        float phase = (float)e->phase / 35999.0f * 2.0f * 3.14159f;
        torque = (int32_t)(((float)e->magnitude * sinf(2.0f*3.14159f*t + phase)
                            + (float)e->offset) * SCALE);
        torque = (torque * g_cfg.ffb_strength) / 100;
        break;
    }

    case FFB_ET_SQUARE: {
        if (e->period_ms == 0) break;
        uint32_t phase = (now - e->start_ms) % e->period_ms;
        int32_t mag = (int32_t)((float)e->magnitude * SCALE);
        torque = (phase < (e->period_ms / 2)) ? mag : -mag;
        torque += (int32_t)((float)e->offset * SCALE);
        torque = (torque * g_cfg.ffb_strength) / 100;
        break;
    }

    case FFB_ET_TRIANGLE: {
        if (e->period_ms == 0) break;
        float t = (float)((now - e->start_ms) % e->period_ms)
                  / (float)e->period_ms;
        float tri = (t < 0.25f) ? (4.0f*t)
                  : (t < 0.75f) ? (2.0f - 4.0f*t)
                  :               (4.0f*t - 4.0f);
        torque = (int32_t)((tri * (float)e->magnitude
                            + (float)e->offset) * SCALE);
        torque = (torque * g_cfg.ffb_strength) / 100;
        break;
    }

    default: break;
    }

    return apply_envelope(e, torque, now);
}

/* ── Main calculation with validated mixing ─────────────────────────────── */
int32_t FFB_Calculate(float wheel_angle_deg, float wheel_velocity)
{
    if (!g_ffb_enabled) return 0;

    uint32_t now = HAL_GetTick();

    /* Two separate buckets */
    int32_t spring_sum  = 0;   /* position effects                          */
    int32_t dynamic_sum = 0;   /* all other effects                         */

    for (int i = 0; i < FFB_MAX_EFFECTS; i++) {
        FFB_Effect_t *e = &s_effects[i];
        if (!(e->state & FFB_STATE_PLAYING)) continue;

        /* Expire finished effects */
        if (e->duration_ms != 0xFFFFFFFF) {
            if ((now - e->start_ms) >= e->duration_ms) {
                e->state &= ~FFB_STATE_PLAYING;
                continue;
            }
        }

        int32_t t = calc_effect(e, wheel_angle_deg, wheel_velocity, now);

        /* Route to correct bucket */
        if (e->type == FFB_ET_SPRING) {
            spring_sum += t;
        } else {
            dynamic_sum += t;
        }
    }

    /* Clamp each bucket independently */
    spring_sum  = clamp(spring_sum,  -SPRING_MAX,  SPRING_MAX);
    dynamic_sum = clamp(dynamic_sum, -DYNAMIC_MAX, DYNAMIC_MAX);

    /* Combine: spring takes priority.
       If spring is already using most of its budget, dynamic effects are
       scaled down proportionally so they never override the spring entirely. */
    int32_t spring_headroom = SPRING_MAX - (spring_sum < 0
                              ? -spring_sum : spring_sum);
    int32_t dynamic_allowed = (spring_headroom * DYNAMIC_MAX) / SPRING_MAX;
    dynamic_sum = clamp(dynamic_sum, -dynamic_allowed, dynamic_allowed);

    int32_t total = spring_sum + dynamic_sum;

    /* Apply global device gain from host */
    total = (total * (int32_t)g_ffb_device_gain) / 255;

    /* Final hard clamp */
    return clamp(total, -1000, 1000);
}
