#ifndef CONFIG_H
#define CONFIG_H

#include "ffb_wheel.h"

/* Load config from flash into g_cfg.
   If flash is blank or magic doesn't match, loads defaults.                 */
void Config_Load(void);

/* Save g_cfg to flash (erases sector 7 first). */
void Config_Save(void);

/* Reset g_cfg to factory defaults (does NOT save automatically). */
void Config_Defaults(void);

#endif
