#include "ffb_wheel.h"
#include "motor.h"
#include "adc_input.h"
#include "encoder.h"
#include "ffb_engine.h"
#include "config.h"
#include "serial_cmd.h"
#include "usb_hid_desc.h"
#include "ffb_timer.h"
#include "failsafe.h"
#include "stm32f4xx_hal.h"
#include "usbd_customhid.h"
#include "usbd_composite.h"

extern void MX_USB_DEVICE_Init(void);
extern USBD_HandleTypeDef hUsbDeviceFS;
extern volatile uint16_t g_adc[ADC_CHANNELS];

static void send_hid_report(void)
{
    static HID_InputReport_t rpt;
    rpt.report_id = REPORT_ID_INPUT;

    float angle;
    if (g_cfg.input_mode == INPUT_MODE_ENCODER) {
        angle = Encoder_GetAngle();
    } else {
        angle = ADC_GetWheelAngle();
    }

    float max_ang = (float)g_cfg.wheel_range / 2.0f;
    if (max_ang < 1.0f) max_ang = 1.0f;
    float norm = angle / max_ang;
    if (norm >  1.0f) norm =  1.0f;
    if (norm < -1.0f) norm = -1.0f;
    rpt.steering = (int16_t)(norm * 32767.0f);

    rpt.throttle = ADC_GetAxis(ADC_IDX_THR);
    rpt.brake    = ADC_GetAxis(ADC_IDX_BRAKE);
    rpt.clutch   = ADC_GetAxis(ADC_IDX_CLUTCH);

    rpt.buttons = 0;
    for (uint8_t b = 0; b < 8; b++) {
        if (HAL_GPIO_ReadPin(GPIOB, (1U << b)) == GPIO_PIN_RESET)
            rpt.buttons |= (1U << b);
    }
    rpt.gear = (uint8_t)ADC_GetGear();

    HID_SendReport_FS((uint8_t*)&rpt, sizeof(rpt));
    Failsafe_KickHID();
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

static void LED_Init(void)
{
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin   = GPIO_PIN_13;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &gpio);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq() / 1000);
    HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
    HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);

    LED_Init();
    Config_Load();
    Motor_Init();
    ADC_Input_Init();
    Buttons_Init();
    FFB_Init();

    /* Init encoder if configured — must be after Config_Load()            */
    if (g_cfg.input_mode == INPUT_MODE_ENCODER) {
        Encoder_Init(g_cfg.encoder_ppr);
    }

    MX_USB_DEVICE_Init();
    /* USB OTG priority (5) set in HAL_PCD_MspInit in usbd_conf.c.
       TIM2 is priority 0 — WDT kick can never be starved by USB.          */

    uint32_t start = HAL_GetTick();
    while ((uint32_t)(HAL_GetTick() - start) < 500) {}

    FFB_Timer_Init();
    Failsafe_Init();

    uint32_t t_hid  = HAL_GetTick();
    uint32_t t_live = HAL_GetTick();
    uint32_t t_led  = HAL_GetTick();

    while (1)
    {
        uint32_t now = HAL_GetTick();

        if ((uint32_t)(now - t_hid) >= 5) {
            t_hid = now;
            ADC_Filter_Update();
            send_hid_report();
        }

        if ((uint32_t)(now - t_live) >= 300) {
            t_live = now;
            if (CDC_IsConnected()) {
                Serial_SendLive();
            }
        }

        Serial_Task();

        if ((uint32_t)(now - t_led) >= 500) {
            t_led = now;
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        }

        Failsafe_KickWDT();
    }
}

void USBD_HID_OutCallback(uint8_t report_id, uint8_t *buf, uint16_t len)
{
    FFB_ProcessReport(report_id, buf, len);
    Failsafe_KickUSB();
}

void CDC_ReceiveCallback(uint8_t *buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
        Serial_ProcessByte(buf[i]);
}

void Error_Handler(void) { while (1) {} }
