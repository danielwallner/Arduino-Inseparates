// Copyright (c) 2024 Daniel Wallner

// Simultaneous receive and send

#define INS_FAST_TIME 1
#define DEBUG_FULL_TIMING 0
#define DEBUG_CYCLE_TIMING 0
#define DEBUG_DRY_TIMING 0

#define SEND_ESI 0
#define SEND_TECHNICS_SC 1

#include <Inseparates.h>
#include <DebugUtils.h>

#include <ProtocolUtils.h>
#include <ProtocolESI.h>
#include <ProtocolRC5.h>
#include <ProtocolTechnicsSC.h>

const uint16_t kIRSendPin = 10;
const uint16_t kRC5RecvPin = 10;
const uint16_t kESIRecvPin = 10;
const uint16_t kTechnicsSCDataPin = 3;
const uint16_t kTechnicsSCClockPin = 2;

using namespace inseparates;

DebugPrinter printer;
Scheduler scheduler;

#if DEBUG_FULL_TIMING
TimeAccumulator tAcc;
#endif
#if DEBUG_CYCLE_TIMING
CycleChecker cCheck;
#endif

PushPullPinWriter pinWriter(kIRSendPin);
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
  for(;;);
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
  TimeKeeper _time;
  uint16_t encodedValue;

public:
  MainTask() :
#if SEND_ESI
    _txESI(&pinWriter, LOW),
#elif SEND_TECHNICS_SC
    _txTechnicsSC(&tscDataWriter, &tscClockWriter, kTechnicsSCDataPin, kTechnicsSCClockPin, LOW),
#else
    _txRC5(&pinWriter, LOW),
#endif
    _esiDecoder(kESIRecvPin, LOW, this),
    _rc5Decoder(kRC5RecvPin, LOW, this),
    _technicsDecoder(kTechnicsSCDataPin, kTechnicsSCClockPin, LOW, this)
  {
    // Don't start things here since the initalization order of global variables in different translation units is unspecified.
  }

  void begin()
  {
    scheduler.add(&_esiDecoder);
    //scheduler.add(&_rc5Decoder);
    scheduler.add(&_technicsDecoder);
#if SEND_TECHNICS_SC
    // TxTechnicsSC must unlike other transmitters be active always.
    scheduler.add(&_txTechnicsSC);
#endif
    scheduler.add(this);
  }

  void RxESIDelegate_data(uint32_t data) override
  {
    // Printing to serial port in these callbacks is not idea.lDebugPrinter is though better than Serial.
    // Only do this for debugging and know it will affect the timing of tasks!
    printer.print("ESI data: ");
    printer.println(String(data, HEX));
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
  uint16_t SteppedTask_step(uint32_t now) override
  {
    if (_time.millisSinceReset(now) > 400)
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
      static uint32_t i = 0;
      static uint32_t j = 0;
      uint32_t data = (i << 16) | j;
      ++j;
      if (!j)
      {
        ++i;
      }
      uint32_t encodedMessage = TxESI::encodeRC5(0, toggle, address, command);
      _txESI.prepare(encodedMessage);
      scheduler.add(&_txESI);
#elif SEND_TECHNICS_SC
      // TxTechnicsSC behaves different than other transmitters will be kept active once added to the scheduler.
      if (!_txTechnicsSC.done()) // Check that we are done with the previous message.
        return 1000;
      uint32_t encodedMessage = TxTechnicsSC::encodeIR(0x00, 0x21); // Volume down.
      _txTechnicsSC.prepare(encodedMessage);
#else
      uin16_t encodedMessage = _txRC5.encodeRC5(toggle, address, command);
      _txRC5.prepare(encodedMessage);
      scheduler.add(&_txRC5);
#endif
      printer.print("Will send: ");
      printer.println(String(encodedValue, HEX));
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
  Serial.println();
  Serial.print("IR send pin: ");
  Serial.println(kIRSendPin);
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
