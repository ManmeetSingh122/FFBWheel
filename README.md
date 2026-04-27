# DIY FFB Racing Wheel — Firmware & Config App

## STM32F411 + BTS7960 + B10K Pot

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

## Flashing

1. To flash: plug STM32 board to laptop/PC via USB cable the press and hold BOOT0 button and while holding press RESET button and release BOOT0 button.
2. Open STM32CubeProgrammer software and select USB from dropdown menu and click refresh icon. USB1 should appear now.
3. Now click on open file and select FFBWheel.elf file.
4. Click on download and wait for it to complete and after completion click on disconnect in software and press RESET button of board.
   **Firmware is flashed now!**
   
After flashing, blue light on board start blinking and the wheel should appear in Windows as:
- A **USB Input Device** in Device Manager (HID)
- A **COM port** in Device Manager (CDC)
- A **STM32 Device** in Joy.cpl

---

## Configuring — Use the Config App

1. Open `WebApp/webapp_fixed.html` in **Google Chrome**
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
