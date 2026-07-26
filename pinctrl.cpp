/*
 *  Author: Rada Berar
 *  email: ujagaga@gmail.com
 *
 *  GPIO management module
 */
#include "wifi_connection.h"
#include "config.h"
#include "iec104.h"


static bool ledState = false;

void PINCTRL_init(){
  pinMode(LED_PIN, OUTPUT);
  pinMode(LED_PIN_1, OUTPUT);
  pinMode(LED_PIN_2, OUTPUT);
  pinMode(LED_PIN_3, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  digitalWrite(LED_PIN_1, LOW);
  digitalWrite(LED_PIN_2, LOW);
  digitalWrite(LED_PIN_3, LOW);
}

void PINCTRL_setLed(bool on)
{
  bool changed = (on != ledState);
  ledState = on;
  digitalWrite(LED_PIN, on ? LOW : HIGH);
  digitalWrite(LED_PIN_1, on ? HIGH : LOW);
  digitalWrite(LED_PIN_2, on ? HIGH : LOW);
  digitalWrite(LED_PIN_3, on ? HIGH : LOW);
  if (changed) {
    IEC104_reportLedState(on);
  }
}

bool PINCTRL_getLed(void)
{
  return ledState;
}
