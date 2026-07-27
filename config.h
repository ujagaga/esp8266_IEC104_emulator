#ifndef CONFIG_H
#define CONFIG_H

// After this period since startup, try to connect as wifi client. This is
// to make sure it is not too frequent after a reset, so the router lease is
// updated.
#define AP_MODE_TIMEOUT_S 1

#define LED_PIN     2
#define LED_PIN_1   12
#define LED_PIN_2   13
#define LED_PIN_3   14
#define FREQ_PIN    5

#define AP_NAME_PREFIX "IEC104_" // Will be appended by device MAC

#define IEC104_PORT 2404
#define IEC104_COMMON_ADDR 1     // Common address of ASDU
#define IEC104_IOA_MEASURED 1001 // IOA of the LED state (M_SP_NA_1)
#define IEC104_IOA_CONTROL 2001  // IOA of the LED command (C_SC_NA_1)
#define IEC104_IOA_FREQUENCY 1002 // IOA of the mains frequency (M_ME_NC_1)

#define WIFI_PASS_EEPROM_ADDR (0)
#define WIFI_PASS_SIZE (32)
#define SSID_EEPROM_ADDR (WIFI_PASS_EEPROM_ADDR + WIFI_PASS_SIZE)
#define SSID_SIZE (32)
#define EEPROM_SIZE (WIFI_PASS_SIZE + SSID_SIZE)

#endif
