# FFBWheel — DIY Force Feedback Steering Wheel

Open-source firmware for a direct-drive FFB steering wheel built on the **STM32F411 Black Pill**.  
Appears in Windows as **FFB Steering Wheel** (DirectInput HID + CDC COM port for configuration).

> **Status:** Working. Tested on STM32F411CEU6 (Black Pill) with BTS7960 motor driver and 775 DC motor.

---

## What's New in V3

- **STM FFB Utility** — standalone Windows desktop app (replaces web app)
- **Encoder support** — quadrature encoder on PB4/PB5 (TIM3), any PPR
- **Active bumpstop** — return force at rotation limits
- **Always-on center spring** — independent of game FFB
- **Output torque smoothing** — reduces motor noise
- **5-point custom pedal curves** — interactive curve editor with presets
- **Configurable ADC filter** — 4 levels (off/light/medium/heavy)
- **FFB profiles** — save, load, export, import named profiles
- **Firmware v2.5** with config struct v2.5

---

## Features

**Firmware**
- Full DirectInput PID force feedback — Spring, Damper, Friction, Inertia, Constant Force, Sine, Square, Triangle
- Up to 8 simultaneous FFB effects with validated mixing
- Steering + Throttle + Brake + Clutch axes (12-bit ADC, two-stage EMA + hysteresis filter)
- H-pattern shifter (6 gears + Reverse via pushdown switch on PB8)
- 8 digital buttons
- Soft lock with cosine ramp (configurable 180°–1800°)
- Active bumpstop force at rotation limits
- Always-on center spring (independent of game FFB)
- Output torque smoothing
- Quadrature encoder support (TIM3, PB4/PB5, configurable PPR)
- Three independent failsafes: IWDG watchdog (4s), USB timeout (200ms), stuck motor detection
- All settings saved to flash — survives power cycles
- Firmware v2.5

**STM FFB Utility App**
- Standalone Windows desktop app (no browser needed)
- Live dashboard with rotating steering wheel image, pedal bars, telemetry chart
- Wheel Setup — rotation range, pulley ratio, pot sweep, encoder PPR, calibration
- FFB Tuning — all effect gains, bumpstop, center spring, torque smoothing, filter
- Pedals — per-pedal calibration, 5-point interactive curve editor, 6 presets
- Shifter — gate calibration with live position visualizer
- Profiles — save/load/export/import named profiles, 5 built-in presets
- Diagnostic — serial log, live telemetry chart, failsafe status and clear
- Auto-detects firmware version on connect

---

## Hardware

### What You Need

| Part | Notes |
|---|---|
| STM32F411 Black Pill | WeAct or clone, 25 MHz HSE crystal required |
| BTS7960 motor driver | 43A H-bridge module |
| 775 DC motor | 12V, geared down via belt/pulley |
| B10K potentiometer | For steering (and pedals/shifter if used) |
| Quadrature encoder | Optional — any PPR, connects to PB4/PB5 |
| 12V PSU | ATX PSU works well |
| USB cable | Data cable, not charge-only |

### Pin Mapping

| Pin | Function |
|---|---|
| PA0 | Steering pot (ADC) |
| PA1 | Throttle pot (ADC) |
| PA2 | Brake pot (ADC) |
| PA3 | Clutch pot (ADC) |
| PA4 | Shifter X pot (ADC) |
| PA5 | Shifter Y pot (ADC) |
| PA8 | BTS7960 RPWM (TIM1 CH1) |
| PA9 | BTS7960 LPWM (TIM1 CH2) |
| PA11 | USB D− *(reserved)* |
| PA12 | USB D+ *(reserved)* |
| PB0–PB7 | Buttons 1–8 (active LOW, internal pull-up) |
| PB8 | Reverse gear switch (active LOW, internal pull-up) |
| PB4 | Encoder A — TIM3 CH1 *(encoder mode only)* |
| PB5 | Encoder B — TIM3 CH2 *(encoder mode only)* |
| PC13 | Onboard LED (heartbeat blink = firmware running) |

### BTS7960 Wiring

| BTS7960 Pin | Connect To |
|---|---|
| M+ | Motor terminal 1 |
| M− | Motor terminal 2 |
| B+ | 12V (PSU yellow, 15A fuse recommended) |
| B− | GND (common) |
| RPWM | PA8 |
| LPWM | PA9 |
| R_EN | 3.3V (tie HIGH permanently) |
| L_EN | 3.3V (tie HIGH permanently) |
| VCC | 5V |
| GND | GND (common) |

### Potentiometer Wiring

```
Left pin   → 3.3V
Middle pin → ADC pin (PA0–PA5)
Right pin  → GND
```

> **Tip:** Add a 100nF ceramic capacitor from each pot wiper to GND to reduce ADC noise.

### Encoder Wiring (optional)

```
Encoder A → PB4
Encoder B → PB5
VCC       → 3.3V or 5V (check encoder spec)
GND       → GND
```

Switch between pot and encoder in the app: Wheel Setup → Input Source.

---

## Flashing

1. Plug STM32 to PC via USB
2. Hold **BOOT0**, press and release **RESET**, then release **BOOT0** — board enters DFU mode
3. Open **STM32CubeProgrammer** → select **USB** → click Refresh → `USB1` appears
4. Open File → select `FFBWheel.elf` from [Releases](../../releases/latest)
5. Click Download → wait → Disconnect → press RESET

After flashing the LED blinks and the device appears as **FFB Steering Wheel** in Device Manager and joy.cpl.

---

## STM FFB Utility App

Download `STM FFB Utility Setup.exe` from [Releases](../../releases/latest) and install it.

### First-Time Calibration

1. **Wheel Setup** — set Rotation Range, Pulley Ratio, Pot Sweep (or switch to Encoder and set PPR)
2. **Wheel Setup** — turn full left → *Set Left Lock*, center → *Set Center*, full right → *Set Right Lock*
3. **Pedals** — for each pedal: release → *Set Released*, full press → *Set Full Press*
4. **Shifter** — move to each gate position → click matching button
5. **FFB Tuning** — start with Spring 65%, Strength 80%
6. Click **Save to Flash**

---

## Serial Commands

The firmware uses a JSON protocol over CDC (115200 baud). The app handles this automatically, but you can also use any serial terminal.

### Commands

| Command | Description |
|---|---|
| `GET_CONFIG` | Returns full config JSON |
| `GET_LIVE` | Returns live sensor values |
| `GET_STATUS` | Returns failsafe state, uptime |
| `SAVE` | Write config to flash |
| `RESET` | Load factory defaults |
| `CLEAR_FAILSAFE` | Clear triggered failsafe |
| `CALIBRATE_CENTER` | Save current steering position as center |
| `CALIBRATE_MIN` | Save current steering position as left lock |
| `CALIBRATE_MAX` | Save current steering position as right lock |
| `CAL_THR_MIN/MAX` | Throttle calibration |
| `CAL_BRAKE_MIN/MAX` | Brake calibration |
| `CAL_CLUTCH_MIN/MAX` | Clutch calibration |
| `CAL_SHFT_LEFT/RIGHT/FWD/REV` | Shifter gate calibration |

### SET Parameters

`SET <key> <value>`

| Key | Description |
|---|---|
| `range` | Rotation range 180–1800° |
| `ratio` | Pulley ratio |
| `pot_deg` | Pot sweep degrees |
| `invert` | Invert steering 0/1 |
| `input_mode` | 0=pot, 1=encoder |
| `encoder_ppr` | Encoder pulses per revolution |
| `strength` | Global FFB strength 0–100 |
| `spring` / `damper` / `friction` / `inertia` | Effect gains 0–100 |
| `max_torque` | Safety torque ceiling 0–100 |
| `filter` | ADC filter 0=off 1=light 2=medium 3=heavy |
| `bumpstop` | Bumpstop strength 0–100 |
| `center_spring` | Always-on center spring 0–100 |
| `torque_smooth` | Output smoothing 0–100 |
| `deadzone` | Center deadzone degrees |
| `thr_curve` / `brake_curve` / `clutch_curve` | 0=custom 1=linear 2=square 3=cubic |
| `thr_pts` / `brake_pts` / `clutch_pts` | Custom curve: `0,25,50,75,100` |

---

## Failsafe System

| Failsafe | Trigger | Action |
|---|---|---|
| IWDG Watchdog | Firmware freeze >4s | MCU reset, motor stops |
| USB Timeout | No FFB data >200ms | Motor stops |
| Stuck Motor | >80% torque, <2° movement, >2s | Motor stops |

---

## Troubleshooting

**Wheel not appearing in Windows** — Use a data USB cable. Press RESET after flashing.

**FFB not working** — Check joy.cpl axes first. Enable FFB in game settings.

**Steering inverted** — Use Invert toggle in app, or swap M+/M− on BTS7960.

**Motor runs at startup** — Redo center calibration.

**Values fluctuating** — Normal for unconnected pins. Add 100nF caps to reduce noise.

---

## Safety

- Start with **Max Torque at 50%** until everything is verified
- Add a **physical E-STOP** between PSU PS_ON and GND
- Always test at low strength with the wheel in hand first
- **Heatsink the BTS7960** for sessions longer than 30 minutes
- Use a **15A fuse** on the 12V motor supply

---

## Building from Source

Open in **STM32CubeIDE** (pre-configured for STM32F411CEU6).  
Build: `Project → Build All`  
Flash: Use `Debug/FFBWheel.elf` with STM32CubeProgrammer.

---

## License

MIT License — see [LICENSE](LICENSE)

---

## Contributing

Issues and PRs welcome. Share a photo if you build one!
