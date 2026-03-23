# DIY FFB Racing Wheel — Firmware & Config App
## STM32F401CCU6 + BTS7960 + B10K Pot

---

## What's In This Folder

```
FFBWheel/
├── Firmware/
│   └── Core/
│       ├── Inc/
│       │   ├── ffb_wheel.h       ← Shared defines and config struct
│       │   ├── motor.h           ← BTS7960 PWM driver header
│       │   ├── adc_input.h       ← Potentiometer ADC header
│       │   ├── ffb_engine.h      ← FFB effect engine header
│       │   ├── config.h          ← Flash storage header
│       │   ├── serial_cmd.h      ← Serial config protocol header
│       │   └── usb_hid_desc.h    ← USB HID report types header
│       └── Src/
│           ├── main.c            ← Main application loop
│           ├── motor.c           ← BTS7960 TIM1 PWM driver
│           ├── adc_input.c       ← 6-channel ADC with DMA
│           ├── ffb_engine.c      ← Spring, Damper, Friction, Constant Force
│           ├── config.c          ← Save/load settings from internal flash
│           ├── serial_cmd.c      ← JSON serial protocol for web app
│           └── usb_hid_desc.c    ← Complete USB HID FFB descriptor bytes
└── WebApp/
    └── index.html                ← Open in Chrome to configure everything
```

---

## Hardware Connections

### BTS7960 Motor Driver
```
BTS7960 Pin    →    Connect To
M+             →    775 Motor terminal 1
M-             →    775 Motor terminal 2
B+             →    12V (from PSU yellow wire, 15A fuse)
B-             →    GND (common)
RPWM           →    PA8  (STM32)
LPWM           →    PA9  (STM32)
R_EN           →    3.3V (tie HIGH permanently)
L_EN           →    3.3V (tie HIGH permanently)
VCC            →    5V   (from PSU red wire)
GND            →    GND  (common)
```

### Potentiometers (all the same wiring)
```
Left pin   →    3.3V
Middle pin →    ADC pin (see table below)
Right pin  →    GND

PA0  =  Steering pot
PA1  =  Throttle pot
PA2  =  Brake pot
PA3  =  Clutch pot
PA4  =  Shifter X pot
PA5  =  Shifter Y pot
```

### Buttons (optional, 8 buttons)
```
PB0–PB7  =  Buttons 1–8
Each button: one pin to PBx, other pin to GND
(firmware uses internal pull-up, no resistors needed)
```

### USB
```
PA11 = USB D-  (built-in, just use the USB port on Black Pill)
PA12 = USB D+
```

### IMPORTANT: PA11 and PA12 are USB — do NOT use them for anything else!

---

## Step 1 — Create STM32CubeIDE Project

1. Open STM32CubeIDE
2. File → New → STM32 Project
3. In the board selector, type **STM32F401CCU6** → select it → Next
4. Name the project **FFBWheel** → Finish

---

## Step 2 — Configure the .ioc File

In the .ioc (pinout) view, configure the following:

### USB (most important)
- Left panel → Connectivity → USB_OTG_FS
- Mode: **Device Only**
- Left panel → Middleware → USB_DEVICE
- Class: **Human Interface Device Class (HID)**

### Enable Virtual COM Port (for config app)
- You need a **Composite HID + CDC** USB device
- In USB_DEVICE, change class to **Custom HID** (we use our own descriptor)
- Add CDC class separately — see note below*

### ADC1
- Enable ADC1
- Enable channels: IN0, IN1, IN2, IN3, IN4, IN5
- Mode: Independent mode, Continuous Conversion
- Enable DMA: DMA2 Stream0, Circular mode

### TIM1
- Enable TIM1
- Channel 1: **PWM Generation CH1** (PA8)
- Channel 2: **PWM Generation CH2** (PA9)
- Prescaler: 3
- Counter Period (ARR): 999

### Clock
- Set HCLK to **84 MHz**
- USB requires 48 MHz on PLL48CLK — CubeMX will configure this automatically

### Generate Code
- Project → Generate Code

---

## Step 3 — Copy Source Files Into Project

After CubeMX generates the project:

1. Copy all files from `Firmware/Core/Inc/` into your project's `Core/Inc/` folder
2. Copy all files from `Firmware/Core/Src/` into your project's `Core/Src/` folder
3. In STM32CubeIDE, right-click Core/Inc → Refresh, and Core/Src → Refresh

---

## Step 4 — Hook Into USB Callbacks

Find these generated files and add the callback hookups:

### In `USB_DEVICE/App/usbd_hid_if.c`:
Find the `CUSTOM_HID_OutEvent_FS` function and add:
```c
extern void USBD_HID_OutCallback(uint8_t report_id, uint8_t *buf, uint16_t len);

static int8_t CUSTOM_HID_OutEvent_FS(uint8_t event_idx, uint8_t state)
{
    USBD_CUSTOM_HID_HandleTypeDef *hhid =
        (USBD_CUSTOM_HID_HandleTypeDef*)hUsbDeviceFS.pClassData;

    if (hhid != NULL)
    {
        uint8_t *buf = hhid->Report_buf;
        uint8_t  report_id = buf[0];           // first byte = report ID
        USBD_HID_OutCallback(report_id, buf, CUSTOM_HID_EPOUT_SIZE);
    }

    return USBD_OK;
}
```

### In `USB_DEVICE/App/usbd_cdc_if.c`:
Find the `CDC_Receive_FS` function and add:
```c
extern void CDC_ReceiveCallback(uint8_t *buf, uint32_t len);

static int8_t CDC_Receive_FS(uint8_t* Buf, uint32_t *Len)
{
    CDC_ReceiveCallback(Buf, *Len);
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, &Buf[0]);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    return USBD_OK;
}
```

### In `USB_DEVICE/App/usbd_desc.c`:
Replace the HID report descriptor with ours:
```c
#include "usb_hid_desc.h"
// Change the descriptor reference to use HID_ReportDescriptor
// and HID_ReportDescriptorSize from our usb_hid_desc.c
```

---

## Step 5 — Build and Flash

1. Press the **Build** button (hammer icon) in STM32CubeIDE
2. Make sure there are no errors (warnings are OK)
3. To flash: hold BOOT0 button on Black Pill while plugging USB
4. Run → Debug (or Run → Run) to flash via ST-Link
   OR use STM32CubeProgrammer with DFU mode

After flashing, the wheel should appear in Windows as:
- A **Joystick / Gamepad** in Device Manager (HID)
- A **COM port** in Device Manager (CDC)

---

## Step 6 — Use the Config App

1. Open `WebApp/index.html` in **Google Chrome**
2. Click **Connect** button
3. Select the COM port of your wheel
4. Go through each tab to calibrate

### First-time calibration order:
1. **Wheel Setup tab** → Set Rotation Range (900), Pulley Ratio (4.5), Pot Sweep (200)
2. **Wheel Setup tab** → Turn wheel full left → "Set Left Lock", center → "Set Center", full right → "Set Right Lock"
3. **Pedals tab** → For each pedal: release it → "Set Released", press fully → "Set Full Press"
4. **Shifter tab** → Move to each gear position → click matching button
5. **FFB Tuning tab** → Start with Spring 65%, Strength 80%, try in game
6. Click **Save to Flash** — done forever

---

## Default Pin Summary

| Pin  | Function            |
|------|---------------------|
| PA0  | Steering pot ADC    |
| PA1  | Throttle ADC        |
| PA2  | Brake ADC           |
| PA3  | Clutch ADC          |
| PA4  | Shifter X ADC       |
| PA5  | Shifter Y ADC       |
| PA8  | BTS7960 RPWM        |
| PA9  | BTS7960 LPWM        |
| PA11 | USB D- (reserved)   |
| PA12 | USB D+ (reserved)   |
| PB0–PB7 | Buttons 1–8    |

---

## Troubleshooting

**Wheel not appearing in Windows:**
- Check USB cable is data cable (not charge-only)
- Make sure Black Pill is not still in DFU mode after flashing

**FFB not working in game:**
- Open Windows Game Controllers (joy.cpl) — test axes first
- Enable FFB in game settings (usually under "steering wheel" device settings)
- Try DirectX Diagnostic Tool (dxdiag) to verify FFB device is listed

**Steering direction inverted:**
- Use Invert toggle in Wheel Setup tab
- OR swap the two motor wires on BTS7960 M+/M-

**Motor runs at power-on (without touching wheel):**
- Re-do center calibration — pot center ADC value is off

**Web app won't connect:**
- Must use Google Chrome (Firefox does not support Web Serial API)
- Allow serial port permission when Chrome asks

---

## Safety Reminders

- Always start with Max Torque Limit at 50% until you verify everything works
- Add a physical E-STOP button between PSU PS_ON and GND
- The 775 motor is powerful — always test with the wheel in hand at low strength first
- Heatsink on BTS7960 if running sessions longer than 30 minutes

---

*Note on Composite HID+CDC: If CubeMX doesn't support composite HID+CDC directly,
search for "STM32 USB Composite HID CDC" on GitHub — there are several ready-made
templates that work with STM32F401. The Middleware layer just needs both class
handlers registered. This is the trickiest part of the setup — the Discord community
at discord.gg/openffboard can help if you get stuck here.
