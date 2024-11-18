// Copyright (c) 2024 Daniel Wallner

// Demo with concurrent reception of different protocols on different pins.

#define INS_FAST_TIME 1
#define DEBUG_FULL_TIMING 0
#define DEBUG_CYCLE_TIMING 0
#define DEBUG_DRY_TIMING 0

#include <Inseparates.h>
#include <DebugUtils.h>
#include <ProtocolUtils.h>

#include <ProtocolBeo36.h>
#include <ProtocolDatalink80.h>
#include <ProtocolDatalink86.h>
#include <ProtocolESI.h>
#include <ProtocolNEC.h>
#include <ProtocolRC5.h>
#include <ProtocolSIRC.h>
#include <ProtocolTechnicsSC.h>

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

const uint8_t kBeo36RecvPin = D_2;
const uint8_t kDatalink80RecvPin = D_9;
const uint8_t kDatalink86RecvPin = D_6;
const uint8_t kESIRecvPin = D_10;
const uint8_t kNECRecvPin = D_2;
const uint8_t kRC5RecvPin = D_8;
const uint8_t kSIRCRecvPin = D_2;
const uint8_t kTechnicsSCDataPin = D_5;
const uint8_t kTechnicsSCClockPin = D_4;

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

class Delegate :
  public RxBeo36::Delegate,
  public RxDatalink80::Delegate,
  public RxDatalink86::Delegate,
  public RxESI::Delegate,
  public RxNEC::Delegate,
  public RxRC5::Delegate,
  public RxSIRC::Delegate,
  public RxTechnicsSC::Delegate
{
public:
  void RxBeo36Delegate_data(uint8_t data) override
  {
    // Printing here is not ideal.
    // Only do this for debugging and know it will affect the timing of tasks!
    printer.print("Beo36: ");
    printer.print(" ");
    printer.print(String(data << 1, HEX));
    printer.print(" ");
    printer.println(String(data, HEX));
  }

  void RxDatalink80Delegate_data(uint8_t data, uint8_t bus) override
  {
    printer.print("Datalink80: ");
    printer.print(String(0x7F & ~data, BIN));
    printer.print(" ");
    printer.println(String(data, HEX));
  }

  void RxDatalink80Delegate_timingError() override
  {
    printer.println("Datalink80 timing error");
  }

  void RxDatalink86Delegate_data(uint64_t data, uint8_t bits, uint8_t bus) override
  {
    printer.print("Datalink86: ");
    printer.print(String(uint32_t(data >> 32), HEX));
    printer.print(String(uint32_t(data), HEX));
    printer.print(" ");
    printer.println(String(bits));
  }

  void RxESIDelegate_data(uint64_t data, uint8_t bits) override
  {
    printer.printf("ESI data: %0lx%0lx bits: %hhu\n", uint32_t(data >> 32),  uint32_t(data), bits);
  }

  void RxNECDelegate_data(uint32_t data) override
  {
    printer.print("NEC: ");
    printer.println(String(data, HEX));
  }

  void RxRC5Delegate_data(uint16_t data) override
  {
    printer.print("RC5: ");
    printer.println(String(data, HEX));
  }

  void RxSIRCDelegate_data(uint32_t data, uint8_t bits) override
  {
    printer.print("SIRC: ");
    printer.print(String(data, HEX));
    printer.print(" ");
    printer.println(String(bits));
  }

  void RxTechnicsSCDelegate_data(uint32_t data) override
  {
    printer.print("Technics SC: ");
    printer.println(String(data, HEX));
  }
};

Delegate delegate;

RxBeo36 beo36Decoder(LOW, &delegate);
RxDatalink80 datalink80Decoder(LOW, &delegate);
RxDatalink86 datalink86Decoder(LOW, &delegate);
RxESI esiDecoder(HIGH, &delegate);
RxNEC necDecoder(LOW, &delegate);
RxRC5 rc5Decoder(HIGH, &delegate);
RxSIRC sircDecoder(LOW, &delegate);
RxTechnicsSC technicsDecoder(kTechnicsSCDataPin, kTechnicsSCClockPin, LOW, &delegate);

void setup()
{
  Serial.begin(115200);

  while (!Serial)
    delay(50);

  Serial.print("B&O 36 input pin: ");
  Serial.println(kBeo36RecvPin);
  Serial.print("Datalink 80 input pin: ");
  Serial.println(kDatalink80RecvPin);
  Serial.print("Datalink 86 input pin: ");
  Serial.println(kDatalink86RecvPin);
  Serial.print("ESI input pin: ");
  Serial.println(kESIRecvPin);
  Serial.print("NEC input pin: ");
  Serial.println(kNECRecvPin);
  Serial.print("RC-5 input pin: ");
  Serial.println(kRC5RecvPin);
  Serial.print("SIRC input pin: ");
  Serial.println(kSIRCRecvPin);
  Serial.print("TechnicsSC data pin: ");
  Serial.println(kTechnicsSCDataPin);
  Serial.print("TechnicsSC clock pin: ");
  Serial.println(kTechnicsSCClockPin);
  Serial.flush();

  scheduler.begin();
  scheduler.add(&printer);

#if !AVR
  // AVR cannot reliably handle all protocols at the same time.
  scheduler.add(&beo36Decoder, kBeo36RecvPin);
  scheduler.add(&datalink80Decoder, kDatalink80RecvPin);
  scheduler.add(&datalink86Decoder, kDatalink86RecvPin);
  pinMode(kESIRecvPin, INPUT); // To turn off pull-up
  scheduler.add(&esiDecoder, kESIRecvPin);
  scheduler.add(&technicsDecoder);
#endif
  scheduler.add(&necDecoder, kNECRecvPin);
  scheduler.add(&rc5Decoder, kRC5RecvPin);
  scheduler.add(&sircDecoder, kSIRCRecvPin);
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

  // Never run anything that delays in loop() as that will break the timing of tasks.
  // (See note above about Serial.)
#if DEBUG_DRY_TIMING
  // Not completely dry as we need to poll the printer.
  if (!printer.empty())
    printer.SteppedTask_step();
#else
  scheduler.poll();
#endif

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
