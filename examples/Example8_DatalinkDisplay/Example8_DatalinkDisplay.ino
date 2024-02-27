// Copyright (c) 2024 Daniel Wallner

// Display data on Bang & Olufsen Datalink.

#define INS_FAST_TIME 1
#define DEBUG_FULL_TIMING 0
#define DEBUG_CYCLE_TIMING 1

#include <Inseparates.h>
#include <DebugUtils.h>
#include <ProtocolUtils.h>

#include <ProtocolDatalink86.h>

#if !defined(D5) && defined(PD5)
#define D5 PD5
#endif

const uint16_t kDatalink86RecvPin = D5;

using namespace inseparates;

DebugPrinter printer;
#if DEBUG_FULL_TIMING
TimeAccumulator tAcc;
#endif
#if DEBUG_CYCLE_TIMING
CycleChecker cCheck;
#endif

Timekeeper timekeeper;
Scheduler scheduler;

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

class Delegate : public RxDatalink86::Delegate
{
public:
  void RxDatalink86Delegate_data(uint64_t data, uint8_t bits) override
  {
    printer.print("Datalink86: ");
    printer.print(String(uint32_t(data >> 32), HEX));
    printer.print(String(uint32_t(data), HEX));
    printer.print(" ");
    printer.println(String(bits));
  }
};

Delegate delegate;

RxDatalink86 datalink86Decoder(LOW, &delegate);

void setup()
{
  Serial.begin(115200);

  while (!Serial)
    delay(50);

  Serial.print("Datalink 86 input pin: ");
  Serial.println(kDatalink86RecvPin);
  Serial.flush();

  scheduler.begin();
#if DEBUG_FULL_TIMING || DEBUG_CYCLE_TIMING
  scheduler.add(&printer);
#endif

  scheduler.add(&datalink86Decoder, kDatalink86RecvPin);
}

void loop()
{
  // On AVR fastMicros() has microsecond resolution and the resolution of micros() is 4 microseconds.
  uint16_t now = fastMicros();

#if DEBUG_FULL_TIMING
  TimeInserter tInserter(tAcc, now);
#endif
#if DEBUG_CYCLE_TIMING
  cCheck.tick(now);
#endif

  scheduler.poll();

  if (timekeeper.microsSinceReset(now) < 5000000L)
  {
    return;
  }
  timekeeper.reset();
#if DEBUG_FULL_TIMING
  tAcc.report(printer);
#endif
#if DEBUG_CYCLE_TIMING
  cCheck.report(printer);
#endif
}
