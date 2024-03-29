// Copyright (c) 2024 Daniel Wallner

// Prints timing on one or two pins.
// Outputs timing as pulses arrive.
// That means that the printed timing will not be accurate for very short pulses.

#define INS_FAST_TIME 1
#define DEBUG_CYCLE_TIMING 0
#define DUAL_PIN 0
#define PRINT_ACCUMULATED_TIME 0 // Will decrease accuracy for pulses < 200 us on AVR.

#include <Inseparates.h>
#include <DebugUtils.h>
#include <ProtocolUtils.h>

#if !defined(D4) && defined(PD4)
#define D4 PD4
#endif
#if !defined(D5) && defined(PD5)
#define D5 PD5
#endif

const uint16_t kInputPin0 = D4;
#if DUAL_PIN
const uint16_t kInputPin1 = D5;
#endif

const uint32_t kMaxSpaceMicros = 10000;

using namespace inseparates;

#if DEBUG_CYCLE_TIMING
CycleChecker cCheck;
uint32_t lastReport;
#endif

void InsError(uint32_t error)
{
  char errorMsg[5];
  strncpy(errorMsg, (const char*)&error, 4);
  errorMsg[4] = 0;
  Serial.print("ERROR: ");
  Serial.println(errorMsg);
  Serial.flush();
  for(;;) yield();
}

void setup()
{
  Serial.begin(1000000);

  while (!Serial)
    delay(50);

  pinMode(kInputPin0, INPUT_PULLUP);
#if DUAL_PIN
  pinMode(kInputPin1, INPUT_PULLUP);
#endif

#if INS_FAST_TIME
  setupFastTime();
#endif
}

InputFilter inputFilter0;
#if DUAL_PIN
InputFilter inputFilter1;
#endif

uint32_t accumulatedTime = 0;

Timekeeper timekeeper;
DebugPrinter printer;

void loop()
{
  uint8_t pinValue0 = digitalRead(kInputPin0);
#if DUAL_PIN
  uint8_t pinValue1 = digitalRead(kInputPin1);
#endif

  bool didChange = false;
  if (inputFilter0.setState(pinValue0))
  {
    didChange = true;
  }
#if DUAL_PIN
  if (inputFilter1.setState(pinValue1))
  {
    didChange = true;
  }
#endif

  uint16_t now = fastMicros();
  printer.SteppedTask_step();
#if DEBUG_CYCLE_TIMING
  cCheck.tick(now);
#endif

#if DEBUG_CYCLE_TIMING
  if (timekeeper.secondsSinceReset(now) >= 5)
  {
    timekeeper.reset();
    printer.flush();
    cCheck.report(printer);
  }
#endif

  if (!didChange)
  {
    return;
  }

  uint32_t time = inputFilter0.getAndUpdateTimeSinceLastTransition(now);

  if (time > kMaxSpaceMicros)
  {
    accumulatedTime = 0;
  }
  else
  {
    accumulatedTime += time;
  }

  char pinstring[4];
  pinstring[0] = pinValue0 + '0';
#if DUAL_PIN
  pinstring[1] = pinValue1 + '0';
  pinstring[2] = ' ';
  pinstring[3] = 0;
#else
  pinstring[1] = ' ';
  pinstring[2] = 0;
#endif

  printer.print(pinstring);

#if PRINT_ACCUMULATED_TIME
  printer.printf("%lu %lu\n", time, accumulatedTime);
#else
  printer.printf("%lu\n", time);
#endif
}
