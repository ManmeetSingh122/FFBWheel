#include "config.h"
#include "stm32f4xx_hal.h"
#include <string.h>

/* Global config instance */
WheelConfig_t g_cfg;

/* ── Factory defaults ────────────────────────────────────────────────────── */
void Config_Defaults(void)
{
    memset(&g_cfg, 0, sizeof(WheelConfig_t));
    g_cfg.magic           = CONFIG_MAGIC;
    g_cfg.wheel_range     = DEFAULT_WHEEL_RANGE;
    g_cfg.pulley_ratio    = DEFAULT_PULLEY_RATIO;
    g_cfg.pot_degrees     = DEFAULT_POT_DEGREES;
    g_cfg.steer_invert    = 0;
    g_cfg.steer_center    = ADC_MAX / 2;   /* 2047 */
    g_cfg.steer_min       = 0;
    g_cfg.steer_max       = ADC_MAX;

    g_cfg.ffb_strength    = DEFAULT_FFB_STRENGTH;
    g_cfg.spring_gain     = DEFAULT_SPRING_GAIN;
    g_cfg.damper_gain     = DEFAULT_DAMPER_GAIN;
    g_cfg.friction_gain   = DEFAULT_FRICTION_GAIN;
    g_cfg.inertia_gain    = DEFAULT_INERTIA_GAIN;
    g_cfg.max_torque      = DEFAULT_MAX_TORQUE;
    g_cfg.center_deadzone = DEFAULT_CENTER_DEADZONE;

    g_cfg.thr_min    = 0;   g_cfg.thr_max    = ADC_MAX;
    g_cfg.brake_min  = 0;   g_cfg.brake_max  = ADC_MAX;
    g_cfg.clutch_min = 0;   g_cfg.clutch_max = ADC_MAX;
    g_cfg.thr_curve  = 0;
    g_cfg.brake_curve  = 1; /* square for brake feel */
    g_cfg.clutch_curve = 0;

    /* Shifter gate thresholds (3-zone split of 0-4095) */
    g_cfg.shft_x_left  = 1365;
    g_cfg.shft_x_right = 2730;
    g_cfg.shft_y_fwd   = 2730;
    g_cfg.shft_y_rev   = 1365;
}

/* ── Load from flash ─────────────────────────────────────────────────────── */
void Config_Load(void)
{
    const WheelConfig_t *flash = (const WheelConfig_t*)CONFIG_FLASH_ADDR;

    if (flash->magic == CONFIG_MAGIC) {
        memcpy(&g_cfg, flash, sizeof(WheelConfig_t));
    } else {
        Config_Defaults();
    }
}

/* ── Save to flash ───────────────────────────────────────────────────────── */
void Config_Save(void)
{
    /* 1. Unlock flash */
    HAL_FLASH_Unlock();

    /* 2. Erase sector 7 (last sector, STM32F401CCU6 has sectors 0-7) */
    FLASH_EraseInitTypeDef erase = {0};
    erase.TypeErase    = FLASH_TYPEERASE_SECTORS;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;  /* 2.7V–3.6V */
    erase.Sector       = FLASH_SECTOR_7;
    erase.NbSectors    = 1;
    uint32_t sector_error = 0;
    HAL_FLASHEx_Erase(&erase, &sector_error);

    /* 3. Write word by word */
    uint32_t addr = CONFIG_FLASH_ADDR;
    uint32_t *src = (uint32_t*)&g_cfg;
    uint32_t words = (sizeof(WheelConfig_t) + 3) / 4;

    for (uint32_t i = 0; i < words; i++) {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, src[i]);
        addr += 4;
    }

    /* 4. Lock flash */
    HAL_FLASH_Lock();
}
