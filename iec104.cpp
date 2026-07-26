/*
 *  Author: Rada Berar
 *  email: ujagaga@gmail.com
 *
 *  IEC 60870-5-104 slave core: single point (the LED) as
 *  M_SP_NA_1 monitoring data, controlled via C_SC_NA_1, plus
 *  general interrogation (C_IC_NA_1). One TCP master at a time.
 */
#include <ESP8266WiFi.h>
#include <string.h>
#include "config.h"
#include "pinctrl.h"
#include "iec104.h"

/* ASDU type identifiers used. */
#define TYPE_M_SP_NA_1 1
#define TYPE_M_ME_NC_1 13
#define TYPE_C_SC_NA_1 45
#define TYPE_C_IC_NA_1 100

/* Simulated voltage measurement. */
#define VOLTAGE_NOMINAL 220.0f
#define VOLTAGE_INTERVAL_MS 2000

/* Causes of transmission used. */
#define COT_SPONTANEOUS 3
#define COT_ACTIVATION_CON 7
#define COT_ACTIVATION_TERM 10
#define COT_INTERROGATED_BY_STATION 20

/* U-format function octets. */
#define U_STARTDT_ACT 0x07
#define U_STARTDT_CON 0x0B
#define U_STOPDT_ACT 0x13
#define U_STOPDT_CON 0x23
#define U_TESTFR_ACT 0x43
#define U_TESTFR_CON 0x83

static WiFiServer server(IEC104_PORT);
static WiFiClient client;
static bool started = false; /* true once STARTDT act received */
static uint16_t ssn = 0;      /* V(S): our send sequence number */
static uint16_t rsn = 0;      /* V(R): our receive sequence number */

static enum { WAIT_START, WAIT_LEN, WAIT_BODY } rxState = WAIT_START;
static uint8_t rxLenExpected = 0;
static uint8_t rxBody[253];
static uint8_t rxBodyIdx = 0;

static float currentVoltage = VOLTAGE_NOMINAL;
static unsigned long lastVoltageMs = 0;
static bool voltageEnabled = false;

void IEC104_setVoltageEnabled(bool en) { voltageEnabled = en; }

bool IEC104_isVoltageEnabled(void) { return voltageEnabled; }

float IEC104_getVoltage(void) { return currentVoltage; }

static void sendUFormat(uint8_t function) {
  uint8_t frame[6] = {0x68, 4, function, 0, 0, 0};
  client.write(frame, sizeof(frame));
}

static void sendIFormat(const uint8_t *asdu, uint8_t asduLen) {
  uint8_t frame[6 + 14];
  frame[0] = 0x68;
  frame[1] = 4 + asduLen;
  frame[2] = (uint8_t)((ssn << 1) & 0xFF);
  frame[3] = (uint8_t)((ssn >> 7) & 0xFF);
  frame[4] = (uint8_t)((rsn << 1) & 0xFF);
  frame[5] = (uint8_t)((rsn >> 7) & 0xFF);
  memcpy(&frame[6], asdu, asduLen);
  client.write(frame, 6 + asduLen);
  ssn = (ssn + 1) & 0x7FFF;
}

/* Builds an M_SP_NA_1 (single point information) ASDU, returns its length. */
static uint8_t buildMSpAsdu(uint8_t *buf, uint8_t cot, bool value) {
  buf[0] = TYPE_M_SP_NA_1;
  buf[1] = 0x01; /* VSQ: 1 object, not sequential */
  buf[2] = cot;
  buf[3] = 0; /* originator address */
  buf[4] = (uint8_t)(IEC104_COMMON_ADDR & 0xFF);
  buf[5] = (uint8_t)((IEC104_COMMON_ADDR >> 8) & 0xFF);
  buf[6] = (uint8_t)(IEC104_IOA_MEASURED & 0xFF);
  buf[7] = (uint8_t)((IEC104_IOA_MEASURED >> 8) & 0xFF);
  buf[8] = (uint8_t)((IEC104_IOA_MEASURED >> 16) & 0xFF);
  buf[9] = value ? 0x01 : 0x00; /* SIQ, quality bits all 0 */
  return 10;
}

/* Builds an M_ME_NC_1 (short floating point measured value) ASDU, returns its length. */
static uint8_t buildVoltageAsdu(uint8_t *buf, uint8_t cot, float value) {
  buf[0] = TYPE_M_ME_NC_1;
  buf[1] = 0x01; /* VSQ: 1 object, not sequential */
  buf[2] = cot;
  buf[3] = 0; /* originator address */
  buf[4] = (uint8_t)(IEC104_COMMON_ADDR & 0xFF);
  buf[5] = (uint8_t)((IEC104_COMMON_ADDR >> 8) & 0xFF);
  buf[6] = (uint8_t)(IEC104_IOA_VOLTAGE & 0xFF);
  buf[7] = (uint8_t)((IEC104_IOA_VOLTAGE >> 8) & 0xFF);
  buf[8] = (uint8_t)((IEC104_IOA_VOLTAGE >> 16) & 0xFF);
  memcpy(&buf[9], &value, 4); /* IEEE 754 float, little-endian (native on ESP8266) */
  buf[13] = 0; /* QDS, quality good */
  return 14;
}

static void sendVoltageReport(uint8_t cot) {
  uint8_t asdu[14];
  uint8_t len = buildVoltageAsdu(asdu, cot, currentVoltage);
  sendIFormat(asdu, len);
}

void IEC104_reportLedState(bool on) {
  if (client && client.connected() && started) {
    uint8_t asdu[10];
    uint8_t len = buildMSpAsdu(asdu, COT_SPONTANEOUS, on);
    sendIFormat(asdu, len);
  }
}

static void handleAsdu(const uint8_t *asdu, uint8_t len) {
  if (len < 10) {
    return;
  }

  uint8_t type = asdu[0];
  uint32_t ioa = asdu[6] | ((uint32_t)asdu[7] << 8) | ((uint32_t)asdu[8] << 16);

  if (type == TYPE_C_SC_NA_1) {
    if (ioa != IEC104_IOA_CONTROL) {
      return;
    }
    uint8_t sco = asdu[9];
    bool value = sco & 0x01;
    PINCTRL_setLed(value);

    uint8_t resp[10];
    memcpy(resp, asdu, 10);
    resp[2] = COT_ACTIVATION_CON;
    sendIFormat(resp, 10);
  } else if (type == TYPE_C_IC_NA_1) {
    uint8_t resp[10];
    memcpy(resp, asdu, 10);
    resp[2] = COT_ACTIVATION_CON;
    sendIFormat(resp, 10);

    uint8_t info[10];
    uint8_t infoLen = buildMSpAsdu(info, COT_INTERROGATED_BY_STATION, PINCTRL_getLed());
    sendIFormat(info, infoLen);

    if (voltageEnabled) {
      sendVoltageReport(COT_INTERROGATED_BY_STATION);
    }

    resp[2] = COT_ACTIVATION_TERM;
    sendIFormat(resp, 10);
  }
}

static void handleFrame(const uint8_t *ctrl, const uint8_t *asdu, uint8_t asduLen) {
  if ((ctrl[0] & 0x01) == 0) {
    /* I-format */
    uint16_t recvSsn = (ctrl[0] >> 1) | ((uint16_t)ctrl[1] << 7);
    rsn = (recvSsn + 1) & 0x7FFF;
    handleAsdu(asdu, asduLen);
  } else if ((ctrl[0] & 0x03) == 0x01) {
    /* S-format: pure acknowledgement, nothing to act on. */
  } else {
    /* U-format */
    if (ctrl[0] == U_STARTDT_ACT) {
      started = true;
      ssn = 0;
      rsn = 0;
      sendUFormat(U_STARTDT_CON);
    } else if (ctrl[0] == U_STOPDT_ACT) {
      started = false;
      sendUFormat(U_STOPDT_CON);
    } else if (ctrl[0] == U_TESTFR_ACT) {
      sendUFormat(U_TESTFR_CON);
    }
  }
}

void IEC104_init(void) {
  server.begin();
  randomSeed(millis());
}

void IEC104_process(void) {
  if (voltageEnabled && (millis() - lastVoltageMs) >= VOLTAGE_INTERVAL_MS) {
    lastVoltageMs = millis();
    currentVoltage = VOLTAGE_NOMINAL * (0.95f + random(0, 1001) / 10000.0f);
    if (client && client.connected() && started) {
      sendVoltageReport(COT_SPONTANEOUS);
    }
  }

  if (!client || !client.connected()) {
    WiFiClient newClient = server.available();
    if (newClient) {
      if (client) {
        client.stop();
      }
      client = newClient;
      started = false;
      ssn = 0;
      rsn = 0;
      rxState = WAIT_START;
      rxBodyIdx = 0;
    }
    return;
  }

  while (client.available()) {
    uint8_t b = client.read();
    switch (rxState) {
      case WAIT_START:
        if (b == 0x68) {
          rxState = WAIT_LEN;
        }
        break;
      case WAIT_LEN:
        rxLenExpected = b;
        if (rxLenExpected < 4) {
          rxState = WAIT_START;
          break;
        }
        rxBodyIdx = 0;
        rxState = WAIT_BODY;
        break;
      case WAIT_BODY:
        rxBody[rxBodyIdx++] = b;
        if (rxBodyIdx >= rxLenExpected) {
          handleFrame(&rxBody[0], &rxBody[4], rxLenExpected - 4);
          rxState = WAIT_START;
        }
        break;
    }
  }
}
