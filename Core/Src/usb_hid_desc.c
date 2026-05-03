#include "usb_hid_desc.h"

/*
 * USB HID Report Descriptor for a Force Feedback Steering Wheel.
 *
 * Follows USB HID Usage Tables v1.12 for:
 *   - Generic Desktop (0x01) — axes and buttons
 *   - Physical Interface Device / PID (0x0F) — DirectInput FFB effects
 *
 * This descriptor makes Windows recognize the device as a steering wheel
 * with full DirectInput force feedback support (Spring, Damper, Friction,
 * Constant Force, Inertia, and periodic effects).
 *
 * Do NOT modify this descriptor unless you know exactly what you are doing.
 * Any mistake here breaks FFB recognition in Windows entirely.
 */

const uint8_t HID_ReportDescriptor[] = {

    /* ── INPUT REPORT (ID 0x01): Axes + Buttons ─────────────────────────── */
    0x05, 0x01,         /* Usage Page (Generic Desktop)                      */
    0x09, 0x04,         /* Usage (Joystick)                                  */
    0xA1, 0x01,         /* Collection (Application)                          */
    0x85, 0x01,         /*   Report ID (1)                                   */

    /* Steering axis (X) */
    0x09, 0x30,         /*   Usage (X)                                       */
    0x16, 0x01, 0x80,   /*   Logical Minimum (-32767)                        */
    0x26, 0xFF, 0x7F,   /*   Logical Maximum (32767)                         */
    0x75, 0x10,         /*   Report Size (16)                                */
    0x95, 0x01,         /*   Report Count (1)                                */
    0x81, 0x02,         /*   Input (Data, Var, Abs)                          */

    /* Throttle axis (Y) — 0 to 32767 (unidirectional: 0=released) */
    0x09, 0x31,         /*   Usage (Y)                                       */
    0x15, 0x00,         /*   Logical Minimum (0)                             */
    0x26, 0xFF, 0x7F,   /*   Logical Maximum (32767)                         */
    0x75, 0x10,
    0x95, 0x01,
    0x81, 0x02,

    /* Brake axis (Z) — 0 to 32767 (unidirectional: 0=released) */
    0x09, 0x32,         /*   Usage (Z)                                       */
    0x15, 0x00,         /*   Logical Minimum (0)                             */
    0x26, 0xFF, 0x7F,   /*   Logical Maximum (32767)                         */
    0x75, 0x10,
    0x95, 0x01,
    0x81, 0x02,

    /* Clutch axis (Rx) — 0 to 32767 (unidirectional: 0=released) */
    0x09, 0x33,         /*   Usage (Rx)                                      */
    0x15, 0x00,         /*   Logical Minimum (0)                             */
    0x26, 0xFF, 0x7F,   /*   Logical Maximum (32767)                         */
    0x75, 0x10,
    0x95, 0x01,
    0x81, 0x02,

    /* 8 buttons */
    0x05, 0x09,         /*   Usage Page (Button)                             */
    0x19, 0x01,         /*   Usage Minimum (Button 1)                        */
    0x29, 0x08,         /*   Usage Maximum (Button 8)                        */
    0x15, 0x00,         /*   Logical Minimum (0)                             */
    0x25, 0x01,         /*   Logical Maximum (1)                             */
    0x75, 0x01,         /*   Report Size (1)                                 */
    0x95, 0x08,         /*   Report Count (8)                                */
    0x81, 0x02,         /*   Input (Data, Var, Abs)                          */

    /* Gear (1 byte) */
    0x05, 0x01,         /*   Usage Page (Generic Desktop)                    */
    0x09, 0x39,         /*   Usage (Hat switch) — repurposed for gear        */
    0x15, 0x00,         /*   Logical Minimum (0)                             */
    0x25, 0x07,         /*   Logical Maximum (7)                             */
    0x75, 0x08,         /*   Report Size (8)                                 */
    0x95, 0x01,         /*   Report Count (1)                                */
    0x81, 0x42,         /*   Input (Data, Var, Abs, Null)                    */

    /* ── PID (Physical Interface Device) Collections ─────────────────────── */
    0x05, 0x0F,         /*   Usage Page (PID)                                */
    0x09, 0x92,         /*   Usage (PID State Report)                        */
    0xA1, 0x02,         /*   Collection (Logical)                            */
    0x85, 0x0C,         /*     Report ID (12) — PID State                    */
    0x09, 0x9F,         /*     Usage (Device Paused)                         */
    0x09, 0xA0,         /*     Usage (Actuators Enabled)                     */
    0x09, 0xA4,         /*     Usage (Safety Switch)                         */
    0x09, 0xA5,         /*     Usage (Actuator Override Switch)              */
    0x09, 0xA6,         /*     Usage (Actuator Power)                        */
    0x15, 0x00,
    0x25, 0x01,
    0x35, 0x00,
    0x45, 0x01,
    0x75, 0x01,
    0x95, 0x05,
    0x81, 0x02,
    0x95, 0x03,         /*     3 padding bits                                */
    0x81, 0x03,
    0x09, 0x94,         /*     Usage (Effect Playing)                        */
    0x15, 0x00,
    0x25, 0x01,
    0x75, 0x01,
    0x95, 0x01,
    0x81, 0x02,
    0x09, 0x22,         /*     Usage (Effect Block Index)                    */
    0x15, 0x01,
    0x25, 0x08,         /*     Max 8 effects                                 */
    0x75, 0x07,
    0x95, 0x01,
    0x81, 0x02,
    0xC0,               /*   End Collection (PID State)                      */

    /* ── Create New Effect (Feature) ────────────────────────────────────── */
    0x09, 0xAB,         /*   Usage (Create New Effect)                       */
    0xA1, 0x02,
    0x85, 0x0A,         /*     Report ID (10)                                */
    0x09, 0x25,         /*     Usage (Effect Type)                           */
    0xA1, 0x02,
    0x09, 0x26,         /*       Usage (ET Spring)                           */
    0x09, 0x27,         /*       Usage (ET Damper)                           */
    0x09, 0x28,         /*       Usage (ET Inertia)                          */
    0x09, 0x29,         /*       Usage (ET Friction)                         */
    0x09, 0x24,         /*       Usage (ET Constant Force)                   */
    0x09, 0x2F,         /*       Usage (ET Square)                           */
    0x09, 0x30,         /*       Usage (ET Sine)                             */
    0x09, 0x31,         /*       Usage (ET Triangle)                         */
    0x15, 0x01,
    0x25, 0x08,
    0x75, 0x08,
    0x95, 0x01,
    0xB1, 0x00,         /*       Feature (Data, Array, Abs)                  */
    0xC0,
    0x09, 0xAC,         /*     Usage (Byte Count)                            */
    0x17, 0x00, 0x00, 0x00, 0x00,
    0x27, 0xFF, 0xFF, 0x00, 0x00,
    0x75, 0x10,
    0x95, 0x01,
    0xB1, 0x02,
    0xC0,

    /* ── Block Load (IN Feature) ─────────────────────────────────────────── */
    0x09, 0x89,         /*   Usage (Block Load Report)                       */
    0xA1, 0x02,
    0x85, 0x0B,         /*     Report ID (11)                                */
    0x09, 0x22,         /*     Usage (Effect Block Index)                    */
    0x15, 0x01,
    0x25, 0x08,
    0x75, 0x08,
    0x95, 0x01,
    0xB1, 0x02,
    0x09, 0x8B,         /*     Usage (Block Load Status)                     */
    0x15, 0x01,
    0x25, 0x03,
    0x75, 0x08,
    0x95, 0x01,
    0xB1, 0x02,
    0x09, 0xAC,         /*     Usage (Byte Count / RAM Pool Available)       */
    0x17, 0x00, 0x00, 0x00, 0x00,
    0x27, 0xFF, 0xFF, 0x00, 0x00,
    0x75, 0x10,
    0x95, 0x01,
    0xB1, 0x02,
    0xC0,

    /* ── Set Effect Output Report ────────────────────────────────────────── */
    0x09, 0x21,         /*   Usage (Set Effect Report)                       */
    0xA1, 0x02,
    0x85, 0x02,         /*     Report ID (2)                                 */
    0x09, 0x22,         /*     Usage (Effect Block Index)                    */
    0x15, 0x01,
    0x25, 0x08,
    0x75, 0x08,
    0x95, 0x01,
    0x91, 0x02,
    0x09, 0x25,         /*     Usage (Effect Type)                           */
    0x15, 0x01,
    0x25, 0x08,
    0x75, 0x08,
    0x95, 0x01,
    0x91, 0x00,
    0x09, 0x50,         /*     Usage (Duration)                              */
    0x09, 0x54,         /*     Usage (Trigger Repeat Interval)               */
    0x09, 0x51,         /*     Usage (Sample Period)                         */
    0x15, 0x00,
    0x27, 0xFF, 0xFF, 0x00, 0x00,
    0x75, 0x10,
    0x95, 0x03,
    0x91, 0x02,
    0x09, 0x52,         /*     Usage (Gain)                                  */
    0x09, 0x53,         /*     Usage (Trigger Button)                        */
    0x15, 0x00,
    0x25, 0xFF,
    0x75, 0x08,
    0x95, 0x02,
    0x91, 0x02,
    0x09, 0x55,         /*     Usage (Axes Enable)                           */
    0x09, 0x56,         /*     Usage (Direction Enable)                      */
    0x15, 0x00,
    0x25, 0x01,
    0x75, 0x01,
    0x95, 0x02,
    0x91, 0x02,
    0x95, 0x06,         /*     6 padding bits                                */
    0x91, 0x03,
    0x09, 0x57,         /*     Usage (Direction)                             */
    0xA1, 0x02,
    0x0B, 0x01, 0x00, 0x0A, 0x00,
    0x0B, 0x02, 0x00, 0x0A, 0x00,
    0x66, 0x14, 0x00,
    0x55, 0xFE,
    0x15, 0x00,
    0x27, 0xA0, 0x8C, 0x00, 0x00,
    0x75, 0x10,
    0x95, 0x02,
    0x91, 0x02,
    0x55, 0x00,
    0x66, 0x00, 0x00,
    0xC0,
    0xC0,

    /* ── Set Envelope Output Report ──────────────────────────────────────── */
    0x09, 0x5A,         /*   Usage (Set Envelope Report)                     */
    0xA1, 0x02,
    0x85, 0x03,         /*     Report ID (3)                                 */
    0x09, 0x22,
    0x15, 0x01, 0x25, 0x08, 0x75, 0x08, 0x95, 0x01, 0x91, 0x02,
    0x09, 0x5B,         /*     Usage (Attack Level)                          */
    0x09, 0x5D,         /*     Usage (Fade Level)                            */
    0x16, 0x00, 0x00,
    0x26, 0x10, 0x27,   /*     Max 10000                                     */
    0x75, 0x10, 0x95, 0x02, 0x91, 0x02,
    0x09, 0x5C,         /*     Usage (Attack Time)                           */
    0x09, 0x5E,         /*     Usage (Fade Time)                             */
    0x27, 0xFF, 0xFF, 0x00, 0x00,
    0x75, 0x10, 0x95, 0x02, 0x91, 0x02,
    0xC0,

    /* ── Set Condition Output Report ─────────────────────────────────────── */
    0x09, 0x5F,         /*   Usage (Set Condition Report)                    */
    0xA1, 0x02,
    0x85, 0x04,         /*     Report ID (4)                                 */
    0x09, 0x22, 0x15, 0x01, 0x25, 0x08, 0x75, 0x08, 0x95, 0x01, 0x91, 0x02,
    0x09, 0x23,         /*     Usage (Parameter Block Offset)                */
    0x15, 0x00, 0x25, 0x01, 0x75, 0x04, 0x95, 0x01, 0x91, 0x02,
    0x09, 0x58,         /*     Usage (Type Specific Block Offset)            */
    0x15, 0x00, 0x25, 0x01, 0x75, 0x04, 0x95, 0x01, 0x91, 0x02,
    0x09, 0x60,         /*     Usage (CP Offset)                             */
    0x16, 0xF0, 0xD8,
    0x26, 0x10, 0x27,
    0x75, 0x10, 0x95, 0x01, 0x91, 0x02,
    0x09, 0x61,         /*     Usage (Positive Coefficient)                  */
    0x09, 0x62,         /*     Usage (Negative Coefficient)                  */
    0x15, 0x00,
    0x26, 0x10, 0x27,
    0x75, 0x10, 0x95, 0x02, 0x91, 0x02,
    0x09, 0x63,         /*     Usage (Positive Saturation)                   */
    0x09, 0x64,         /*     Usage (Negative Saturation)                   */
    0x15, 0x00,
    0x27, 0xFF, 0xFF, 0x00, 0x00,
    0x75, 0x10, 0x95, 0x02, 0x91, 0x02,
    0x09, 0x65,         /*     Usage (Dead Band)                             */
    0x15, 0x00,
    0x27, 0xFF, 0xFF, 0x00, 0x00,
    0x75, 0x10, 0x95, 0x01, 0x91, 0x02,
    0xC0,

    /* ── Set Periodic Output Report ──────────────────────────────────────── */
    0x09, 0x6E,         /*   Usage (Set Periodic Report)                     */
    0xA1, 0x02,
    0x85, 0x05,         /*     Report ID (5)                                 */
    0x09, 0x22, 0x15, 0x01, 0x25, 0x08, 0x75, 0x08, 0x95, 0x01, 0x91, 0x02,
    0x09, 0x70,         /*     Usage (Magnitude)                             */
    0x16, 0xF0, 0xD8,
    0x26, 0x10, 0x27,
    0x75, 0x10, 0x95, 0x01, 0x91, 0x02,
    0x09, 0x6F,         /*     Usage (Offset)                                */
    0x16, 0xF0, 0xD8,
    0x26, 0x10, 0x27,
    0x75, 0x10, 0x95, 0x01, 0x91, 0x02,
    0x09, 0x71,         /*     Usage (Phase)                                 */
    0x15, 0x00,
    0x27, 0xA0, 0x8C, 0x00, 0x00,
    0x75, 0x10, 0x95, 0x01, 0x91, 0x02,
    0x09, 0x72,         /*     Usage (Period)                                */
    0x15, 0x00,
    0x27, 0xFF, 0xFF, 0x00, 0x00,
    0x75, 0x10, 0x95, 0x01, 0x91, 0x02,
    0xC0,

    /* ── Set Constant Force Output Report ────────────────────────────────── */
    0x09, 0x73,         /*   Usage (Set Constant Force Report)               */
    0xA1, 0x02,
    0x85, 0x06,         /*     Report ID (6)                                 */
    0x09, 0x22, 0x15, 0x01, 0x25, 0x08, 0x75, 0x08, 0x95, 0x01, 0x91, 0x02,
    0x09, 0x74,         /*     Usage (Magnitude)                             */
    0x16, 0xF0, 0xD8,
    0x26, 0x10, 0x27,
    0x75, 0x10, 0x95, 0x01, 0x91, 0x02,
    0xC0,

    /* ── Effect Operation Output Report ──────────────────────────────────── */
    0x09, 0x77,         /*   Usage (Effect Operation Report)                 */
    0xA1, 0x02,
    0x85, 0x07,         /*     Report ID (7)                                 */
    0x09, 0x22, 0x15, 0x01, 0x25, 0x08, 0x75, 0x08, 0x95, 0x01, 0x91, 0x02,
    0x09, 0x78,         /*     Usage (Op Effect Start)                       */
    0x09, 0x79,         /*     Usage (Op Effect Start Solo)                  */
    0x09, 0x7A,         /*     Usage (Op Effect Stop)                        */
    0x15, 0x01, 0x25, 0x03, 0x75, 0x08, 0x95, 0x01, 0x91, 0x00,
    0x09, 0x7C,         /*     Usage (Loop Count)                            */
    0x15, 0x00, 0x27, 0xFF, 0xFF, 0x00, 0x00,
    0x75, 0x10, 0x95, 0x01, 0x91, 0x02,
    0xC0,

    /* ── Device Control Output Report ───────────────────────────────────── */
    0x09, 0x96,         /*   Usage (PID Device Control)                      */
    0xA1, 0x02,
    0x85, 0x08,         /*     Report ID (8)                                 */
    0x09, 0x97,         /*     Usage (DC Enable Actuators)                   */
    0x09, 0x98,         /*     Usage (DC Disable Actuators)                  */
    0x09, 0x99,         /*     Usage (DC Stop All Effects)                   */
    0x09, 0x9A,         /*     Usage (DC Device Reset)                       */
    0x09, 0x9B,         /*     Usage (DC Device Pause)                       */
    0x09, 0x9C,         /*     Usage (DC Device Continue)                    */
    0x15, 0x01, 0x25, 0x06, 0x75, 0x08, 0x95, 0x01, 0x91, 0x00,
    0xC0,

    /* ── Device Gain Output Report ───────────────────────────────────────── */
    0x09, 0x7D,         /*   Usage (Device Gain Report)                      */
    0xA1, 0x02,
    0x85, 0x09,         /*     Report ID (9)                                 */
    0x09, 0x7E,         /*     Usage (Device Gain)                           */
    0x15, 0x00, 0x25, 0xFF, 0x75, 0x08, 0x95, 0x01, 0x91, 0x02,
    0xC0,

    0xC0,               /* End Collection (Application)                      */
};

const uint16_t HID_ReportDescriptorSize = sizeof(HID_ReportDescriptor);
