#ifndef SERIAL_CMD_H
#define SERIAL_CMD_H

#include <stdint.h>

/* Call from USB CDC receive callback */
void Serial_ProcessByte(uint8_t byte);

/* Send a null-terminated string over USB CDC */
void Serial_Send(const char *str);

/* Called from main loop ~10x per second to send live data */
void Serial_SendLive(void);

#endif
