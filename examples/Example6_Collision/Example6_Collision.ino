// Copyright (c) 2024 Daniel Wallner

// Send with collision detection.
// kESIPin and kJamPin must be connected to generate collisions.

// DECODE_COLLISION_DETECTION determines if collision detection is done with the CheckingPinWriter
// or by comparing the sent message with the decoded message from the output pin.
#define DECODE_COLLISION_DETECTION 1

#define INS_FAST_TIME 0

#include <Inseparates.h>
#include <DebugUtils.h>

#include <ProtocolUtils.h>
#include <ProtocolESI.h>

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
// On boards without INPUT_PULLDOWN an external pulldown must be connected.
#define INPUT_PULLDOWN INPUT
#endif

const uint16_t kESIPin = D_4;
const uint16_t kJamPin = D_5;

#define ACTIVE HIGH
#define MIN_SPACE_AFTER_SEND_MICROS 20000
#define JAM_LENGTH_MICROS 50000
#define STEP_SLEEP_MICROS 100
#define SEND_WAIT_MICROS 50000

using namespace inseparates;

DebugPrinter printer;
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

class MainTask  : public SteppedTask, public RxESI::Delegate, public CheckingPinWriter::Delegate, public Scheduler::Delegate
{
#if DECODE_COLLISION_DETECTION
  OpenDrainPinWriter _pinWriter;
#else
  CheckingPinWriter _pinWriter;
#endif
  TxESI _txESI;
  RxESI _esiDecoder;
  OpenDrainPinWriter _jamWriter;
  TxJam _jammer;
  Timekeeper _jamTimekeeper;
  Timekeeper16 _resendTimekeeper;
  Timekeeper16 _spaceTimekeeper;
  uint32_t _nextJamMicros;

  uint8_t _toggle = 1; // Must be 1 to trigger start as _encodedMessage is used as a flag.
  uint8_t _address = 0;
  uint8_t _command = 0;
  uint32_t _encodedMessage = 0;
  uint32_t _receivedMessage = 0;

public:
  MainTask() :
#if DECODE_COLLISION_DETECTION
    _pinWriter(kESIPin, ACTIVE, INPUT_PULLDOWN),
#else
    _pinWriter(kESIPin, 10, this, ACTIVE, INPUT_PULLDOWN),
#endif
    _txESI(&_pinWriter, ACTIVE),
    _esiDecoder(ACTIVE, this),
    _jamWriter(kJamPin, ACTIVE),
    _jammer(&_jamWriter, ACTIVE, JAM_LENGTH_MICROS)
  {
  }

  void begin()
  {
#if !DECODE_COLLISION_DETECTION
    scheduler.add(&_pinWriter);
#endif
    scheduler.add(&_esiDecoder, kESIPin);
    scheduler.add(this);
  }

  void RxESIDelegate_data(uint64_t data, uint8_t bits, uint8_t bus) override
  {
    _receivedMessage = data;
    printer.printf("ESI data: %0lx%0lx bits: %hhu\n", uint32_t(data >> 32),  uint32_t(data), bits);
  }

  void CheckingPinWriterDelegate_error(uint8_t pin) override
  {
    if (scheduler.active(&_txESI))
    {
      // Prevent triggering of done.
      scheduler.remove(&_txESI);
      printer.println("Collision in message");
    }
    else
    {
      printer.printf("Collision in space after %ld us\n", (long)_spaceTimekeeper.microsSinceReset());
    }
  }

  void SchedulerDelegate_done(SteppedTask *task) override
  {
    if (task == &_txESI)
    {
      // Send done.
      // This starts a counter to disable collision detection later.
      // Doing it here already is not safe.
      _spaceTimekeeper.reset();
      return;
    }

    if (task == &_jammer)
    {
      // Jam done.
      // Schedule next.
      _nextJamMicros = random(5, 500) * 1000L;
      _jamTimekeeper.reset();
      return;
    }
  }

  uint16_t SteppedTask_step() override
  {
    if (!scheduler.active(&_jammer) && _jamTimekeeper.microsSinceReset() > _nextJamMicros)
    {
      printer.println("Jam");
      scheduler.add(&_jammer, this);
    }

    if (digitalRead(kESIPin) == ACTIVE)
    {
      _resendTimekeeper.reset();
    }

    if (scheduler.active(&_txESI))
    {
      return STEP_SLEEP_MICROS;
    }

#if DECODE_COLLISION_DETECTION
    while (_encodedMessage)
#else
    if (_pinWriter.enabled())
#endif
    {
      if (_spaceTimekeeper.microsSinceReset() < MIN_SPACE_AFTER_SEND_MICROS)
      {
        return STEP_SLEEP_MICROS;
      }
#if DECODE_COLLISION_DETECTION
      if (_receivedMessage != _encodedMessage)
      {
        printer.println("Collision");
        // Resend same data.
        break;
      }
      _encodedMessage = 0;
#else
      // Pause collision detection.
      _pinWriter.disable();

      if (_receivedMessage != _encodedMessage)
      {
        printer.println("ERROR: Data corruption!");
        printer.flush();
        for(;;) yield();
      }
#endif

      // Update data.
      _toggle ^= 1;
      ++_command;
      if (_command & 0x80)
      {
        _command = 0;
        ++_address;
      }
    }

    if (_resendTimekeeper.microsSinceReset() > SEND_WAIT_MICROS)
    {
      _resendTimekeeper.reset();
      send();
    }

    return STEP_SLEEP_MICROS;
  }

  void send()
  {
      printer.println("Send");
      // Calling flush() is not really advisable normally as this will block.
      // Used here to force the order of messages.
      printer.flush();
#if DECODE_COLLISION_DETECTION
      _receivedMessage = 0;
#else
      // Enable collision detection.
      _pinWriter.enable();
#endif
      _encodedMessage = TxESI::encodeRC5(0, _toggle, _address, _command);
      // Set sleepUntilRepeat to false here.
      // Otherwise the transmitter will sleep and not finish until long after MIN_SPACE_AFTER_SEND_MILLIS
      // which would mean that we would check for collision for too long.
      _txESI.prepare(_encodedMessage, false);
      scheduler.add(&_txESI, this);
  }
};

MainTask mainTask;

void setup()
{
  Serial.begin(115200);

  while (!Serial)
    delay(50);

  Serial.print("ESI pin: ");
  Serial.println(kESIPin);
  Serial.print("Jam pin: ");
  Serial.println(kJamPin);
  Serial.flush();

  scheduler.begin();
  scheduler.add(&printer);
  mainTask.begin();
}

void loop()
{
  scheduler.poll();
}
