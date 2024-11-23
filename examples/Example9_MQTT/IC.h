// Copyright (c) 2024 Daniel Wallner

// Implements the interconnect protocols and B&O 36/455 kHz IR decoding

#define ENABLE_READ_INTERRUPTS 0
#define ENABLE_WRITE_INTERRUPTS 0 // Not yet stable enough to use

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
#define HAVE_DATALINK86 1
const uint16_t kDatalink86Pin = D_12;
#if !HAVE_RC5
#define HAVE_DATALINK80 1
const uint16_t kDatalink80Tape1Pin = D_8;
const uint16_t kDatalink80Tape2Pin = D_9;
#endif
#if !HAVE_TRIGGER
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
#if !HAVE_ESI
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

#if ENABLE_READ_INTERRUPTS
InterruptScheduler scheduler;
#else
Scheduler scheduler;
#endif
#if ENABLE_WRITE_INTERRUPTS
InterruptWriteScheduler writeScheduler(30, 10000);
#endif

#if DEBUG_FULL_TIMING
TimeAccumulator tAcc;
#endif
#if DEBUG_CYCLE_TIMING
CycleChecker cCheck;
#endif

#if ENABLE_WRITE_INTERRUPTS
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
#if HAVE_RC5
  public RxRC5::Delegate,
#endif
#if HAVE_ESI
  public RxESI::Delegate,
#endif
#if HAVE_SR
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
    _rxRC5(HIGH, this),
#endif
#if HAVE_ESI
    _txESI(&esiPinWriter, HIGH),
    _rxESI(HIGH, this),
#endif
#if HAVE_SR
    _txNEC(&srPinWriter, LOW),
    _rxNEC(LOW, this),
    _txSIRC(&srPinWriter, LOW),
    _rxSIRC(LOW, this),
#endif
    _rxBeo36(LOW, this),
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
    _txTechnics(&technicsDataPinWriter, &technicsClockPinWriter, kTechnicsSCDataPin, kTechnicsSCClockPin, LOW)
#endif
  {
  }

  void begin()
  {
#if HAVE_RC5
    scheduler.add(&_rxRC5, kRC5Pin);
#endif
#if HAVE_ESI
    scheduler.add(&_rxESI, kESIPin);
#endif
#if HAVE_SR
    scheduler.add(&_rxNEC, kSRPin);
    scheduler.add(&_rxSIRC, kSRPin);
#endif
    scheduler.add(&_rx455, kIR455ReceivePin);
    scheduler.add(&_rxBeo36, kIRReceivePin);
#if HAVE_DATALINK86
    scheduler.add(&_rxDatalink86, kDatalink86Pin);
#endif
#if HAVE_DATALINK80
    scheduler.add(&_rxDatalink80Tape1, kDatalink80Tape1Pin);
    scheduler.add(&_rxDatalink80Tape2, kDatalink80Tape2Pin);
#endif
#if HAVE_TECHNICS_SC
    scheduler.add(&_rxTechnics);
    // TxTechnicsSC must unlike other encoders always be active.
    scheduler.add(&_txTechnics);
#endif
#if ENABLE_WRITE_INTERRUPTS
    scheduler.add(&writeScheduler);
#endif

#if HAVE_TRIGGER
    Serial.print("Trigger 0 active on pin ");
    Serial.println(kTrigger0Pin);
    Serial.print("Trigger 1 active on pin ");
    Serial.println(kTrigger1Pin);
#endif
    Serial.print("IR 455 receiver active on pin ");
    Serial.println(kIR455ReceivePin);
    Serial.print("RC5 bus active on pin ");
    Serial.println(kRC5Pin);
    Serial.print("ESI bus active on pin ");
    Serial.println(kESIPin);
    Serial.print("IR bus active on pin ");
    Serial.println(kSRPin);
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

#if HAVE_RC5
  void RxRC5Delegate_data(uint16_t data) override
  {
    Message message;
    message.value = data;
    message.protocol_name = "RC5";
    message.protocol = RC5;
    message.repeat = 0;
    message.bits = 12; // RC5X?
    message.bus = 1;
    received(message);
  }
#endif

#if HAVE_ESI
  void RxESIDelegate_data(uint64_t data, uint8_t bits) override
  {
    Message message;
    message.value = data;
    message.protocol_name = "ESI";
    message.protocol = ESI;
    message.repeat = 0;
    message.bits = bits;
    message.bus = 1;
    received(message);
  }
#endif

#if HAVE_SR
  void RxNECDelegate_data(uint32_t data) override
  {
    Message message;
    message.value = data;
    message.protocol_name = "NEC";
    message.protocol = NEC;
    message.repeat = 0;
    message.bits = 32;
    message.bus = 1;
    received(message);
  }

  void RxSIRCDelegate_data(uint32_t data, uint8_t bits) override
  {
    Message message;
    message.value = data;
    message.protocol_name = "SONY";
    message.protocol = SONY;
    message.repeat = 0;
    message.bits = bits;
    message.bus = 1;
    received(message);
  }
#endif

  void RxBeo36Delegate_data(uint8_t data) override
  {
    Message message;
    message.value = data;
    message.protocol_name = "BEO36";
    message.protocol = BEO36;
    message.repeat = 0;
    message.bits = 6;
    message.bus = 1;
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
        scheduler.add(&_trigger, this);
        repeatTask = &_trigger;
      }
      break;
#endif

#if HAVE_RC5
    case RC5:
      if (scheduler.active(&_txRC5))
      {
        _sendBuffer[&_txRC5].push_back(message); 
        break;
      }
      _txRC5.prepare(message.value);
      scheduler.add(&_txRC5, this);
      repeatTask = &_txRC5;
      break;
#endif

#if HAVE_ESI
    case ESI:
      if (scheduler.active(&_txESI))
      {
        _sendBuffer[&_txESI].push_back(message); 
        break;
      }
      _txESI.prepare(message.value, message.bits);
      scheduler.add(&_txESI, this);
      repeatTask = &_txESI;
      break;
#endif

#if HAVE_SR
    case NEC:
    case NEC2:
      if (scheduler.active(&_txNEC))
      {
        _sendBuffer[&_txNEC].push_back(message); 
        break;
      }
      _txNEC.prepare((repeatMessage && message.protocol == NEC) ? 0 : message.value);
      scheduler.add(&_txNEC, this);
      repeatTask = &_txNEC;
      break;

    case SONY:
      if (scheduler.active(&_txSIRC))
      {
        _sendBuffer[&_txSIRC].push_back(message); 
        break;
      }
      _txSIRC.prepare(message.value, message.bits);
      scheduler.add(&_txSIRC, this);
      repeatTask = &_txSIRC;
      break;
#endif

#if HAVE_DATALINK86
    case DATALINK86:
      {
        if (scheduler.active(&_txDatalink86))
        {
          _sendBuffer[&_txDatalink86].push_back(message); 
          break;
        }
        _txDatalink86.prepare(message.value, message.bits, false, repeatMessage);
        scheduler.add(&_txDatalink86, this);
        repeatTask = &_txDatalink86;
      }
      break;
#endif

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
        scheduler.add(&_txDatalink80, this);
        repeatTask = &_txDatalink80;
      }
      break;
#endif

#if HAVE_TECHNICS_SC
    case TECHNICS_SC:
      {
        if (!_txTechnics.done())
        {
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
      // Not handled since that would require an additional IR send pin!
    default:
      logLine("UNHANDLED IC");
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
  mainTask.begin();
}

void loopInseparates()
{
  scheduler.poll();
}
