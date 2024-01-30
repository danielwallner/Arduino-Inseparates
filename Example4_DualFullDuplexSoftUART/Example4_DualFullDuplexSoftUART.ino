// Copyright (c) 2024 Daniel Wallner

// Example and stress test with two full duplex UARTs.

#define INS_FAST_TIME 1
#define DEBUG_FULL_TIMING 0
#define DEBUG_CYCLE_TIMING 0
#define DEBUG_DRY_TIMING 0

// Disabling this enables more debug printing.
#define DUAL_UARTS 1

#include <Inseparates.h>
#include <ProtocolUART.h>
#include <DebugUtils.h>

const uint32_t baudRate = 9600;
const uint16_t kUART1Pin = 2;
const uint16_t kUART2Pin = 13;

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

// This task shares time with other active tasks.
class FullDuplexUART  : public Scheduler::Delegate, public RxUART::Delegate
{
  TimeKeeper _time;
  TxUART _tx;
  RxUART _rx;
  uint8_t _sendData = 0;
  uint8_t _receiveData = 0;
public:
  FullDuplexUART(uint8_t pin) :
    _tx(pin, HIGH),
    _rx(pin, HIGH, this)
  {
    _tx.setBaudrate(baudRate);
    _rx.setBaudrate(baudRate);
    // Don't start things here since the initalization order of global variables in different translation units is unspecified.
  }

  void begin()
  {
    scheduler.add(&_rx);
    // Trigger start
    SchedulerDelegate_done(nullptr);
  }

  void RxUARTDelegate_data(uint8_t data) override
  {
    if (data == 0xFF)
    {
      Serial.println("Done");
      scheduler.remove(&_rx);
    }
    if (data != _receiveData)
    {
      Serial.print("Did get: ");
      Serial.print(data, HEX);
      Serial.print(" != ");
      Serial.println(_receiveData, HEX);
      scheduler.remove(&_rx);
    }
    ++_receiveData;
  }

  void RxUARTDelegate_timingError() override
  {
    Serial.println("Timing Error");
    scheduler.remove(&_rx);
  }

  void RxUARTDelegate_parityError() override
  {
    Serial.println("Parity Error");
    scheduler.remove(&_rx);
  }

  void SchedulerDelegate_done(SteppedTask */*task*/) override
  {
    if (!scheduler.active(&_rx))
      return;
#ifndef DUAL_UARTS
    Serial.print("Will send: ");
    Serial.println(_sendData, HEX);
    Serial.flush();
#endif
    _tx.prepare(_sendData);
    ++_sendData;
    scheduler.add(&_tx, this);
  }
};

FullDuplexUART uart1(kUART1Pin);
FullDuplexUART uart2(kUART2Pin);

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    delay(50);

#if ESP8266
  // This delay seems necessary after flashing, but not after power up or activating the reset button.
  delay(5000);
  Serial.println();
#endif
  Serial.println("Starting UART test");
  Serial.flush(); // Make sure that the serial interrupt doesn't interfere with anything. 

  scheduler.begin();
  scheduler.add(&printer);
  uart1.begin();
#ifdef DUAL_UARTS
  uart2.begin();
#endif
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
  if (!lastReport1)
    lastReport1 = now;
  if (now - lastReport1 >= 5000000)
  {
    lastReport1 = now;
    tAcc.report(printer);
  }
#endif
#if DEBUG_CYCLE_TIMING
  static uint32_t lastReport2;
  if (!lastReport2)
    lastReport2 = now;
  if (now - lastReport2 >= 5010000)
  {
    lastReport2 = now;
    cCheck.report(printer);
  }
#endif
}
