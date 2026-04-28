/* usbd_conf.c
 *
 * Fix 1 (HAL_PCD_MspInit): USB OTG priority 5, below TIM2 (priority 0).
 *   TIM2 kicks the IWDG watchdog every 1ms. If USB had higher priority it
 *   could starve TIM2 during enumeration bursts and trigger a WDT reset.
 *
 * Fix 2 (USBD_LL_Delay): NOP cycle loop instead of HAL_Delay.
 *   HAL_Delay spins on HAL_GetTick() which needs SysTick to increment.
 *   SysTick cannot preempt the USB ISR → deadlock → USB disconnect.
 *
 * Fix 3 (FIFO): Corrected FIFO allocation to fit within the 320-word
 *   hardware limit of the STM32F411 USB OTG FS peripheral.
 *   Previous values totalled 448 words, overflowing the FIFO RAM and
 *   corrupting the HID endpoint as soon as CDC data started flowing.
 */

#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"
#include "usbd_def.h"
#include "usbd_core.h"
#include "usbd_customhid.h"

PCD_HandleTypeDef hpcd_USB_OTG_FS;
void Error_Handler(void);
void SystemClock_Config(void);
USBD_StatusTypeDef USBD_Get_USB_Status(HAL_StatusTypeDef hal_status);

void HAL_PCD_MspInit(PCD_HandleTypeDef* pcdHandle)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if (pcdHandle->Instance == USB_OTG_FS)
    {
        __HAL_RCC_GPIOA_CLK_ENABLE();
        GPIO_InitStruct.Pin       = GPIO_PIN_11 | GPIO_PIN_12;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF10_OTG_FS;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
        __HAL_RCC_USB_OTG_FS_CLK_ENABLE();
        HAL_NVIC_SetPriority(OTG_FS_IRQn, 5, 0);   /* FIX 1: was 0,0 */
        HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
    }
}

void HAL_PCD_MspDeInit(PCD_HandleTypeDef* pcdHandle)
{
    if (pcdHandle->Instance == USB_OTG_FS) {
        __HAL_RCC_USB_OTG_FS_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_11 | GPIO_PIN_12);
        HAL_NVIC_DisableIRQ(OTG_FS_IRQn);
    }
}

void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd)
{ USBD_LL_SetupStage((USBD_HandleTypeDef*)hpcd->pData, (uint8_t *)hpcd->Setup); }

void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{ USBD_LL_DataOutStage((USBD_HandleTypeDef*)hpcd->pData, epnum, hpcd->OUT_ep[epnum].xfer_buff); }

void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{ USBD_LL_DataInStage((USBD_HandleTypeDef*)hpcd->pData, epnum, hpcd->IN_ep[epnum].xfer_buff); }

void HAL_PCD_SOFCallback(PCD_HandleTypeDef *hpcd)
{ USBD_LL_SOF((USBD_HandleTypeDef*)hpcd->pData); }

void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd)
{
    if (hpcd->Init.speed != PCD_SPEED_FULL) Error_Handler();
    USBD_LL_SetSpeed((USBD_HandleTypeDef*)hpcd->pData, USBD_SPEED_FULL);
    USBD_LL_Reset((USBD_HandleTypeDef*)hpcd->pData);
}

void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_LL_Suspend((USBD_HandleTypeDef*)hpcd->pData);
    __HAL_PCD_GATE_PHYCLOCK(hpcd);
    if (hpcd->Init.low_power_enable)
        SCB->SCR |= (uint32_t)(SCB_SCR_SLEEPDEEP_Msk | SCB_SCR_SLEEPONEXIT_Msk);
}

void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *hpcd)
{ USBD_LL_Resume((USBD_HandleTypeDef*)hpcd->pData); }

void HAL_PCD_ISOOUTIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{ USBD_LL_IsoOUTIncomplete((USBD_HandleTypeDef*)hpcd->pData, epnum); }

void HAL_PCD_ISOINIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{ USBD_LL_IsoINIncomplete((USBD_HandleTypeDef*)hpcd->pData, epnum); }

void HAL_PCD_ConnectCallback(PCD_HandleTypeDef *hpcd)
{ USBD_LL_DevConnected((USBD_HandleTypeDef*)hpcd->pData); }

void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *hpcd)
{ USBD_LL_DevDisconnected((USBD_HandleTypeDef*)hpcd->pData); }

USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef *pdev)
{
    hpcd_USB_OTG_FS.Instance               = USB_OTG_FS;
    hpcd_USB_OTG_FS.Init.dev_endpoints     = 4;
    hpcd_USB_OTG_FS.Init.speed             = PCD_SPEED_FULL;
    hpcd_USB_OTG_FS.Init.dma_enable        = DISABLE;
    hpcd_USB_OTG_FS.Init.phy_itface        = PCD_PHY_EMBEDDED;
    hpcd_USB_OTG_FS.Init.Sof_enable        = DISABLE;
    hpcd_USB_OTG_FS.Init.low_power_enable  = DISABLE;
    hpcd_USB_OTG_FS.Init.lpm_enable        = DISABLE;
    hpcd_USB_OTG_FS.Init.vbus_sensing_enable = DISABLE;
    hpcd_USB_OTG_FS.Init.use_dedicated_ep1 = DISABLE;
    if (HAL_PCD_Init(&hpcd_USB_OTG_FS) != HAL_OK) Error_Handler();

    /* FIFO sizes — STM32F411 USB OTG FS has 320 words (1280 bytes) total.
       All TX FIFOs + RX FIFO must fit within 320 words.
       RX FIFO is shared across all OUT endpoints.
       Each TX FIFO is per IN endpoint.

       Allocation (must total <= 320 words):
         RX        : 0x40 (64 words)  — shared, handles all OUT EPs + SETUP
         EP0 TX    : 0x20 (32 words)  — control responses (max 64 bytes = 16 words, 2x headroom)
         EP1 TX    : 0x40 (64 words)  — HID IN (64-byte interrupt packets)
         EP2 TX    : 0x40 (64 words)  — CDC IN bulk data
         EP3 TX    : 0x10 (16 words)  — CDC notification (8-byte interrupt, rarely used)
       Total: 64+32+64+64+16 = 240 words — well within 320 word limit.

       Previous values (0x80+0x40+0x80+0x40+0x40 = 448 words) EXCEEDED the
       320-word hardware limit, corrupting the FIFO and causing USB disconnect
       as soon as CDC data started flowing (i.e. when the app opened the COM port). */
    HAL_PCDEx_SetRxFiFo(&hpcd_USB_OTG_FS,    0x40);   /* 64 words  RX shared  */
    HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 0, 0x20);   /* 32 words  EP0 ctrl   */
    HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 1, 0x40);   /* 64 words  EP1 HID    */
    HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 2, 0x40);   /* 64 words  EP2 CDC    */
    HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 3, 0x10);   /* 16 words  EP3 notif  */

    pdev->pData = &hpcd_USB_OTG_FS;
    hpcd_USB_OTG_FS.pData = pdev;
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef *pdev)
{ return USBD_Get_USB_Status(HAL_PCD_DeInit(pdev->pData)); }

USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef *pdev)
{ return USBD_Get_USB_Status(HAL_PCD_Start(pdev->pData)); }

USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef *pdev)
{ return USBD_Get_USB_Status(HAL_PCD_Stop(pdev->pData)); }

USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t ep_type, uint16_t ep_mps)
{ return USBD_Get_USB_Status(HAL_PCD_EP_Open(pdev->pData, ep_addr, ep_mps, ep_type)); }

USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{ return USBD_Get_USB_Status(HAL_PCD_EP_Close(pdev->pData, ep_addr)); }

USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{ return USBD_Get_USB_Status(HAL_PCD_EP_Flush(pdev->pData, ep_addr)); }

USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{ return USBD_Get_USB_Status(HAL_PCD_EP_SetStall(pdev->pData, ep_addr)); }

USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{ return USBD_Get_USB_Status(HAL_PCD_EP_ClrStall(pdev->pData, ep_addr)); }

uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    PCD_HandleTypeDef *hpcd = (PCD_HandleTypeDef*)pdev->pData;
    if ((ep_addr & 0x80) == 0x80) return hpcd->IN_ep[ep_addr & 0x7F].is_stall;
    else                          return hpcd->OUT_ep[ep_addr & 0x7F].is_stall;
}

USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef *pdev, uint8_t dev_addr)
{ return USBD_Get_USB_Status(HAL_PCD_SetAddress(pdev->pData, dev_addr)); }

USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t *pbuf, uint32_t size)
{ return USBD_Get_USB_Status(HAL_PCD_EP_Transmit(pdev->pData, ep_addr, pbuf, size)); }

USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t *pbuf, uint32_t size)
{ return USBD_Get_USB_Status(HAL_PCD_EP_Receive(pdev->pData, ep_addr, pbuf, size)); }

uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{ return HAL_PCD_EP_GetRxCount((PCD_HandleTypeDef*)pdev->pData, ep_addr); }

/* FIX 2: NOP loop instead of HAL_Delay.
   Called from inside USB ISR — HAL_Delay deadlocks because SysTick
   cannot preempt USB ISR to increment HAL_GetTick.                   */
void USBD_LL_Delay(uint32_t Delay)
{
    uint32_t cycles = Delay * 84000UL;   /* 84 MHz */
    while (cycles--) { __NOP(); }
}

void *USBD_static_malloc(uint32_t size)
{
    /* Static allocator — returns a single fixed buffer.
       Must be large enough for the largest class handle the USB stack
       will ever allocate. 512 bytes covers all ST middleware class handles.
       The old size (sizeof USBD_CUSTOM_HID_HandleTypeDef) was too small
       for composite use and could cause silent memory corruption.          */
    static uint32_t mem[512 / 4];
    (void)size;
    return mem;
}

void USBD_static_free(void *p) { (void)p; }

USBD_StatusTypeDef USBD_Get_USB_Status(HAL_StatusTypeDef hal_status)
{
    switch (hal_status) {
        case HAL_OK:   return USBD_OK;
        case HAL_BUSY: return USBD_BUSY;
        default:       return USBD_FAIL;
    }
}
