#ifndef SERIAL_CMD_H
#define SERIAL_CMD_H

#include <stdint.h>

/* Called from USB interrupt — only buffers one byte, never transmits */
void Serial_ProcessByte(uint8_t byte);

/* Called from main loop — transmits queued data and processes commands */
void Serial_Task(void);

/* Called from main loop — sends live ADC data to web app */
void Serial_SendLive(void);

/* Called from main loop — queues a string for CDC transmission */
void Serial_Send(const char *str);

#endif /* SERIAL_CMD_H */
