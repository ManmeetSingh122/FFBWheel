/* usb_device.c
 * Replace the CubeMX generated usb_device.c with this file.
 * Only change: registers USBD_COMPOSITE instead of USBD_CUSTOM_HID.
 */
#include "usb_device.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_composite.h"

USBD_HandleTypeDef hUsbDeviceFS;

void MX_USB_DEVICE_Init(void)
{
    USBD_Init(&hUsbDeviceFS, &FS_Desc, DEVICE_FS);
    USBD_RegisterClass(&hUsbDeviceFS, &USBD_COMPOSITE);
    USBD_Start(&hUsbDeviceFS);
}
