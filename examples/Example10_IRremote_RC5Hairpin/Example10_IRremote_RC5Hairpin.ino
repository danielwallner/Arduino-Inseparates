// Copyright (c) 2024 Daniel Wallner

// Single pin IR translator. Uses IRremote as receiver.
// Uses a Philips CD player with an RC5 connector as IR-receiver for Yamaha (NEC) and sends RC5 back on same pin.
// This sketch makes it possible to use a Yamaha remote with a Philips CD player.

// This will likely only work with remotes that sends a true NEC repeat message.
// There may not be room to send an RC-5 message between normal NEC messages.
// Define SEND_ON_NON_REPEAT to send on all messages if there's room in the first message.
// There will be a more noticeable delay if SEND_ON_NON_REPEAT is disabled.

// A logic analyzer is your friend when debugging.

#define INS_FAST_TIME 1
//#define DEBUG_FULL_TIMING 1
//#define DEBUG_CYCLE_TIMING 1
//#define DEBUG_PRINTS 1
#define SEND_ON_NON_REPEAT 1
//#define USE_IRREMOTE 1
#define IR_RECEIVE_POLL_INTERVAL 1000 // Once every millisecond.
#define SEND_DELAY_US 7500
#define RECEIVE_HOLD_OFF 26000
#define MAX_REPEAT_DELAY_US 250000UL

#if defined(ESP8266) // WEMOS D1 R2
static const uint8_t D_8  = 12;
#elif defined(ESP32) // WEMOS D1 R32
static const uint8_t D_8  = 12;
#else // Uno / Zero
static const uint8_t D_8  = 8;
#endif

#ifndef INPUT_PULLDOWN
#define INPUT_PULLDOWN INPUT
#endif

const uint8_t kRC5Pin = D_8;

// IRMP:
// Does not support open drain output.
// IRMP_ENABLE_PIN_CHANGE_INTERRUPT
// IRMP_HIGH_ACTIVE
// IR_OUTPUT_IS_ACTIVE_LOW
// IRSND_GENERATE_NO_SEND_RF // Also forces active low output!

// IRremote:
// Only the TinyReceiver uses pin change interrupt.
// The normal receiver is using polling driven by timer interrupts.
// Output is always active low.
// IR_INPUT_IS_ACTIVE_HIGH
// USE_NO_SEND_PWM
// USE_OPEN_DRAIN_OUTPUT_FOR_SEND_PIN

// IRremoteESP8266:
// Always using pin change interrupt on all pins.
// Auto input active detection
// Does not support open drain output.
// IRrecv::enableIRIn has a parameter to enable input pullup.
// Output polarity and modulation on/off are set as input parameters to the IRsend constructor.

// None of the above libraries can be used for send as this requires active high open drain.
// IRremote can be used to receive which may be useful to support protocols not supported by Inseparates.
// Enable USE_IRREMOTE to use IRremote.

#include <Inseparates.h>
#include <DebugUtils.h>

#include <ProtocolUtils.h>
#include <ProtocolRC5.h>

#if USE_IRREMOTE
#define IR_FEEDBACK_LED_PIN 3
#define DECODE_NEC
#define IR_INPUT_IS_ACTIVE_HIGH 1
#include <IRremote.hpp>
#else
#include <ProtocolNEC.h>
#endif

using namespace inseparates;

enum CDCommands
{
  CD_POWER = 12,
  CD_1 = 1,
  CD_2 = 2,
  CD_3 = 3,
  CD_4 = 4,
  CD_5 = 5,
  CD_6 = 6,
  CD_7 = 7,
  CD_8 = 8,
  CD_9 = 9,
  CD_0 = 0,
  CD_FFWD = 52,
  CD_REW = 50,
  CD_NEXT = 32,
  CD_PREV = 33,
  CD_PLAY = 53,
  CD_PAUSE = 48,
  CD_STOP = 54,
  CD_RANDOM = 28,
  CD_REPEAT = 29,
  CD_NEXT_DISC = 30,
  CD_OPEN_CLOSE = 45,
};

DebugPrinter printer;
Timekeeper timekeeper;
Scheduler scheduler;

#if DEBUG_FULL_TIMING
TimeAccumulator tAcc;
#endif
#if DEBUG_CYCLE_TIMING
CycleChecker cCheck;
#endif

OpenDrainPinWriter pinWriter(kRC5Pin, HIGH, INPUT_PULLDOWN);

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

class MainTask  : public SteppedTask
#if USE_IRREMOTE
  , public Scheduler::Delegate
#else
  , public RxNEC::Delegate
#endif
{
  DummyTask _dummy;
  Timekeeper _timekeeper;
  TxRC5 _txRC5;
#if !USE_IRREMOTE
  RxNEC _rxNEC;
#endif
  uint8_t _toggle = 0;
  uint16_t _encodedMessage = 0;

public:
  MainTask() :
    _txRC5(&pinWriter, HIGH)
#if !USE_IRREMOTE
    ,_rxNEC(HIGH, this)
#endif
  {
    pinMode(kRC5Pin, INPUT_PULLDOWN); // To turn off pull-up
  }

  void begin()
  {
    scheduler.add(this);
#if USE_IRREMOTE
    IrReceiver.begin(kRC5Pin, false);
#else
    scheduler.add(&_rxNEC, kRC5Pin);
#endif
  }

  uint16_t SteppedTask_step() override
  {
    _timekeeper.tick();
#if USE_IRREMOTE
    if (scheduler.active(&_dummy))
      return IR_RECEIVE_POLL_INTERVAL;

    if (!IrReceiver.decode())
      return IR_RECEIVE_POLL_INTERVAL;

    uint8_t protocol = IrReceiver.decodedIRData.protocol;
    uint8_t address = IrReceiver.decodedIRData.address;
    uint8_t command = IrReceiver.decodedIRData.command;
    bool isRepeat = IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT;
    bool parityError = IrReceiver.decodedIRData.flags & IRDATA_FLAGS_PARITY_FAILED;
    if (parityError || (protocol != NEC && protocol != NEC2))
    {
      IrReceiver.resume();
      return IR_RECEIVE_POLL_INTERVAL;
    }

    return OnMessage(address, command, isRepeat);
#else
    return RECEIVE_HOLD_OFF;
#endif
  }

#if !USE_IRREMOTE
  void RxNECDelegate_data(uint32_t data) override
  {
    uint8_t address = data;
    uint8_t command = data >> 16;
    bool isRepeat = !data;
    if (data && RxNEC::checkParity(data))
    {
      return;
    }
    OnMessage(address, command, isRepeat);
  }
#endif

  uint16_t OnMessage(uint8_t address, uint8_t command, bool isRepeat)
  {
    uint8_t _command = 0;
    // Yamaha CD remotes
    if (address == 121)
    {
      switch (command)
      {
      case 17: _command = CD_1; break;
      case 18: _command = CD_2; break;
      case 19: _command = CD_3; break;
      case 20: _command = CD_4; break;
      case 21: _command = CD_5; break;
      case 22: _command = CD_6; break;
      case 23: _command = CD_7; break;
      case 24: _command = CD_8; break;
      case 25: _command = CD_9; break;
      case 16: _command = CD_0; break;
      case 6: _command = CD_FFWD; break;
      case 5: _command = CD_REW; break;
      case 7: _command = CD_NEXT; break;
      case 4: _command = CD_PREV; break;
      case 2: _command = CD_PLAY; break;
      case 85: _command = CD_PAUSE; break;
      case 86: _command = CD_STOP; break;
      case 27: _command = CD_RANDOM; break;
      case 8: _command = CD_REPEAT; break;
      case 79: _command = CD_NEXT_DISC; break;
      case 1: _command = CD_OPEN_CLOSE; break;
      }
    }
    // Yamaha system remotes VU07410,VU07420,VU07430,VP59040
    if (address == 122)
    {
      switch (command)
      {
      case 12: _command = CD_FFWD; break;
      case 13: _command = CD_REW; break;
      case 10: _command = CD_NEXT; break;
      case 11: _command = CD_PREV; break;
      case 8: _command = CD_PLAY; break;
      //case 9: _command = CD_PAUSE; break; // CD_PAUSE_STOP
      case 9: _command = CD_STOP; break; // CD_PAUSE_STOP
      case 79: _command = CD_NEXT_DISC; break;
      }
    }

    if (!isRepeat)
    {
      _timekeeper.reset();
      if (!_command)
      {
        _encodedMessage = 0;
#if USE_IRREMOTE
        IrReceiver.resume();
#endif
#if DEBUG_PRINTS
        printer.print("Unsupported: a");
        printer.print(String(address));
        printer.print(" c");
        printer.println(String(command));
#endif
        return IR_RECEIVE_POLL_INTERVAL;
      }

      _toggle ^= 1;
      _encodedMessage = TxRC5::encodeRC5(_toggle, 20, _command);
#if !SEND_ON_NON_REPEAT
#if USE_IRREMOTE
      IrReceiver.resume();
#endif
      return IR_RECEIVE_POLL_INTERVAL;
#endif
    }

    if (_timekeeper.microsSinceReset() > MAX_REPEAT_DELAY_US)
    {
      _encodedMessage = 0;
    }

    if (!_encodedMessage)
    {
#if USE_IRREMOTE
      IrReceiver.resume();
#endif
      return IR_RECEIVE_POLL_INTERVAL;
    }
    _timekeeper.reset();

    _txRC5.prepare(_encodedMessage, false); // We must not enable sleep until next repeat or the send task may still be active when we schedule it again!
#if USE_IRREMOTE
    scheduler.add(&_txRC5);
    scheduler.addDelayed(&_dummy, RECEIVE_HOLD_OFF, this);
#else
    scheduler.addDelayed(&_txRC5, SEND_DELAY_US);
#endif

#if DEBUG_PRINTS
    printer.print("Sent: ");
    printer.println(String(_encodedMessage, HEX));
#endif
    return RECEIVE_HOLD_OFF;
  }

#if USE_IRREMOTE
  void SchedulerDelegate_done(SteppedTask *task)
  {
    if (task == &_dummy)
    {
      IrReceiver.resume();
    }
  }
#endif
};

MainTask mainTask;

void setup()
{
  Serial.begin(115200);

  while (!Serial)
    delay(50);

  Serial.print(F("RC-5 pin: "));
  Serial.println(kRC5Pin);
  Serial.flush();

  scheduler.begin();
  scheduler.add(&printer);
  mainTask.begin();
}

void loop()
{
  uint16_t now = fastMicros();

#if DEBUG_FULL_TIMING
  TimeInserter tInserter(tAcc, now);
#endif
#if DEBUG_CYCLE_TIMING
  cCheck.tick(now);
#endif

  scheduler.poll();

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
