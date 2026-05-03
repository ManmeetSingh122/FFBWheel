# FFBWheel — DIY Force Feedback Steering Wheel

Open-source firmware for a direct-drive style FFB steering wheel built on the **STM32F411 Black Pill**.  
Appears in Windows as a **DirectInput FFB joystick** (HID) + **COM port** (CDC) for configuration.

> **Status:** Working. Tested on STM32F411CEU6 (Black Pill) with BTS7960 motor driver and 775 DC motor.

---

## Features

- Full DirectInput PID force feedback (Spring, Damper, Friction, Inertia, Constant Force, Sine, Square, Triangle)
- Up to 8 simultaneous FFB effects
- Steering + Throttle + Brake + Clutch axes (12-bit ADC, EMA filtered + hysteresis)
- H-pattern shifter (6 gears + Reverse via pushdown switch)
- 8 digital buttons
- Soft lock (configurable rotation range, 180°–1800°)
- Three independent failsafes: IWDG watchdog, USB timeout, stuck motor detection
- All settings saved to flash — survives power cycles
- Web app config tool (Chrome only, Web Serial API)

---

## Hardware

### What You Need

| Part | Notes |
|---|---|
| STM32F411 Black Pill | WeAct or clone, 25 MHz HSE crystal required |
| BTS7960 motor driver | 43A H-bridge module |
| 775 DC motor | 12V, any RPM — geared down via belt/pulley |
| B10K potentiometer | For steering (and pedals/shifter if used) |
| 12V PSU | ATX PSU works well — use yellow (12V) and black (GND) |
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
| PA11 | USB D− *(reserved — do not use)* |
| PA12 | USB D+ *(reserved — do not use)* |
| PB0–PB7 | Buttons 1–8 (active LOW, internal pull-up) |
| PB8 | Reverse gear switch (active LOW, internal pull-up) |
| PC13 | Onboard LED (heartbeat blink = firmware running) |

### BTS7960 Wiring

| BTS7960 Pin | Connect To |
|---|---|
| M+ | Motor terminal 1 |
| M− | Motor terminal 2 |
| B+ | 12V (PSU yellow, 15A fuse recommended) |
| B− | GND (common) |
| RPWM | PA8 (STM32) |
| LPWM | PA9 (STM32) |
| R_EN | 3.3V (tie HIGH permanently) |
| L_EN | 3.3V (tie HIGH permanently) |
| VCC | 5V (PSU red) |
| GND | GND (common) |

### Potentiometer Wiring (all pots same)

```
Left pin  → 3.3V
Middle pin → ADC pin (PA0–PA5)
Right pin  → GND
```

> **Tip:** Add a 100nF ceramic capacitor from each pot wiper pin to GND, as close to the STM32 as possible. This significantly reduces ADC noise.

### Buttons

Connect one side of each button to PBx, other side to GND. No resistors needed — firmware uses internal pull-up.

### Reverse Gear Switch

Connect a momentary pushbutton (or microswitch under shifter knob) between PB8 and GND.  
Push knob down + move to 1st gear position = Reverse.

---

## Flashing

1. Plug STM32 to PC via USB
2. Hold **BOOT0**, press and release **RESET**, then release **BOOT0** — board enters DFU mode
3. Open **STM32CubeProgrammer**, select **USB** from dropdown, click refresh — `USB1` appears
4. Click **Open File** → select `FFBWheel.elf` (from [Releases](../../releases/latest))
5. Click **Download**, wait for completion
6. Click **Disconnect**, press **RESET**

After flashing the blue LED blinks and the device appears in Windows as:
- `STM32 Custom Human interface` in joy.cpl
- A HID device in Device Manager
- A COM port (COMx) in Device Manager

---

## Configuration App

Open `webapp.html` in **Google Chrome** (Firefox does not support Web Serial API).  
Download `webapp.html` from [Releases](../../releases/latest) — do not open directly from the source tree.

### First-Time Calibration Order

1. **Wheel Setup tab** — set Rotation Range (e.g. 900°), Pulley Ratio, Pot Sweep
2. **Wheel Setup tab** — turn wheel full left → *Set Left Lock*, center → *Set Center*, full right → *Set Right Lock*
3. **Pedals tab** — for each pedal: fully release → *Set Released*, fully press → *Set Full Press*
4. **Shifter tab** — move to each gear position → click matching button
5. **FFB Tuning tab** — start with Spring 65%, Strength 80%, test in game
6. Click **Save to Flash** — settings survive power cycles

---

## Serial Commands (Advanced)

The firmware exposes a JSON protocol over the CDC COM port (115200 baud, any terminal or the web app).

### Read Commands

| Command | Response |
|---|---|
| `GET_CONFIG` | Full config JSON with all current settings |
| `GET_LIVE` | Live ADC values, wheel angle, gear, FFB state |
| `GET_STATUS` | Failsafe state, reason, FFB enabled, uptime (ms) |

### Calibration Commands

| Command | Action |
|---|---|
| `CALIBRATE_CENTER` | Save current steering ADC as center |
| `CALIBRATE_MIN` | Save current steering ADC as left lock |
| `CALIBRATE_MAX` | Save current steering ADC as right lock |
| `CAL_THR_MIN` | Save current throttle ADC as released |
| `CAL_THR_MAX` | Save current throttle ADC as full press |
| `CAL_BRAKE_MIN` | Save current brake ADC as released |
| `CAL_BRAKE_MAX` | Save current brake ADC as full press |
| `CAL_CLUTCH_MIN` | Save current clutch ADC as released |
| `CAL_CLUTCH_MAX` | Save current clutch ADC as full press |
| `CAL_SHFT_LEFT` | Save current shifter X as left gate |
| `CAL_SHFT_RIGHT` | Save current shifter X as right gate |
| `CAL_SHFT_FWD` | Save current shifter Y as forward gate |
| `CAL_SHFT_REV` | Save current shifter Y as reverse gate |

### Set Commands

Format: `SET <key> <value>`

| Key | Type | Description |
|---|---|---|
| `range` | int | Wheel rotation range in degrees (180–1800) |
| `ratio` | float | Pulley ratio (wheel degrees / pot degrees) |
| `pot_deg` | float | Physical pot sweep in degrees |
| `invert` | 0/1 | Invert steering direction |
| `strength` | 0–100 | Global FFB strength % |
| `spring` | 0–100 | Spring effect gain % |
| `damper` | 0–100 | Damper effect gain % |
| `friction` | 0–100 | Friction effect gain % |
| `inertia` | 0–100 | Inertia effect gain % |
| `max_torque` | 0–100 | Motor torque ceiling % (safety limit) |
| `deadzone` | int | Center deadzone in degrees |
| `thr_curve` | 0–2 | Throttle curve: 0=linear, 1=square, 2=cubic |
| `brake_curve` | 0–2 | Brake curve |
| `clutch_curve` | 0–2 | Clutch curve |

### Other Commands

| Command | Action |
|---|---|
| `SAVE` | Write current config to flash |
| `RESET` | Load factory defaults (does not save automatically) |
| `CLEAR_FAILSAFE` | Clear a triggered failsafe without hardware reset |

### Example Session

```
→ GET_CONFIG
← {"type":"config","range":900,"ratio":4.50,...}

→ SET strength 75
← {"type":"ok"}

→ SAVE
← {"type":"ok","msg":"Saved"}

→ GET_STATUS
← {"type":"status","failsafe":0,"reason":"none","ffb_en":1,"uptime":12453}
```

---

## Failsafe System

Three independent safety mechanisms protect against motor runaway:

| Failsafe | Trigger | Action |
|---|---|---|
| **IWDG Watchdog** | Firmware freeze for >4 seconds | MCU reset, motor stops |
| **USB Timeout** | No FFB data from game for >200ms | Motor stops |
| **Stuck Motor** | >80% torque for >2s with <2° movement | Motor stops |

After a failsafe triggers, send `CLEAR_FAILSAFE` via serial or press the board's RESET button.

---

## Troubleshooting

**Wheel not appearing in Windows**
- Use a data USB cable (charge-only cables have no data lines)
- Make sure the board is not still in DFU mode — press RESET after flashing

**FFB not working in game**
- Verify axes move in joy.cpl first
- Enable FFB in game settings under the steering wheel device
- Some games need the wheel set as "primary controller"

**Steering direction inverted**
- Use the Invert toggle in the config app, or swap M+/M− on the BTS7960

**Motor runs at power-on without input**
- Re-do center calibration — the pot center ADC value is off

**Values fluctuating in live data**
- Normal for unconnected (floating) ADC pins — connect pots or ignore
- Add 100nF caps from each pot wiper to GND to reduce noise

**Web app won't connect**
- Must use Google Chrome — Firefox does not support Web Serial API
- Click Allow when Chrome asks for serial port permission

---

## Safety

- **Start with Max Torque at 50%** until you verify everything works correctly
- **Add a physical E-STOP** — wire a normally-closed button between PSU PS_ON and GND
- **The 775 motor is powerful** — always test at low strength with the wheel in hand first
- **Heatsink the BTS7960** if running sessions longer than 30 minutes
- **15A fuse** on the 12V motor supply line

---

## Building from Source

Open in **STM32CubeIDE**. The project is pre-configured for STM32F411CEU6.

Build: `Project → Build All` (or Ctrl+B)  
Flash: Use the `.elf` from `Debug/` folder with STM32CubeProgrammer as described above.

---

## License

MIT License — see [LICENSE](LICENSE)

---

## Contributing

Issues and PRs welcome. If you build one, share a photo!
