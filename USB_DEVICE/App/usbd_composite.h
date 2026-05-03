#ifndef USBD_COMPOSITE_H
#define USBD_COMPOSITE_H

#include "usbd_def.h"
#include "usbd_ioreq.h"

/* ── Endpoint addresses ──────────────────────────────────────────────────── */
#define HID_IN_EP       0x81U
#define HID_OUT_EP      0x01U
#define HID_IN_EP_SIZE  64U
#define HID_OUT_EP_SIZE 64U
#define HID_FS_BINTERVAL 1U

#define CDC_CMD_EP          0x83U
#define CDC_IN_EP           0x82U
#define CDC_OUT_EP          0x02U
#define CDC_CMD_EP_SIZE     8U
#define CDC_DATA_EP_SIZE    64U
#define CDC_CMD_BINTERVAL   16U

/* ── Interface numbers ───────────────────────────────────────────────────── */
#define CDC_CMD_IF_NUM   0U
#define CDC_DATA_IF_NUM  1U
#define HID_IF_NUM       2U

/* ── Config descriptor size ──────────────────────────────────────────────── */
/* 9+8+9+5+5+4+5+7+9+7+7+9+9+7+7 = 116 bytes */
#define COMPOSITE_CFGDESC_SIZE  116U

/* ── Class object ────────────────────────────────────────────────────────── */
extern USBD_ClassTypeDef USBD_COMPOSITE;

/* ── Public functions ────────────────────────────────────────────────────── */
uint8_t HID_SendReport_FS(uint8_t *report, uint16_t len);
uint8_t CDC_Transmit_FS(uint8_t *Buf, uint16_t Len);

/* Returns 1 when a host app has the COM port open (DTR set).
   Always check this before calling Serial_SendLive() or any CDC TX.       */
uint8_t CDC_IsConnected(void);

#endif /* USBD_COMPOSITE_H */
