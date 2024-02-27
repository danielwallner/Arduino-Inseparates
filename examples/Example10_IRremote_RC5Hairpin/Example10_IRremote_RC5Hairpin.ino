// Copyright (c) 2024 Daniel Wallner

// Single pin IR translator.
// Uses Philips CD player with RC5 connector as IR-receiver for Yamaha (NEC) and sends RC5 back on same pin.
// This sketch makes it possible to use a Yamaha system remote with a Philips CD player.

#define INS_FAST_TIME 1
#define DEBUG_FULL_TIMING 1
#define DEBUG_CYCLE_TIMING 1
#define IR_RECEIVE_POLL_INTERVAL 1000 // Once every millisecond.

#if !defined(D2) && defined(PD2)
#define D2 PD2
#endif

#ifndef INPUT_PULLDOWN
#define INPUT_PULLDOWN INPUT
#endif

// For IRremote
#define IR_INPUT_IS_ACTIVE_HIGH 1
#define IR_RECEIVE_PIN  D2

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
// Auto input active detection?
// Does not support open drain output.
// IRrecv::enableIRIn has a parameter to enable input pullup.
// Output polarity and modulation on/off are set as input parameters to the IRsend constructor.
// OpenMQTTGateway polarity?

#include <IRremote.hpp>

#include <Inseparates.h>
#include <DebugUtils.h>

#include <ProtocolUtils.h>
#include <ProtocolRC5.h>

const uint16_t kRC5SendPin = IR_RECEIVE_PIN;

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

OpenDrainPinWriter pinWriter(kRC5SendPin, HIGH, INPUT_PULLDOWN);

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
{
  TxRC5 _txRC5;
  uint8_t _command = 0;
  uint8_t _toggle = 0;
  uint8_t _count = 0;

public:
  MainTask() :
    _txRC5(&pinWriter, HIGH)
  {
  }

  void begin()
  {
    scheduler.add(this);
  }

  uint16_t SteppedTask_step() override
  {
    if (_count)
    {
      ++_count;
      if (_count > 20)
      {
        send();
      }
      return IR_RECEIVE_POLL_INTERVAL;
    }

    if (!IrReceiver.decode())
      return IR_RECEIVE_POLL_INTERVAL;

    if (IrReceiver.decodedIRData.protocol != NEC)
      return IR_RECEIVE_POLL_INTERVAL;

    _command = 0;

    // Yamaha CD remotes
    if (IrReceiver.decodedIRData.address == 121)
    {
      switch (IrReceiver.decodedIRData.command)
      {
      case 6: _command = 52; break; // CD_FFWD
      case 5: _command = 50; break; // CD_REW
      case 7: _command = 32; break; // CD_NEXT
      case 4: _command = 33; break; // CD_PREV
      case 2: _command = 53; break; // CD_PLAY
      case 85: _command = 48; break; // CD_PAUSE
      case 86: _command = 54; break; // CD_STOP
      case 1: _command = 45; break; // CD_OPEN_CLOSE
      }
    }
    // Yamaha system remotes VU07410,VU07420,VU07430,VP59040
    if (IrReceiver.decodedIRData.address == 122)
    {
      switch (IrReceiver.decodedIRData.command)
      {
      case 12: _command = 52; break; // CD_FFWD
      case 13: _command = 50; break; // CD_REW
      case 10: _command = 32; break; // CD_NEXT
      case 11: _command = 33; break; // CD_PREV
      case 8: _command = 53; break; // CD_PLAY
      case 9: _command = 54; break; // CD_PAUSE_STOP
      //case 79: _command = ; break; // CD_NEXT_DISC
      }
    }

    if (_command)
    {
      _count = 1;
      if (!(IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT))
        _toggle ^= 1;
    }

    return IR_RECEIVE_POLL_INTERVAL; // Run once every millisecond.
  }

  void send()
  {
    _count = 0;

    uint8_t address = 20;
    uint16_t encodedMessage = _txRC5.encodeRC5(_toggle, address, _command);
    _txRC5.prepare(encodedMessage);
    scheduler.add(&_txRC5);

    printer.print("Send: ");
    printer.println(String(encodedMessage, HEX));
  }
};

MainTask mainTask;

void setup()
{
  Serial.begin(115200);

  while (!Serial)
    delay(50);

  Serial.print("IR input pin: " INS_STR(IR_RECEIVE_PIN));
  Serial.print("RC-5 output pin: ");
  Serial.println(kRC5SendPin);
  Serial.flush();

  IrReceiver.begin(IR_RECEIVE_PIN, false);

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
