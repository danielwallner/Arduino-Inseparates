// Copyright (c) 2024 Daniel Wallner

// Example and stress test with two full duplex UARTs.

#include <Arduino.h>

#if defined(ESP8266) // WEMOS D1 R2
static const uint8_t D_2  = 16; // No interrupts
static const uint8_t D_3  = 5;
static const uint8_t D_4  = 4;
static const uint8_t D_5  = 0;
static const uint8_t D_6  = 2;
static const uint8_t D_7  = 14;
#elif defined(ESP32) // WEMOS D1 R32
static const uint8_t D_2  = 26;
static const uint8_t D_3  = 25;
static const uint8_t D_4  = 17;
static const uint8_t D_5  = 16;
static const uint8_t D_6  = 27;
static const uint8_t D_7  = 14;
#else
static const uint8_t D_2  = 2;
static const uint8_t D_3  = 3;
static const uint8_t D_4  = 4;
static const uint8_t D_5  = 5;
static const uint8_t D_6  = 6;
static const uint8_t D_7  = 7;
#endif

// These defines must be after Arduino.h to get pin definitions but before all the Inseparates headers.
#define INS_DEBUGGING 1 // Extra debug output
#define INS_FAST_TIME 1
#define INS_ENABLE_INPUT_FILTER 0
#define INS_UART_FRACTIONAL_TIME 0
#define INS_SAMPLE_DEBUG_PIN D_4
#define INS_TIMEOUT_DEBUG_PIN D_5
#define DEBUG_FULL_TIMING 0
#define DEBUG_CYCLE_TIMING 1
#define DEBUG_DRY_TIMING 0
#if AVR || ARDUINO_ARCH_SAMD
#define ENABLE_READ_INTERRUPTS false // Not working
#define ENABLE_WRITE_TIMER 0 // Not supported on AVR
#else
#define ENABLE_READ_INTERRUPTS true
#define ENABLE_WRITE_TIMER 1
#endif

#if defined(AVR)
#define DUAL_UARTS 0 // Also enables more debug output.
const uint32_t baudRate = 2400;
#else
#define DUAL_UARTS 1
const uint32_t baudRate = 4800;
#endif
const uint16_t kUART1Pin = D_2;
const uint16_t kUART2Pin = D_3;

#include <Inseparates.h>
#include <ProtocolUART.h>
#include <DebugUtils.h>

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
#if ENABLE_WRITE_TIMER
InterruptWriteScheduler writeScheduler(20);
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

// This task shares time with other active tasks.
class FullDuplexUART  : public Scheduler::Delegate, public RxUART::Delegate
{
  Timekeeper _timekeeper;
  uint8_t _pin;
#if ENABLE_WRITE_TIMER
  InterruptPinWriter _pinWriter;
#else
  PushPullPinWriter _pinWriter;
#endif
  TxUART _tx;
  RxUART _rx;
  uint8_t _sendData;
  uint8_t _receiveData;
public:
  FullDuplexUART(uint8_t pin) :
    _pin(pin),
#if ENABLE_WRITE_TIMER
    _pinWriter(&writeScheduler, pin, LOW, HIGH),
#else
    _pinWriter(pin),
#endif
    _tx(&_pinWriter, HIGH),
    _rx(HIGH, this)
  {
    _tx.setBaudrate(baudRate);
    _rx.setBaudrate(baudRate);
  }

  void begin()
  {
    _sendData = 0;
    _receiveData = 0;
    scheduler.add(&_rx, _pin, ENABLE_READ_INTERRUPTS);
    // Trigger start
    SchedulerDelegate_done(nullptr);
  }

  void RxUARTDelegate_data(uint8_t data, uint8_t bus) override
  {
    if (data == 0xFF)
    {
      printer.printf("Done: %p\n", this);
      scheduler.remove(&_rx);
    }
    if (data != _receiveData)
    {
      printer.printf("Did get: 0x%hhX != 0x%hhX\n", data, _receiveData);
      scheduler.remove(&_rx);
    }
    ++_receiveData;
  }

  void RxUARTDelegate_timingError(uint8_t bus) override
  {
    printer.printf("Timing Error at: %hhu\n", _sendData);
    scheduler.remove(&_rx);
  }

  void RxUARTDelegate_parityError(uint8_t bus) override
  {
    printer.printf("Parity Error at: %hhu\n", _sendData);
    scheduler.remove(&_rx);
  }

  void SchedulerDelegate_done(SteppedTask */*task*/) override
  {
    if (!scheduler.active(&_rx))
      return;
#if !DUAL_UARTS
    printer.printf("%hhX\n", _sendData);
    printer.flush();
#endif
    _tx.prepare(_sendData);
    ++_sendData;
    scheduler.add(&_tx, this);
  }
};

FullDuplexUART uart1(kUART1Pin);
#if DUAL_UARTS
FullDuplexUART uart2(kUART2Pin);
#endif

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    delay(50);

  Serial.println("Dual full duplex UART test");

  timekeeper.reset();

  scheduler.begin();
  scheduler.add(&printer);
#if ENABLE_WRITE_TIMER
  scheduler.add(&writeScheduler);
#endif

#ifdef INS_SAMPLE_DEBUG_PIN
  pinMode(INS_SAMPLE_DEBUG_PIN, OUTPUT);
#endif
#ifdef INS_TIMEOUT_DEBUG_PIN
  pinMode(INS_TIMEOUT_DEBUG_PIN, OUTPUT);
#endif
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

  // Never run anything that delays in loop() as that will break the timing of tasks.
  // (See note above about Serial.)
#if DEBUG_DRY_TIMING
  // Not completely dry as we need to poll the printer.
  if (!printer.empty())
    printer.SteppedTask_step();
#else
  scheduler.poll();
#endif

  if (timekeeper.microsSinceReset(now) < 2000000)
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

  Serial.println("Restarting UART test");
  Serial.flush();

  uart1.begin();
#if DUAL_UARTS
  uart2.begin();
#endif
}
