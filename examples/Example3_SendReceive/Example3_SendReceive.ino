// Copyright (c) 2024 Daniel Wallner

// Simultaneous receive and send.

#define INS_FAST_TIME 1
#define DEBUG_FULL_TIMING 0
#define DEBUG_CYCLE_TIMING 1
#define DEBUG_DRY_TIMING 0

#define HW_PWM 0 // Will use timer 2 on AVR.
#define SW_PWM 0 // Define HW_PWM or SW_PWM to modulate the IR output. Direct pin loopback will not work with a modulated output.
#define SEND_ESI 0
#define SEND_TECHNICS_SC 0

#include <Inseparates.h>
#include <DebugUtils.h>

#include <ProtocolUtils.h>
#include <ProtocolESI.h>
#include <ProtocolRC5.h>
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

#ifndef INPUT_PULLDOWN
#define INPUT_PULLDOWN INPUT
#endif

const uint16_t kIRSendPin = D_3;
const uint16_t kRC5RecvPin = D_8;
const uint16_t kESISendPin = D_10;
const uint16_t kESIRecvPin = D_10;
const uint16_t kTechnicsSCDataPin = D_5;
const uint16_t kTechnicsSCClockPin = D_4;

using namespace inseparates;

DebugPrinter printer;
Timekeeper timekeeper;
Scheduler scheduler;

#if DEBUG_FULL_TIMING
TimeAccumulator tAcc;
#endif
#if DEBUG_CYCLE_TIMING
CycleChecker cCheck;
#endif

#if HW_PWM
PWMPinWriter irPinWriter(kIRSendPin, LOW);
#elif SW_PWM
SoftPWMPinWriter irPinWriter(kIRSendPin, LOW);
#else
PushPullPinWriter irPinWriter(kIRSendPin);
#endif
#if SEND_ESI
OpenDrainPinWriter esiPinWriter(kESISendPin, HIGH, INPUT_PULLDOWN);
#endif
#if SEND_TECHNICS_SC
OpenDrainPinWriter tscDataWriter(kTechnicsSCDataPin, LOW);
OpenDrainPinWriter tscClockWriter(kTechnicsSCClockPin, LOW);
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
class MainTask  : public SteppedTask, public RxRC5::Delegate, public RxESI::Delegate, public RxTechnicsSC::Delegate
{
#if SEND_ESI
  TxESI _txESI;
#elif SEND_TECHNICS_SC
  TxTechnicsSC _txTechnicsSC;
#else
  TxRC5 _txRC5;
#endif
  RxESI _esiDecoder;
  RxRC5 _rc5Decoder;
  RxTechnicsSC _technicsDecoder;
  Timekeeper _time;

public:
  MainTask() :
#if SEND_ESI
    _txESI(&esiPinWriter, HIGH),
#elif SEND_TECHNICS_SC
    _txTechnicsSC(&tscDataWriter, &tscClockWriter, kTechnicsSCDataPin, kTechnicsSCClockPin, LOW),
#else
    _txRC5(&irPinWriter, LOW),
#endif
    _esiDecoder(HIGH, this),
    _rc5Decoder(LOW, this),
    _technicsDecoder(kTechnicsSCDataPin, kTechnicsSCClockPin, LOW, this)
  {
    pinMode(kESIRecvPin, INPUT); // To turn off pull-up
  }

  void begin()
  {
    scheduler.add(&_esiDecoder, kESIRecvPin);
    scheduler.add(&_rc5Decoder, kRC5RecvPin);
    scheduler.add(&_technicsDecoder);
#if SEND_TECHNICS_SC
    // TxTechnicsSC must unlike other encoders always be active.
    scheduler.add(&_txTechnicsSC);
#endif
    scheduler.add(this);
  }

  void RxESIDelegate_data(uint64_t data, uint8_t bits) override
  {
    // Printing to serial port in these callbacks is not ideal. DebugPrinter is better than Serial though.
    // Only do this for debugging and know it will affect the timing of tasks!
    printer.printf("ESI data: %0lx%0lx bits: %hhu\n", uint32_t(data >> 32),  uint32_t(data), bits);
  }

  void RxRC5Delegate_data(uint16_t data) override
  {
    printer.print("RC5 data: ");
    printer.println(String(data, HEX));
  }

  void RxTechnicsSCDelegate_data(uint32_t data) override
  {
    printer.print("Technics SC: ");
    printer.println(String(data, HEX));
  }

  // This is where everything that you normally put in loop() goes.
  // Never use any form of delay here.
  // Instead return the number of microseconds to sleep before the next step. 
  uint16_t SteppedTask_step() override
  {
    if (_time.microsSinceReset() > 400000L)
    {
      _time.reset();

      static /*const*/ uint8_t address = 16; // Preamp
      static /*const*/ uint8_t command = 13; // Mute
      //static /*const*/ uint8_t command = 16; // Vol down
      //static /*const*/ uint8_t command = 17; // Vol up
      //static /*const*/ uint8_t command = 35; // Speaker A
      //static /*const*/ uint8_t command = 39; // Speaker B
      static uint8_t toggle = 0;
      toggle ^= 1;
#if SEND_ESI
      uint64_t encodedMessage = TxESI::encodeRC5(0, toggle, address, command);
      _txESI.prepare(encodedMessage, TxESI::kRC5MessageBits);
      scheduler.add(&_txESI);
#elif SEND_TECHNICS_SC
      // TxTechnicsSC behaves different than other encoders and will be kept active once added to the scheduler.
      if (!_txTechnicsSC.done()) // Check that we are done with the previous message.
        return 1000;
      uint32_t encodedMessage = TxTechnicsSC::encodeIR(0x00, 0x21); // Volume down.
      _txTechnicsSC.prepare(encodedMessage);
#else
      uint16_t encodedMessage = _txRC5.encodeRC5(toggle, address, command);
      _txRC5.prepare(encodedMessage);
      scheduler.add(&_txRC5);
#endif
      printer.print("Will send: ");
      printer.println(String(encodedMessage, HEX));
    }
    return 10000;
  }
};

MainTask mainTask;

void setup()
{
  Serial.begin(115200);

  while (!Serial)
    delay(50);

  Serial.print("IR output pin: ");
  Serial.println(kIRSendPin);
  Serial.print("ESI output pin: ");
  Serial.println(kESISendPin);
  Serial.print("ESI input pin: ");
  Serial.println(kESIRecvPin);
  Serial.print("RC-5 input pin: ");
  Serial.println(kRC5RecvPin);
#if SEND_TECHNICS_SC
  Serial.print("Technics SC data pin: ");
  Serial.println(kTechnicsSCDataPin);
  Serial.print("Technics SC clock pin: ");
  Serial.println(kTechnicsSCClockPin);
#endif
  Serial.flush();

  scheduler.begin();
  scheduler.add(&printer);
  mainTask.begin();

#if HW_PWM || SW_PWM
  irPinWriter.prepare(36000, 30);
#endif
}

void loop()
{
  // On AVR fastMicros() has microsecond resolution and micros() resolution is 4 microseconds.
  uint16_t now = fastMicros();

#if DEBUG_FULL_TIMING
  TimeInserter tInserter(tAcc, now);
#endif
#if DEBUG_CYCLE_TIMING
  cCheck.tick(now);
#endif

  // Never run anything that uses delays in loop() as that will break the timing of tasks.
  // (See note above about Serial.)
#if DEBUG_DRY_TIMING
  // Not completely dry as we need to poll the printer.
  if (!printer.empty())
    printer.SteppedTask_step();
#else
  scheduler.poll();
#endif

#if DEBUG_FULL_TIMING || DEBUG_CYCLE_TIMING
  if (timekeeper.microsSinceReset(now) < 5000000L)
    return;
  timekeeper.reset();
#endif
#if DEBUG_FULL_TIMING
  tAcc.report(printer);
#endif
#if DEBUG_CYCLE_TIMING
  cCheck.report(printer);
#endif
}
