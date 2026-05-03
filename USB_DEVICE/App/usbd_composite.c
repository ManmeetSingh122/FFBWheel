/*
 * usbd_composite.c
 * Composite USB: Custom HID (FFB Wheel) + CDC (VCP config port)
 */

#include "usbd_composite.h"
#include "usbd_ctlreq.h"
#include "usb_hid_desc.h"
#include "ffb_engine.h"
#include "serial_cmd.h"
#include <string.h>
#include "usbd_cdc.h"

static uint8_t USBD_COMPOSITE_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_COMPOSITE_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_COMPOSITE_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
static uint8_t USBD_COMPOSITE_EP0_RxReady(USBD_HandleTypeDef *pdev);
static uint8_t USBD_COMPOSITE_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t USBD_COMPOSITE_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t *USBD_COMPOSITE_GetFSCfgDesc(uint16_t *length);
static uint8_t *USBD_COMPOSITE_GetDeviceQualifierDesc(uint16_t *length);

USBD_ClassTypeDef USBD_COMPOSITE = {
    USBD_COMPOSITE_Init,   USBD_COMPOSITE_DeInit,
    USBD_COMPOSITE_Setup,  NULL,
    USBD_COMPOSITE_EP0_RxReady,
    USBD_COMPOSITE_DataIn, USBD_COMPOSITE_DataOut,
    NULL, NULL, NULL, NULL,
    USBD_COMPOSITE_GetFSCfgDesc,
    NULL,
    USBD_COMPOSITE_GetDeviceQualifierDesc,
};

/* ── Buffers ─────────────────────────────────────────────────────────────── */
static uint8_t s_hid_out_buf[HID_OUT_EP_SIZE];
static uint8_t s_cdc_out_buf[CDC_DATA_EP_SIZE];
static uint8_t s_cdc_cmd_buf[8];

#define CDC_TX_BUF_SIZE  512
static uint8_t s_cdc_tx_buf[CDC_TX_BUF_SIZE];

static USBD_CDC_LineCodingTypeDef s_line_coding = { 115200, 0, 0, 8 };

static volatile uint8_t s_hid_tx_busy = 0;
static volatile uint8_t s_cdc_tx_busy = 0;
static volatile uint32_t s_cdc_dtr_tick = 0;

/* DTR flag — set by SET_CONTROL_LINE_STATE when host app opens port.
   When DTR=0 the host is not reading EP2, so we must NOT transmit —
   the FIFO fills up, DataIn never fires, and USB crashes after ~3s. */
static volatile uint8_t s_cdc_dtr = 0;

/* ── Configuration descriptor ───────────────────────────────────────────── */
static uint8_t USBD_COMPOSITE_CfgDesc[COMPOSITE_CFGDESC_SIZE] = {
    0x09, 0x02,
    LOBYTE(COMPOSITE_CFGDESC_SIZE), HIBYTE(COMPOSITE_CFGDESC_SIZE),
    0x03, 0x01, 0x00, 0xC0, 0x32,
    0x08, 0x0B, 0x00, 0x02, 0x02, 0x02, 0x01, 0x00,
    0x09, 0x04, CDC_CMD_IF_NUM, 0x00, 0x01, 0x02, 0x02, 0x01, 0x00,
    0x05, 0x24, 0x00, 0x10, 0x01,
    0x05, 0x24, 0x01, 0x00, CDC_DATA_IF_NUM,
    0x04, 0x24, 0x02, 0x02,
    0x05, 0x24, 0x06, CDC_CMD_IF_NUM, CDC_DATA_IF_NUM,
    0x07, 0x05, CDC_CMD_EP, 0x03, CDC_CMD_EP_SIZE, 0x00, CDC_CMD_BINTERVAL,
    0x09, 0x04, CDC_DATA_IF_NUM, 0x00, 0x02, 0x0A, 0x00, 0x00, 0x00,
    0x07, 0x05, CDC_IN_EP,  0x02, CDC_DATA_EP_SIZE, 0x00, 0x00,
    0x07, 0x05, CDC_OUT_EP, 0x02, CDC_DATA_EP_SIZE, 0x00, 0x00,
    0x09, 0x04, HID_IF_NUM, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00,
    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22,
    LOBYTE(HID_REPORT_DESC_SIZE), HIBYTE(HID_REPORT_DESC_SIZE),  /* wDescriptorLength */
    0x07, 0x05, HID_IN_EP,  0x03, HID_IN_EP_SIZE,  0x00, HID_FS_BINTERVAL,
    0x07, 0x05, HID_OUT_EP, 0x03, HID_OUT_EP_SIZE, 0x00, HID_FS_BINTERVAL,
};

static uint8_t USBD_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC] = {
    USB_LEN_DEV_QUALIFIER_DESC, USB_DESC_TYPE_DEVICE_QUALIFIER,
    0x00, 0x02, 0xEF, 0x02, 0x01, 0x40, 0x01, 0x00,
};

static uint8_t USBD_COMPOSITE_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
    (void)cfgidx;
    USBD_LL_OpenEP(pdev, HID_IN_EP,  USBD_EP_TYPE_INTR, HID_IN_EP_SIZE);
    USBD_LL_OpenEP(pdev, HID_OUT_EP, USBD_EP_TYPE_INTR, HID_OUT_EP_SIZE);
    pdev->ep_in[HID_IN_EP   & 0x0F].is_used = 1;
    pdev->ep_out[HID_OUT_EP & 0x0F].is_used = 1;

    USBD_LL_OpenEP(pdev, CDC_CMD_EP, USBD_EP_TYPE_INTR, CDC_CMD_EP_SIZE);
    USBD_LL_OpenEP(pdev, CDC_IN_EP,  USBD_EP_TYPE_BULK, CDC_DATA_EP_SIZE);
    USBD_LL_OpenEP(pdev, CDC_OUT_EP, USBD_EP_TYPE_BULK, CDC_DATA_EP_SIZE);
    pdev->ep_in[CDC_CMD_EP & 0x0F].is_used = 1;
    pdev->ep_in[CDC_IN_EP  & 0x0F].is_used = 1;
    pdev->ep_out[CDC_OUT_EP & 0x0F].is_used = 1;

    USBD_LL_PrepareReceive(pdev, HID_OUT_EP, s_hid_out_buf, HID_OUT_EP_SIZE);
    USBD_LL_PrepareReceive(pdev, CDC_OUT_EP, s_cdc_out_buf, CDC_DATA_EP_SIZE);

    s_hid_tx_busy = 0;
    s_cdc_tx_busy = 0;
    s_cdc_dtr     = 0;   /* port not open yet */
    return USBD_OK;
}

static uint8_t USBD_COMPOSITE_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
    (void)cfgidx;
    USBD_LL_CloseEP(pdev, HID_IN_EP);  USBD_LL_CloseEP(pdev, HID_OUT_EP);
    USBD_LL_CloseEP(pdev, CDC_CMD_EP); USBD_LL_CloseEP(pdev, CDC_IN_EP);
    USBD_LL_CloseEP(pdev, CDC_OUT_EP);
    s_hid_tx_busy = 0;
    s_cdc_tx_busy = 0;
    s_cdc_dtr     = 0;
    return USBD_OK;
}

static uint8_t USBD_COMPOSITE_Setup(USBD_HandleTypeDef *pdev,
                                    USBD_SetupReqTypedef *req)
{
    uint8_t ifnum = LOBYTE(req->wIndex);

    if (ifnum == HID_IF_NUM) {
        switch (req->bmRequest & USB_REQ_TYPE_MASK) {
        case USB_REQ_TYPE_CLASS:
            switch (req->bRequest) {
            case 0x09: USBD_CtlPrepareRx(pdev, s_hid_out_buf, MIN(req->wLength, HID_OUT_EP_SIZE)); break;
            case 0x0A: break;
            default:   USBD_CtlError(pdev, req); return USBD_FAIL;
            }
            break;
        case USB_REQ_TYPE_STANDARD:
            if (req->bRequest == USB_REQ_GET_DESCRIPTOR) {
                if ((req->wValue >> 8) == 0x22) {
                    USBD_CtlSendData(pdev, (uint8_t *)HID_ReportDescriptor,
                                     MIN(HID_REPORT_DESC_SIZE, req->wLength));
                } else if ((req->wValue >> 8) == 0x21) {
                    USBD_CtlSendData(pdev, USBD_COMPOSITE_CfgDesc + 91, MIN(9, req->wLength));
                } else {
                    USBD_CtlError(pdev, req); return USBD_FAIL;
                }
            }
            break;
        }
        return USBD_OK;
    }

    if (ifnum == CDC_CMD_IF_NUM || ifnum == CDC_DATA_IF_NUM) {
        if ((req->bmRequest & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_CLASS) {
            switch (req->bRequest) {
            case 0x20: /* SET_LINE_CODING */
                USBD_CtlPrepareRx(pdev, s_cdc_cmd_buf,
                                  MIN(req->wLength, sizeof(s_line_coding)));
                break;
            case 0x21: /* GET_LINE_CODING */
                USBD_CtlSendData(pdev, (uint8_t *)&s_line_coding,
                                 MIN(req->wLength, 7));
                break;
            case 0x22: /* SET_CONTROL_LINE_STATE
                          wValue bit 0 = DTR: 1 = host app opened port
                                               0 = host app closed port   */
                s_cdc_dtr = (req->wValue & 0x01) ? 1 : 0;
                if (s_cdc_dtr) {
                	s_cdc_dtr_tick = HAL_GetTick();
                } else {
                    /* Port closed — clear any stuck TX state */
                    s_cdc_tx_busy = 0;
                }
                break;
            default:
                /* Handle unknown CDC class requests with data stages.
                   PuTTY sends SEND_ENCAPSULATED_COMMAND (0x00) with wLength > 0.
                   Without receiving/discarding the data, EP0 stalls and USB crashes. */
                if (req->wLength > 0) {
                    if (req->bmRequest & 0x80U) {
                        /* Device-to-host: send a zero-length packet */
                        USBD_CtlSendData(pdev, NULL, 0);
                    } else {
                        /* Host-to-device: receive data into dummy buffer and discard */
                        static uint8_t dummy[64];
                        USBD_CtlPrepareRx(pdev, dummy, MIN(req->wLength, 64U));
                    }
                }
                break;
            }
        }
        return USBD_OK;
    }

    return USBD_OK;
}

static uint8_t USBD_COMPOSITE_EP0_RxReady(USBD_HandleTypeDef *pdev)
{
    (void)pdev;
    if (s_cdc_cmd_buf[0] || s_cdc_cmd_buf[1]) {
        memcpy(&s_line_coding, s_cdc_cmd_buf, sizeof(s_line_coding));
        memset(s_cdc_cmd_buf, 0, sizeof(s_cdc_cmd_buf));
    }
    return USBD_OK;
}

static uint8_t USBD_COMPOSITE_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
    (void)pdev;
    if (epnum == (HID_IN_EP & 0x0F)) s_hid_tx_busy = 0;
    if (epnum == (CDC_IN_EP & 0x0F)) s_cdc_tx_busy = 0;
    return USBD_OK;
}

extern void USBD_HID_OutCallback(uint8_t report_id, uint8_t *buf, uint16_t len);
extern void CDC_ReceiveCallback(uint8_t *buf, uint32_t len);

static uint8_t USBD_COMPOSITE_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
    uint32_t rx_len;

    if (epnum == (HID_OUT_EP & 0x0F)) {
        rx_len = USBD_LL_GetRxDataSize(pdev, epnum);
        if (rx_len > 0)
            USBD_HID_OutCallback(s_hid_out_buf[0], s_hid_out_buf, (uint16_t)rx_len);
        USBD_LL_PrepareReceive(pdev, HID_OUT_EP, s_hid_out_buf, HID_OUT_EP_SIZE);
    }

    if (epnum == (CDC_OUT_EP & 0x0F)) {
        rx_len = USBD_LL_GetRxDataSize(pdev, epnum);
        if (rx_len > 0)
            CDC_ReceiveCallback(s_cdc_out_buf, rx_len);
        USBD_LL_PrepareReceive(pdev, CDC_OUT_EP, s_cdc_out_buf, CDC_DATA_EP_SIZE);
    }

    return USBD_OK;
}

static uint8_t *USBD_COMPOSITE_GetFSCfgDesc(uint16_t *length)
{ *length = COMPOSITE_CFGDESC_SIZE; return USBD_COMPOSITE_CfgDesc; }

static uint8_t *USBD_COMPOSITE_GetDeviceQualifierDesc(uint16_t *length)
{ *length = sizeof(USBD_DeviceQualifierDesc); return USBD_DeviceQualifierDesc; }

/* ══ PUBLIC API ══════════════════════════════════════════════════════════════ */
extern USBD_HandleTypeDef hUsbDeviceFS;

/* Returns 1 if a host application has the COM port open (DTR set).
   Main loop should call this before Serial_SendLive() to avoid
   filling the TX FIFO when nobody is reading it.                          */
uint8_t CDC_IsConnected(void)
{
    if (!s_cdc_dtr) return 0;
    if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED) return 0;
    /* Wait 100ms after DTR goes high before sending — lets PuTTY/Chrome
       complete their initialization before we start transmitting.        */
    return ((uint32_t)(HAL_GetTick() - s_cdc_dtr_tick) >= 100) ? 1 : 0;
}

/* Send CDC data. Only transmits when DTR=1 (port is open).
   Prevents FIFO overflow when no app has the port open.                   */
uint8_t CDC_Transmit_FS(uint8_t *Buf, uint16_t Len)
{
    if (!s_cdc_dtr) return USBD_FAIL;          /* port not open — don't fill FIFO */
    if (s_cdc_tx_busy) return USBD_BUSY;
    if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED) return USBD_FAIL;
    if (Len == 0) return USBD_OK;
    if (Len > CDC_TX_BUF_SIZE) Len = CDC_TX_BUF_SIZE;

    memcpy(s_cdc_tx_buf, Buf, Len);
    s_cdc_tx_busy = 1;
    USBD_LL_Transmit(&hUsbDeviceFS, CDC_IN_EP, s_cdc_tx_buf, Len);
    return USBD_OK;
}

uint8_t HID_SendReport_FS(uint8_t *report, uint16_t len)
{
    if (s_hid_tx_busy) return USBD_BUSY;
    if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED) return USBD_FAIL;
    s_hid_tx_busy = 1;
    USBD_LL_Transmit(&hUsbDeviceFS, HID_IN_EP, report, len);
    return USBD_OK;
}
