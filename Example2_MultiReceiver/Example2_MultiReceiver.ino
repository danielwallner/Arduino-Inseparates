// Copyright (c) 2024 Daniel Wallner

// Concurrent receive of different protocols on different pins.

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

const uint16_t kBeo36RecvPin = 4;
const uint16_t kDatalink80RecvPin = 10;
const uint16_t kDatalink86RecvPin = 10;
const uint16_t kESIRecvPin = 10;
const uint16_t kNECRecvPin = 10;
const uint16_t kRC5RecvPin = 10;
const uint16_t kSIRCRecvPin = 10;
const uint16_t kTechnicsSCDataPin = 3;
const uint16_t kTechnicsSCClockPin = 2;

using namespace inseparates;

DebugPrinter printer;
#if DEBUG_FULL_TIMING
TimeAccumulator tAcc;
#endif
#if DEBUG_CYCLE_TIMING
CycleChecker cCheck;
#endif

Scheduler scheduler;

void InsError(uint32_t error)
{
  char errorMsg[5];
  strncpy(errorMsg, (const char*)&error, 4);
  errorMsg[4] = 0;
  Serial.print("ERROR: ");
  Serial.println(errorMsg);
  Serial.flush();
  for(;;);
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

  void RxDatalink80Delegate_data(uint8_t data) override
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

  void RxDatalink86Delegate_data(uint64_t data, uint8_t bits) override
  {
    printer.print("Datalink86: ");
    printer.print(String(uint32_t(data >> 32), HEX));
    printer.print(String(uint32_t(data), HEX));
    printer.print(" ");
    printer.println(String(bits));
  }

  void RxESIDelegate_data(uint32_t data) override
  {
    printer.print("ESI: ");
    printer.println(String(data, HEX));
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

RxBeo36 beo36Decoder(kBeo36RecvPin, LOW, &delegate);
RxDatalink80 datalink80Decoder(kDatalink80RecvPin, LOW, &delegate);
RxDatalink86 datalink86Decoder(kDatalink86RecvPin, LOW, &delegate);
RxESI esiDecoder(kESIRecvPin, LOW, &delegate);
RxNEC necDecoder(kNECRecvPin, LOW, &delegate);
RxRC5 rc5Decoder(kRC5RecvPin, LOW, &delegate);
RxSIRC sircDecoder(kSIRCRecvPin, LOW, &delegate);
RxTechnicsSC technicsDecoder(kTechnicsSCDataPin, kTechnicsSCClockPin, LOW, &delegate);

void setup()
{
  Serial.begin(115200);

  while (!Serial)
    delay(50);

  Serial.println();
  Serial.print("B&O Early input pin: ");
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
#if DEBUG_FULL_TIMING || DEBUG_CYCLE_TIMING
  scheduler.add(&printer);
#endif

#if !AVR
  // AVR cannot handle all protocols at the same time.
  scheduler.add(&beo36Decoder);
  scheduler.add(&datalink80Decoder);
  scheduler.add(&datalink86Decoder);
  scheduler.add(&esiDecoder);
  scheduler.add(&technicsDecoder);
#endif
  scheduler.add(&necDecoder);
  scheduler.add(&rc5Decoder);
  scheduler.add(&sircDecoder);
}

void loop()
{
  // On AVR fastMicros() has microsecond resolution and micros() resolution is 4 microseconds.
  uint32_t now = fastMicros();

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
    printer.SteppedTask_step(now);
#else
  scheduler.SteppedTask_step(now);
#endif

#if DEBUG_FULL_TIMING
  static uint32_t lastReport1;
  if (now - lastReport1 >= 5000000)
  {
    lastReport1 = now;
    tAcc.report(printer);
  }
#endif
#if DEBUG_CYCLE_TIMING
  static uint32_t lastReport2;
  if (now - lastReport2 >= 5010000)
  {
    lastReport2 = now;
    cCheck.report(printer);
  }
#endif
}
