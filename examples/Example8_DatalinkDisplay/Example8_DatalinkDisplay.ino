// Copyright (c) 2024 Daniel Wallner

// Display data on Bang & Olufsen Datalink.
// Work inprogress only prints raw data.

#define INS_FAST_TIME 1
#define DEBUG_FULL_TIMING 0
#define DEBUG_CYCLE_TIMING 1
#define ENABLE_READ_INTERRUPTS false

#include <Inseparates.h>
#include <DebugUtils.h>
#include <ProtocolUtils.h>

#include <ProtocolDatalink86.h>

#if defined(ESP8266) // WEMOS D1 R2
static const uint8_t D_2  = 16;
static const uint8_t D_3  = 5;
static const uint8_t D_4  = 4;
static const uint8_t D_5  = 0;
static const uint8_t D_6  = 2;
static const uint8_t D_7  = 14;
static const uint8_t D_8  = 12;
static const uint8_t D_9  = 13;
static const uint8_t D_10 = 15;
#elif defined(ESP32) // WEMOS D1 R32
static const uint8_t D_2  = 26;
static const uint8_t D_3  = 25;
static const uint8_t D_4  = 17;
static const uint8_t D_5  = 16;
static const uint8_t D_6  = 27;
static const uint8_t D_7  = 14;
static const uint8_t D_8  = 12;
static const uint8_t D_9  = 13;
static const uint8_t D_10 = 5;
#else
static const uint8_t D_2  = 2;
static const uint8_t D_3  = 3;
static const uint8_t D_4  = 4;
static const uint8_t D_5  = 5;
static const uint8_t D_6  = 6;
static const uint8_t D_7  = 7;
static const uint8_t D_8  = 8;
static const uint8_t D_9  = 9;
static const uint8_t D_10 = 10;
#endif

const uint16_t kDatalink86RecvPin = D_7;

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
  void RxDatalink86Delegate_data(uint64_t data, uint8_t bits, uint8_t bus) override
  {
    if (data >> 32)
      printer.printf("Datalink86: 0x%lX%08lX %hd\n", long(data >> 32), long(data), short(bits));
    else
      printer.printf("Datalink86: 0x%lX %hd\n", long(data), short(bits));
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

  scheduler.add(&datalink86Decoder, kDatalink86RecvPin, ENABLE_READ_INTERRUPTS);
}

void loop()
{
  // On AVR fastMicros() has microsecond resolution and the resolution of micros() is 4 microseconds.
  ins_micros_t now = fastMicros();

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
