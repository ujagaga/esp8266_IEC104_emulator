/*
 *  Author: Rada Berar
 *  email: ujagaga@gmail.com
 *
 *  Mains frequency measurement via zero-crossing pulses on FREQ_PIN.
 *  Expects one edge per AC cycle (e.g. from an opto zero-cross detector).
 */
#include <Arduino.h>
#include "config.h"
#include "freqmeter.h"

#define MIN_PERIOD_US 1000  /* 1000Hz, rejects glitches/bounce */
#define MAX_PERIOD_US 40000 /* 25Hz, rejects missed-edge glitches */
#define SIGNAL_TIMEOUT_US 100000UL /* no edge for 100ms -> report no signal */

static volatile uint32_t lastEdgeMicros = 0;
static volatile uint32_t lastPeriodMicros = 0;
static volatile bool haveReading = false;

static void IRAM_ATTR onZeroCross(void) {
  uint32_t now = micros();
  uint32_t period = now - lastEdgeMicros;
  lastEdgeMicros = now;

  if (period >= MIN_PERIOD_US && period <= MAX_PERIOD_US) {
    lastPeriodMicros = period;
    haveReading = true;
  }
}

void FREQMETER_init(void) {
  pinMode(FREQ_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(FREQ_PIN), onZeroCross, RISING);
}

float FREQMETER_getFrequency(void) {
  noInterrupts();
  uint32_t period = lastPeriodMicros;
  uint32_t lastEdge = lastEdgeMicros;
  bool have = haveReading;
  interrupts();

  if (!have || (uint32_t)(micros() - lastEdge) > SIGNAL_TIMEOUT_US) {
    return 0.0f;
  }

  return 1000000.0f / (float)period;
}
