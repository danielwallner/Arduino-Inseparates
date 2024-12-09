// Copyright (c) 2024 Daniel Wallner

// Implements the wired interconnect protocols and B&O 36/455 kHz IR decoding.
// Implements RC5/SIRC/NEC IR support When ENABLE_IRREMOTE is disabled.

#define DEBUG_FULL_TIMING 0
#define DEBUG_CYCLE_TIMING 0

#if ENABLE_IRREMOTE
// For some reason, turning this on makes IRremoteESP8266 receive not working.
// Turning this off lowers reliablitity and blocks reception during IRremoteESP8266 send.
#define ENABLE_READ_INTERRUPTS false
#else
#define ENABLE_READ_INTERRUPTS true
#endif

// Write timer is currently not supported for PWM output which makes IR timing less accurate
#if defined(ESP32)
#define ENABLE_WRITE_TIMER 1
#else
// IRremoteESP8266 use the only available timer on ESP8266.
// Ideally, this should be shared, as turning it off makes output timing accuracy worse.
#define ENABLE_WRITE_TIMER 0
#endif

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
static const uint8_t D_11 = 13;
static const uint8_t D_12 = 12;
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
static const uint8_t D_11 = 23;
static const uint8_t D_12 = 19;
#endif

#include "IRR.h"

#include <Inseparates.h>

#include <ProtocolUtils.h>
#include <ProtocolRC5.h>
#include <ProtocolESI.h>
#include <ProtocolNEC.h>
#include <ProtocolSIRC.h>
#include <ProtocolBeo36.h>
#include <ProtocolDatalink80.h>
#include <ProtocolDatalink86.h>
#include <ProtocolTechnicsSC.h>

#include <map>
#include <deque>

#ifndef INPUT_PULLDOWN
#define INPUT_PULLDOWN INPUT
#endif

#if REV_A
#define HAVE_TRIGGER 0
const uint16_t kTrigger0Pin = D_5;
const uint16_t kTrigger1Pin = D_4;

const uint16_t kIR455ReceivePin = D_6;
#define HAVE_RC5 1
const uint16_t kRC5Pin = D_8;
#define HAVE_ESI 1
const uint16_t kESIPin = D_12;
#define HAVE_SR 1
const uint16_t kSRPin = D_7; // NEC/SIRC
#if !HAVE_ESI
#define HAVE_DATALINK86 1
const uint16_t kDatalink86Pin = D_12;
#endif
#if !HAVE_RC5
#define HAVE_DATALINK80 1
const uint16_t kDatalink80Tape1Pin = D_8;
const uint16_t kDatalink80Tape2Pin = D_9;
#endif
#if !HAVE_TRIGGER
// Only enable Technics System Control if you really need it as it's always active and takes processing time even when idle.
// This is also less reliable than the other receivers as it isn't using interrupts and will be blocked by other tasks.
#define HAVE_TECHNICS_SC 1
const uint8_t kTechnicsSCDataPin = D_5;
const uint8_t kTechnicsSCClockPin = D_4;
#endif


#else


#define HAVE_TRIGGER 0
const uint16_t kTrigger0Pin = D_5;
const uint16_t kTrigger1Pin = D_6;

const uint16_t kIR455ReceivePin = D_4;
#define HAVE_RC5 1
const uint16_t kRC5Pin = D_8;
#define HAVE_ESI 1
const uint16_t kESIPin = D_10;
#define HAVE_SR 0
const uint16_t kSRPin = D_7; // NEC/SIRC
#if !HAVE_SR
#define HAVE_DATALINK86 1
const uint16_t kDatalink86Pin = D_7;
#endif
#if !HAVE_RC5
#define HAVE_DATALINK80 1
const uint16_t kDatalink80Tape1Pin = D_5;
const uint16_t kDatalink80Tape2Pin = D_6;
#endif
#if !HAVE_TRIGGER
#define HAVE_TECHNICS_SC 1
const uint8_t kTechnicsSCDataPin = D_5;
const uint8_t kTechnicsSCClockPin = D_6;
#endif
#endif

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
InterruptWriteScheduler writeScheduler(30);
#endif

#if ENABLE_WRITE_TIMER
#define SCHEDULER_PARAM &writeScheduler,
#else
#define InterruptPinWriter OpenDrainPinWriter
#define SCHEDULER_PARAM
#endif
#if HAVE_TRIGGER
InterruptPinWriter trigger0PinWriter(SCHEDULER_PARAM kTrigger0Pin, LOW, INPUT_PULLUP);
InterruptPinWriter trigger1PinWriter(SCHEDULER_PARAM kTrigger1Pin, LOW, INPUT_PULLUP);
#endif
#if HAVE_RC5
InterruptPinWriter rc5PinWriter(SCHEDULER_PARAM kRC5Pin, HIGH, INPUT_PULLDOWN);
#endif
#if HAVE_ESI
InterruptPinWriter esiPinWriter(SCHEDULER_PARAM kESIPin, HIGH, INPUT_PULLDOWN);
#endif
#if HAVE_SR
InterruptPinWriter srPinWriter(SCHEDULER_PARAM kSRPin, LOW, INPUT_PULLUP);
#endif
#if HAVE_DATALINK86
InterruptPinWriter datalink86PinWriter(SCHEDULER_PARAM kDatalink86Pin, LOW, INPUT_PULLUP);
#endif
#if HAVE_DATALINK80
InterruptPinWriter datalink80Tape1PinWriter(SCHEDULER_PARAM kDatalink80Tape1Pin, LOW, INPUT_PULLUP);
InterruptPinWriter datalink80Tape2PinWriter(SCHEDULER_PARAM kDatalink80Tape2Pin, LOW, INPUT_PULLUP);
#endif
#if HAVE_TECHNICS_SC
OpenDrainPinWriter technicsDataPinWriter(kTechnicsSCDataPin, LOW, INPUT_PULLUP);
OpenDrainPinWriter technicsClockPinWriter(kTechnicsSCClockPin, LOW, INPUT_PULLUP);
#endif

#if !ENABLE_IRREMOTE
PWMPinWriter irPinWriter(kIRSendPin, IR_SEND_ACTIVE);
#endif

void InsError(uint32_t error)
{
  char errorMsg[5];
  strncpy(errorMsg, (const char*)&error, 4);
  errorMsg[4] = 0;
  String errorString = "FATALERROR: ";
  errorString += errorMsg;
  logLine(errorString);
  Serial.flush();
  for(;;) yield();
}

void updateSwitches(uint64_t switchState)
{
  String logString = "UNIMPLEMENTED SWITCH: ";
  logString += uint32_t(switchState);
  logLine(logString);
}

class MainTask  :
#if HAVE_RC5 || !ENABLE_IRREMOTE
  public RxRC5::Delegate,
#endif
#if HAVE_ESI
  public RxESI::Delegate,
#endif
#if HAVE_SR || !ENABLE_IRREMOTE
  public RxNEC::Delegate,
  public RxSIRC::Delegate,
#endif
  public RxBeo36::Delegate,
  public RxDatalink86::Delegate,
#if HAVE_DATALINK80
  public RxDatalink80::Delegate,
#endif
#if HAVE_TECHNICS_SC
  public RxTechnicsSC::Delegate,
  public TxTechnicsSC::Delegate,
#endif
  public Scheduler::Delegate
{
#if HAVE_TRIGGER
  TxJam _trigger0;
  TxJam _trigger1;
#endif
#if HAVE_RC5
  TxRC5 _txRC5;
  RxRC5 _rxRC5;
#endif
#if HAVE_ESI
  TxESI _txESI;
  RxESI _rxESI;
#endif
#if HAVE_SR
  TxNEC _txNEC;
  RxNEC _rxNEC;
  TxSIRC _txSIRC;
  RxSIRC _rxSIRC;
#endif
  RxBeo36 _rxBeo36;
  RxDatalink86 _rx455;
#if HAVE_DATALINK86
  TxDatalink86 _txDatalink86;
  RxDatalink86 _rxDatalink86;
#endif
#if HAVE_DATALINK80
  TxDatalink80 _txDatalink80Tape1;
  RxDatalink80 _rxDatalink80Tape1;
  TxDatalink80 _txDatalink80Tape2;
  RxDatalink80 _rxDatalink80Tape2;
#endif
#if HAVE_TECHNICS_SC
  RxTechnicsSC _rxTechnics;
  TxTechnicsSC _txTechnics;
  std::deque<Message> _technicsSendBuffer;
#endif

#if !ENABLE_IRREMOTE
  TxRC5 _txRC5_IR;
  RxRC5 _rxRC5_IR;
  TxNEC _txNEC_IR;
  RxNEC _rxNEC_IR;
  TxSIRC _txSIRC_IR;
  RxSIRC _rxSIRC_IR;
  TxDatalink86 _tx455;
#endif

  uint64_t _switchState = 0;

  std::map<SteppedTask*, std::deque<Message>> _sendBuffer;

public:
  MainTask() :
#if HAVE_TRIGGER
    _trigger0(&trigger0PinWriter, LOW, 1),
    _trigger1(&trigger1PinWriter, LOW, 1),
#endif
#if HAVE_RC5
    _txRC5(&rc5PinWriter, HIGH),
    _rxRC5(HIGH, this, 1),
#endif
#if HAVE_ESI
    _txESI(&esiPinWriter, HIGH),
    _rxESI(HIGH, this, 1),
#endif
#if HAVE_SR
    _txNEC(&srPinWriter, LOW),
    _rxNEC(LOW, this, 1),
    _txSIRC(&srPinWriter, LOW),
    _rxSIRC(LOW, this, 1),
#endif
    _rxBeo36(LOW, this, 0),
    _rx455(LOW, this, 0)
#if HAVE_DATALINK86
    ,_txDatalink86(&datalink86PinWriter, LOW),
    _rxDatalink86(LOW, this, 1)
#endif
#if HAVE_DATALINK80
    ,_txDatalink80Tape1(&datalink80Tape1PinWriter, LOW),
    _rxDatalink80Tape1(LOW, this, 1),
    _txDatalink80Tape2(&datalink80Tape2PinWriter, LOW),
    _rxDatalink80Tape2(LOW, this, 2)
#endif
#if HAVE_TECHNICS_SC
    ,_rxTechnics(kTechnicsSCDataPin, kTechnicsSCClockPin, LOW, this),
    _txTechnics(&technicsDataPinWriter, &technicsClockPinWriter, kTechnicsSCDataPin, kTechnicsSCClockPin, LOW, this)
#endif
#if !ENABLE_IRREMOTE
    ,_txRC5_IR(&irPinWriter, IR_SEND_ACTIVE),
    _rxRC5_IR(LOW, this, 0),
    _txNEC_IR(&irPinWriter, IR_SEND_ACTIVE),
    _rxNEC_IR(LOW, this, 0),
    _txSIRC_IR(&irPinWriter, IR_SEND_ACTIVE),
    _rxSIRC_IR(LOW, this, 0),
    _tx455(&irPinWriter, IR_SEND_ACTIVE)
#endif
  {
  }

  void begin()
  {
#if HAVE_RC5
    scheduler.add(&_rxRC5, kRC5Pin, ENABLE_READ_INTERRUPTS);
#endif
#if HAVE_ESI
    scheduler.add(&_rxESI, kESIPin, ENABLE_READ_INTERRUPTS);
#endif
#if HAVE_SR
    scheduler.add(&_rxNEC, kSRPin, ENABLE_READ_INTERRUPTS);
    scheduler.add(&_rxSIRC, kSRPin, ENABLE_READ_INTERRUPTS);
#endif
    scheduler.add(&_rx455, kIR455ReceivePin, ENABLE_READ_INTERRUPTS);
    scheduler.add(&_rxBeo36, kIRReceivePin, ENABLE_READ_INTERRUPTS);
#if HAVE_DATALINK86
    scheduler.add(&_rxDatalink86, kDatalink86Pin, ENABLE_READ_INTERRUPTS);
#endif
#if HAVE_DATALINK80
    scheduler.add(&_rxDatalink80Tape1, kDatalink80Tape1Pin, ENABLE_READ_INTERRUPTS);
    scheduler.add(&_rxDatalink80Tape2, kDatalink80Tape2Pin, ENABLE_READ_INTERRUPTS);
#endif
#if HAVE_TECHNICS_SC
    // TxTechnicsRX is not a standard decoder since it uses two pins and currently does not support interrupt mode.
    scheduler.add(&_rxTechnics);
    // TxTechnicsSC must unlike other encoders always be active.
    // It should also not use absolute time as that may make the pulses too short
    scheduler.add(&_txTechnics, nullptr, false);
#endif
#if ENABLE_WRITE_TIMER
    scheduler.add(&writeScheduler);
#endif

#if !ENABLE_IRREMOTE
    scheduler.add(&_rxRC5_IR, kIRReceivePin, ENABLE_READ_INTERRUPTS);
    scheduler.add(&_rxNEC_IR, kIRReceivePin, ENABLE_READ_INTERRUPTS);
    scheduler.add(&_rxSIRC_IR, kIRReceivePin, ENABLE_READ_INTERRUPTS);
#endif

#if HAVE_TRIGGER
    Serial.print("Trigger 0 active on pin ");
    Serial.println(kTrigger0Pin);
    Serial.print("Trigger 1 active on pin ");
    Serial.println(kTrigger1Pin);
#endif
    Serial.print("IR 455 receiver active on pin ");
    Serial.println(kIR455ReceivePin);
#if HAVE_RC5
    Serial.print("RC5 bus active on pin ");
    Serial.println(kRC5Pin);
#endif
#if HAVE_ESI
    Serial.print("ESI bus active on pin ");
    Serial.println(kESIPin);
#endif
#if HAVE_SR
    Serial.print("SR bus active on pin ");
    Serial.println(kSRPin);
#endif
#if HAVE_DATALINK86
    Serial.print("Datalink 86 active on pin ");
    Serial.println(kDatalink86Pin);
#endif
#if HAVE_DATALINK80
    Serial.print("Datalink 80/1 active on pin ");
    Serial.println(kDatalink80Tape1Pin);
    Serial.print("Datalink 80/2 active on pin ");
    Serial.println(kDatalink80Tape2Pin);
#endif
#if HAVE_TECHNICS_SC
    Serial.print("Technics System Control active on pins ");
    Serial.print(kTechnicsSCDataPin);
    Serial.print(" and ");
    Serial.println(kTechnicsSCClockPin);
#endif
  }

#if HAVE_RC5 || !ENABLE_IRREMOTE
  void RxRC5Delegate_data(uint16_t data, uint8_t bus) override
  {
    Message message;
    message.value = data;
    message.protocol_name = "RC5";
    message.protocol = RC5;
    message.repeat = 0;
    message.bits = 12; // RC5X?
    message.bus = bus;
    received(message);
  }
#endif

#if HAVE_ESI
  void RxESIDelegate_data(uint64_t data, uint8_t bits, uint8_t bus) override
  {
    Message message;
    message.value = data;
    message.protocol_name = "ESI";
    message.protocol = ESI;
    message.repeat = 0;
    message.bits = bits;
    message.bus = bus;
    received(message);
  }
#endif

#if HAVE_SR || !ENABLE_IRREMOTE
  void RxNECDelegate_data(uint32_t data, uint8_t bus) override
  {
    Message message;
    message.value = data;
    message.protocol_name = "NEC";
    message.protocol = NEC;
    message.repeat = 0;
    message.bits = 32;
    message.bus = bus;
    received(message);
  }

  void RxSIRCDelegate_data(uint32_t data, uint8_t bits, uint8_t bus) override
  {
    Message message;
    message.value = data;
    message.protocol_name = "SONY";
    message.protocol = SONY;
    message.repeat = 0;
    message.bits = bits;
    message.bus = bus;
    received(message);
  }
#endif

  void RxBeo36Delegate_data(uint8_t data, uint8_t bus) override
  {
    Message message;
    message.value = data;
    message.protocol_name = "BEO36";
    message.protocol = BEO36;
    message.repeat = 0;
    message.bits = 6;
    message.bus = bus;
    received(message);
  }

  void RxDatalink86Delegate_data(uint64_t data, uint8_t bits, uint8_t bus) override
  {
    Message message;
    message.value = data;
    message.protocol_name = "DATALINK86";
    message.protocol = DATALINK86;
    message.repeat = 0;
    message.bits = bits;
    message.bus = bus;
    received(message);
  }

#if HAVE_DATALINK80
  void RxDatalink80Delegate_data(uint8_t data, uint8_t bus) override
  {
    Message message;
    message.value = data;
    message.protocol_name = "DATALINK80";
    message.protocol = DATALINK80;
    message.repeat = 0;
    message.bits = 7;
    message.bus = bus;
    received(message);
  }

  void RxDatalink80Delegate_timingError() override
  {
  }
#endif

#if HAVE_TECHNICS_SC
  void RxTechnicsSCDelegate_data(uint32_t data) override
  {
    Message message;
    message.value = data;
    message.protocol_name = "TECHNICS_SC";
    message.protocol = TECHNICS_SC;
    message.repeat = 0;
    message.bits = 32;
    message.bus = 1;
    received(message);
  }

  void TxTechnicsSCDelegate_done() override
  {
    if (!_technicsSendBuffer.size())
      return;
    Message &message = _technicsSendBuffer.front();
    send(message, true);
    if (message.repeat > 0)
    {
      --message.repeat;
    }
    else
    {
      _technicsSendBuffer.pop_front();
    }
  }
#endif

  void SchedulerDelegate_done(SteppedTask *task) override
  {
    if (_sendBuffer.count(task))
    {
      std::deque<Message> &d = _sendBuffer[task];
      if (!d.size())
        return;
      Message &message = d.front();
      send(message, true);
      if (message.repeat > 0)
      {
        --message.repeat;
      }
      else
      {
        d.pop_front();
      }
    }
  }

  void send(Message &message, bool repeatMessage = false)
  {
    SteppedTask *repeatTask = nullptr;
    bool unhandled = false;
    switch(message.protocol)
    {
    case SWITCH:
      {
        uint64_t switchMask = 1ULL << message.bus;
        _switchState &= ~switchMask;
        if (message.value)
          _switchState |= switchMask;
        updateSwitches(_switchState);
      }
      break;

    case SWITCH_TOGGLE:
      {
        uint64_t switchMask = 1ULL << message.bus;
        _switchState ^= switchMask;
        updateSwitches(_switchState);
      }
      break;

#if HAVE_TRIGGER
    case TRIGGER:
      {
        TxJam &_trigger = message.bus < 1 ? _trigger0 : _trigger1;
        if (scheduler.active(&_trigger))
        {
          _sendBuffer[&_trigger].push_back(message);
          break;
        }
        _trigger.prepare(1000 * message.value);
        txScheduler.add(&_trigger, this);
        repeatTask = &_trigger;
      }
      break;
#endif

    case RC5:
#if !ENABLE_IRREMOTE
      if (message.bus == 0)
      {
        if (scheduler.active(&_txRC5_IR))
        {
          _sendBuffer[&_txRC5_IR].push_back(message);
          break;
        }
        irPinWriter.prepare(36000, 30);
        _txRC5_IR.prepare(message.value);
        scheduler.add(&_txRC5_IR, this);
        repeatTask = &_txRC5_IR;
        break;
      }
#endif
#if HAVE_RC5
      if (scheduler.active(&_txRC5))
      {
        _sendBuffer[&_txRC5].push_back(message);
        break;
      }
      _txRC5.prepare(message.value);
#if ENABLE_WRITE_TIMER
      writeScheduler.add(&_txRC5, kRC5Pin, this);
#else
      scheduler.add(&_txRC5, this);
#endif
      repeatTask = &_txRC5;
#else
      unhandled = true;
#endif
      break;

#if HAVE_ESI
    case ESI:
      if (scheduler.active(&_txESI))
      {
        _sendBuffer[&_txESI].push_back(message);
        break;
      }
      _txESI.prepare(message.value, message.bits);
#if ENABLE_WRITE_TIMER
      writeScheduler.add(&_txESI, kESIPin, this);
#else
      scheduler.add(&_txESI, this);
#endif
      repeatTask = &_txESI;
      break;
#endif

    case NEC:
    case NEC2:
#if !ENABLE_IRREMOTE
      if (message.bus == 0)
      {
        if (scheduler.active(&_txNEC_IR))
        {
          _sendBuffer[&_txNEC_IR].push_back(message);
          break;
        }
        irPinWriter.prepare(38000, 30);
        _txNEC_IR.prepare(message.value);
        scheduler.add(&_txNEC_IR, this);
        repeatTask = &_txNEC_IR;
        break;
      }
#endif
#if HAVE_SR
      if (scheduler.active(&_txNEC))
      {
        _sendBuffer[&_txNEC].push_back(message);
        break;
      }
      _txNEC.prepare((repeatMessage && message.protocol == NEC) ? 0 : message.value);
#if ENABLE_WRITE_TIMER
      writeScheduler.add(&_txNEC, kSRPin, this);
#else
      scheduler.add(&_txNEC, this);
#endif
      repeatTask = &_txNEC;
#else
      unhandled = true;
#endif
      break;

    case SONY:
#if !ENABLE_IRREMOTE
      if (message.bus == 0)
      {
        if (scheduler.active(&_txSIRC_IR))
        {
          _sendBuffer[&_txSIRC_IR].push_back(message);
          break;
        }
        irPinWriter.prepare(40000, 30);
        _txSIRC_IR.prepare(message.value, message.bits);
        scheduler.add(&_txSIRC_IR, this);
        repeatTask = &_txSIRC_IR;
        break;
      }
#endif
#if HAVE_SR
      if (scheduler.active(&_txSIRC))
      {
        _sendBuffer[&_txSIRC].push_back(message);
        break;
      }
      _txSIRC.prepare(message.value, message.bits);
#if ENABLE_WRITE_TIMER
      writeScheduler.add(&_txSIRC, kSRPin, this);
#else
      scheduler.add(&_txSIRC, this);
#endif
      repeatTask = &_txSIRC;
#else
      unhandled = true; 
#endif
      break;

    case DATALINK86:
#if !ENABLE_IRREMOTE
      if (message.bus == 0)
      {
        if (scheduler.active(&_tx455))
        {
          _sendBuffer[&_tx455].push_back(message);
          break;
        }
        irPinWriter.prepare(455000, 30);
        _tx455.prepare(message.value, message.bits, true, repeatMessage);
        scheduler.add(&_tx455, this);
        repeatTask = &_tx455;
        break;
      }
#endif
#if HAVE_DATALINK86
      if (scheduler.active(&_txDatalink86))
      {
        _sendBuffer[&_txDatalink86].push_back(message);
        break;
      }
      _txDatalink86.prepare(message.value, message.bits, false, repeatMessage);
#if ENABLE_WRITE_TIMER
      writeScheduler.add(&_txDatalink86, kDatalink86Pin, this);
#else
      scheduler.add(&_txDatalink86, this);
#endif
      repeatTask = &_txDatalink86;
#else
      unhandled = true;
#endif
      break;

#if HAVE_DATALINK80
    case DATALINK80:
      {
        TxDatalink80 &_txDatalink80 = message.bus < 2 ? _txDatalink80Tape1 : _txDatalink80Tape2;
        if (scheduler.active(&_txDatalink80))
        {
          _sendBuffer[&_txDatalink80].push_back(message);
          break;
        }
        _txDatalink80.prepare(message.value);
#if ENABLE_WRITE_TIMER
        writeScheduler.add(&_txDatalink80, message.bus < 2 ? kDatalink80Pin1 : kDatalink80Pin2, this);
#else
        scheduler.add(&_txDatalink80, this);
#endif
        repeatTask = &_txDatalink80;
      }
      break;
#endif

#if HAVE_TECHNICS_SC
    case TECHNICS_SC:
      {
        if (!_txTechnics.done())
        {
          message.protocol_name = nullptr;
          _technicsSendBuffer.push_back(message);
          break;
        }
        _txTechnics.prepare(message.value);
        if (message.repeat > 0)
        {
          --message.repeat;
          message.protocol_name = nullptr;
          _technicsSendBuffer.push_back(message);
        }
      }
      break;
#endif

    case BEO36:
      // Only receive is implemeted for BEO36.
    default:
      unhandled = true;
    }
    if (unhandled)
    {
      logLine(message.bus ? "UNHANDLED IC" : "UNHANDLED IR");
      return;
    }
    if (!repeatMessage && repeatTask && message.repeat > 0)
    {
      --message.repeat;
      message.protocol_name = nullptr;
      _sendBuffer[repeatTask].push_back(message);
    }
  }
};

MainTask mainTask;

void sendInseparates(Message &message)
{
  mainTask.send(message);
}

void setupInseparates()
{
  scheduler.begin();
  scheduler.add(&printer);
  mainTask.begin();
}

void loopInseparates()
{
  ins_micros_t now = fastMicros();

#if DEBUG_FULL_TIMING
  TimeInserter tInserter(tAcc, now);
#endif
#if DEBUG_CYCLE_TIMING
  cCheck.tick(now);
#endif

  scheduler.poll();

  //if (timekeeper.microsSinceReset(now) < 5000000L)
  if (timekeeper.microsSinceReset(now) < 1000000L)
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
