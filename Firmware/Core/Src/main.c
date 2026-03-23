#include "ffb_wheel.h"
#include "motor.h"
#include "adc_input.h"
#include "ffb_engine.h"
#include "config.h"
#include "serial_cmd.h"
#include "usb_hid_desc.h"
#include "ffb_timer.h"    /* NEW */
#include "failsafe.h"     /* NEW */
#include "stm32f4xx_hal.h"

extern void MX_USB_DEVICE_Init(void);
extern uint8_t USBD_HID_SendReport(void *pdev, uint8_t *report, uint16_t len);
extern void *hUsbDeviceFS;

volatile uint16_t g_adc[ADC_CHANNELS];

/* ── Build and send HID input report ────────────────────────────────────── */
static void send_hid_report(void)
{
    HID_InputReport_t rpt;
    rpt.report_id = REPORT_ID_INPUT;

    float angle   = ADC_GetWheelAngle();
    float max_ang = (float)g_cfg.wheel_range / 2.0f;
    float norm    = angle / max_ang;
    if (norm >  1.0f) norm =  1.0f;
    if (norm < -1.0f) norm = -1.0f;
    rpt.steering  = (int16_t)(norm * 32767.0f);

    rpt.throttle = ADC_GetAxis(ADC_IDX_THR);
    rpt.brake    = ADC_GetAxis(ADC_IDX_BRAKE);
    rpt.clutch   = ADC_GetAxis(ADC_IDX_CLUTCH);

    rpt.buttons = 0;
    for (uint8_t b = 0; b < 8; b++) {
        if (HAL_GPIO_ReadPin(GPIOB, (1U << b)) == GPIO_PIN_RESET)
            rpt.buttons |= (1U << b);
    }

    rpt.gear = (uint8_t)ADC_GetGear();

    USBD_HID_SendReport(hUsbDeviceFS, (uint8_t*)&rpt, sizeof(rpt));

    /* Tell failsafe the HID pipe is alive */
    Failsafe_KickHID();   /* NEW */
}

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_ON;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM       = 25;
    osc.PLL.PLLN       = 336;
    osc.PLL.PLLP       = RCC_PLLP_DIV4;
    osc.PLL.PLLQ       = 7;
    HAL_RCC_OscConfig(&osc);
    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                         RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2);
}

static void Buttons_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin  = 0x00FF;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &gpio);
}

/* ── Main ────────────────────────────────────────────────────────────────── */
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    Config_Load();
    Motor_Init();
    ADC_Input_Init();
    Buttons_Init();
    FFB_Init();

    MX_USB_DEVICE_Init();
    HAL_Delay(500);

    /* NEW: Init failsafe before timer so it's ready when first tick fires */
    Failsafe_Init();

    /* NEW: Start 1kHz hardware timer — FFB loop runs from interrupt now   */
    FFB_Timer_Init();

    /* ── Timing for non-FFB tasks ────────────────────────────────────────── */
    uint32_t t_hid  = 0;
    uint32_t t_live = 0;

    while (1)
    {
        uint32_t now = HAL_GetTick();

        /* ── HID Input Report @ 1 kHz ────────────────────────────────────── */
        if ((now - t_hid) >= 1) {
            t_hid = now;
            send_hid_report();
        }

        /* ── Live serial data @ 10 Hz ────────────────────────────────────── */
        if ((now - t_live) >= 100) {
            t_live = now;
            Serial_SendLive();

            /* Send failsafe status if triggered so web app can show it     */
            if (Failsafe_IsTriggered()) {
                char buf[80];
                snprintf(buf, sizeof(buf),
                    "{\"type\":\"failsafe\",\"reason\":\"%s\"}\n",
                    Failsafe_Reason());
                Serial_Send(buf);
            }
        }

        /* ── NOTE: FFB calculation and Motor_Set() are GONE from here ──────
           They now run at exactly 1 kHz from FFB_Timer_Tick() in
           ffb_timer.c via TIM2 interrupt.
           ──────────────────────────────────────────────────────────────── */
    }
}

/* ── USB callbacks ───────────────────────────────────────────────────────── */
void USBD_HID_OutCallback(uint8_t report_id, uint8_t *buf, uint16_t len)
{
    FFB_ProcessReport(report_id, buf, len);
    Failsafe_KickUSB();   /* NEW: any FFB traffic = USB alive              */
}

void CDC_ReceiveCallback(uint8_t *buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
        Serial_ProcessByte(buf[i]);
}
