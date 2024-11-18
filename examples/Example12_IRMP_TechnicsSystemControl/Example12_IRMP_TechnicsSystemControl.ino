// Copyright (c) 2024 Daniel Wallner

// Partially emulates the IR translation done by a Technics ST-X302L/ST-X902L
// See notes in ProtocolTechnicsSC.h
// Receive done by IRMP in pin change interrupt mode.

// INS_FAST_TIME is not compatible with IRMP
#define DEBUG_FULL_TIMING 1
#define DEBUG_CYCLE_TIMING 1

#define ENABLE_SC_DECODER 1 // Enable for debugging and reverse engineering

#include <Arduino.h>

#define IRMP_ENABLE_PIN_CHANGE_INTERRUPT
#define IRMP_SUPPORT_NEC_PROTOCOL 1      // Pioneer + Yamaha
#define IRMP_SUPPORT_KASEIKYO_PROTOCOL 1 // Technics
#define IRMP_INPUT_PIN D2

const uint16_t kTechnicsSCDataPin = D5;
const uint16_t kTechnicsSCClockPin = D4;

#include <irmp.hpp>

#include <Inseparates.h>
#include <DebugUtils.h>

#include <ProtocolUtils.h>
#include <ProtocolTechnicsSC.h>

#define DEC_TECHNICS 1
#define DEC_YAMAHA 1
#define DEC_PIONEER 1

// Map IRremote to IRMP
#define RC5 IRMP_RC5_PROTOCOL
#define RC6 IRMP_RC6_PROTOCOL
#define DENON IRMP_DENON_PROTOCOL
#define NEC IRMP_NEC_PROTOCOL
#define SONY IRMP_SIRCS_PROTOCOL
#define PANASONIC IRMP_PANASONIC_PROTOCOL

#include <extras/DecodeIR.h>

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

OpenDrainPinWriter tscDataWriter(kTechnicsSCDataPin, LOW, INPUT_PULLUP);
OpenDrainPinWriter tscClockWriter(kTechnicsSCClockPin, LOW, INPUT_PULLUP);

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
class MainTask  : public SteppedTask
#if ENABLE_SC_DECODER
  , public RxTechnicsSC::Delegate
#endif
{
#if ENABLE_SC_DECODER
  RxTechnicsSC _rxTechnicsSC;
#endif
  TxTechnicsSC _txTechnicsSC;
  IRMP_DATA _irmp_data;

public:
  MainTask() :
#if ENABLE_SC_DECODER
    _rxTechnicsSC(kTechnicsSCDataPin, kTechnicsSCClockPin, LOW, this),
#endif
    _txTechnicsSC(&tscDataWriter, &tscClockWriter, kTechnicsSCDataPin, kTechnicsSCClockPin, LOW)
  {
  }

  void begin()
  {
#if ENABLE_SC_DECODER
    scheduler.add(&_rxTechnicsSC);
#endif
    // TxTechnicsSC must unlike other encoders always be active.
    scheduler.add(&_txTechnicsSC);
    scheduler.add(this);
  }

#if ENABLE_SC_DECODER
  void RxTechnicsSCDelegate_data(uint32_t data) override
  {
    printer.printf("Technics SC: %lX\n", (long)data);
  }
#endif

  uint16_t SteppedTask_step() override
  {
    if (irmp_get_data(&_irmp_data))
    {
      if (!_txTechnicsSC.done())
      {
        return 5000;
      }
      button_type_t button;
      if (decode_ir(_irmp_data.protocol, 0, _irmp_data.address, _irmp_data.command, 0, &button))
      {
        uint32_t message = 0;
        switch(button)
        {
        case VOLUME_UP: message = 0x00200001; break;
        case VOLUME_DOWN: message = 0x00210001; break;
        case VOLUME_MUTE: message = 0x00320001; break;
        //case SOURCE_PHONO: message = 0; break;
        case SOURCE_CD: message = 0x00940001; break;
        case SOURCE_TUNER: message = 0x00920001; break;
        case SOURCE_TAPE1: message = 0x00960001; break;
        //case SOURCE_VCR: message = 0; break;
        case CD_NEXT: message = 0x0A4A0001; break;
        case CD_PREV: message = 0x0A490001; break;
        case CD_PLAY: message = 0x0A0A0001; break;
        case CD_PAUSE: message = 0x0A060001; break;
        case CD_STOP: message = 0x0A000001; break;
        case CD_PGM: message = 0x0A8A0001; break;
        case TAPE_DECK_A_B: message = 0x08950001; break;
        case TAPE_PAUSE_A: message = 0x08060001; break;
        case TAPE_REV_A: message = 0x080B0001; break;
        case TAPE_STOP_A: message = 0x08000001; break;
        case TAPE_PLAY_A: message = 0x080A0001; break;
        case TAPE_REW_A: message = 0x08020001; break;
        case TAPE_FFWD_A: message = 0x08030001; break;
        case TAPE_REC_MUTE_B: message = 0x08820001; break;
        case PHONO_START: message = 0x0E0A0001; break;
        case PHONO_STOP: message = 0x0E000001; break;
        }
        printer.printf("IR: %ld %ld %lX\n", (int)_irmp_data.address, (int)_irmp_data.command, message);
        if (message)
        {
          _txTechnicsSC.prepare(message);
        }
      }
    }
    return 5000;
  }
};

MainTask mainTask;

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    delay(50);

  Serial.print("IR input pin: " INS_STR(IRMP_INPUT_PIN));
  Serial.print("Technics SC data pin: ");
  Serial.println(kTechnicsSCDataPin);
  Serial.print("Technics SC clock pin: ");
  Serial.println(kTechnicsSCClockPin);

  irmp_init();
  Serial.print(F("Enabled IR receive protocols: "));
  irmp_print_active_protocols(&Serial);

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
  if (timekeeper.microsSinceReset(now) < 5000000UL)
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
