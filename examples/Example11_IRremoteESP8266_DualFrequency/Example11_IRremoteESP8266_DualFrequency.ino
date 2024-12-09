// Copyright (c) 2024 Daniel Wallner

// IR command translator

// Receive on one pin with IRremoteESP8266 and on one with Inseparates
// Send translated data on one pin for each message.
// This means that the sent messages may have an incorrect repeat rate.

// A counterfeit 455kHz TSOP7000 cannot share the same input pin with another IR receiver.
// The TSOP7000 will contaminate the data as it also reacts to lower frequencies.
// This is solved here by using two separate decoders on two different pins.

#define INS_FAST_TIME 1
#define DEBUG_FULL_TIMING 0
#define DEBUG_CYCLE_TIMING 0

#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>

#include <Inseparates.h>
#include <DebugUtils.h>
#include <ProtocolUtils.h>

#include <ProtocolDatalink86.h>
#include <ProtocolNEC.h>
#include <ProtocolRC5.h>
#include <ProtocolSIRC.h>

#define DEC_YAMAHA
#define DEC_SONY
#define DEC_TECHNICS
#define DEC_SANSUI
#define DEC_PIONEER
#define DEC_NAD
#define DEC_HARMAN
#define DEC_PHILIPS
#define DEC_DENON
#define DEC_BEO

#define BANG_OLUFSEN 100

#define ENC_PIONEER_TAPE
//#define ENC_DENON_TAPE
#define ENC_SONY_TUNER
#define ENC_PHILIPS_CD
//#define ENC_BEOSYSTEM

#if 0
#define INS_SEND_BEO_DATALINK(data,repeat) IrSender.sendBangOlufsenRawDataLink(data, 16, repeat, false)
#define INS_SEND_RC5(address,command,repeat) IrSender.sendRC5ext(address, command, !repeat)
#define INS_SEND_SIRC(address,command,bits) IrSender.sendSony(address, command, bits);
#define INS_SEND_DENON(address,command) IrSender.sendDenon(address, command, 0);
#define INS_SEND_NEC(address,command) IrSender.sendNEC(address, command, 0)
#define INS_SEND_NEC_REPEAT IrSender.sendNECRepeat
#else
#define INS_SEND_BEO_DATALINK(data,repeat) IrSender.sendBangOlufsenRawDataLink(data, 16, repeat, false)
#define INS_SEND_RC5(address,command,repeat) do { static uint8_t toggle; if (!repeat) toggle ^= 1; txRC5.prepare(TxRC5::encodeRC5(toggle, address, command), false); scheduler.add(&txRC5); } while(0)
#define INS_SEND_SIRC(address,command,bits) do { txSIRC.prepare(TxSIRC::encodeSIRC(address,command), bits, false); scheduler.add(&txSIRC); } while(0)
#define INS_SEND_DENON(address,command) IrSender.sendDenon(address, command);
#define INS_SEND_NEC(address,command) do { txNEC.prepare(TxNEC::encodeNEC(address, command)); scheduler.add(&txNEC); } while(0)
#define INS_SEND_NEC_REPEAT() do { txNEC.prepare(0, false); scheduler.add(&txNEC); } while(0)
#endif

#if defined(ESP8266) // WEMOS D1 R2
static const uint8_t D_2  = 16;
static const uint8_t D_3  = 5;
static const uint8_t D_4  = 4;
static const uint8_t D_7  = 14;
static const uint8_t D_9  = 13;
#elif defined(ESP32) // WEMOS D1 R32
static const uint8_t D_2  = 26;
static const uint8_t D_3  = 25;
static const uint8_t D_4  = 17;
static const uint8_t D_7  = 14;
static const uint8_t D_9  = 13;
#else
static const uint8_t D_2  = 2;
static const uint8_t D_3  = 3;
static const uint8_t D_4  = 4;
static const uint8_t D_7  = 7;
static const uint8_t D_9  = 9;
#endif

const uint16_t kMainReceivePin = D_3;
const uint16_t kDatalink86RecvPin = D_7;
const uint16_t kIRSendPin = D_9;

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

#define ACTIVE LOW

#if HW_PWM
PWMPinWriter irPinWriter(kIRSendPin, ACTIVE);
#elif SW_PWM
SoftPWMPinWriter irPinWriter(kIRSendPin, ACTIVE);
#else
PushPullPinWriter irPinWriter(kIRSendPin);
#endif

TxRC5 txRC5(&irPinWriter, LOW);
TxNEC txNEC(&irPinWriter, LOW);
TxSIRC txSIRC(&irPinWriter, LOW);

#include <extras/DecodeIR.h>
#include <extras/EncodeIR.h>

#if !defined(D2) && defined(PD2)
#define D2 PD2
#endif
#if !defined(D3) && defined(PD3)
#define D3 PD3
#endif
#if !defined(D6) && defined(PD6)
#define D6 PD6
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

// For IRremoteESP8266
IRrecv irrecv(kMainReceivePin);
decode_results results;

class Delegate :
  public SteppedTask,
  public RxDatalink86::Delegate
{
public:

  void RxDatalink86Delegate_data(uint64_t data, uint8_t bits, uint8_t bus) override
  {
    if (bits != 16)
      return;
    printer.print("Datalink86: ");
    printer.println(String((uint32_t)data, HEX));

    button_type_t button;
    decode_ir(BANG_OLUFSEN, 16, data >> 8, data, 0, &button);
    send_ir(button, 0 /* INS_FLAG_REPEAT */);
  }

  uint16_t SteppedTask_step() override
  {
    if (irrecv.decode(&results))
    {
      printer.print("Datalink86: ");
      Serial.println("");

      button_type_t button;
      decode_ir(results.decode_type, results.bits, results.address, results.command, results.value, &button);
      send_ir(button, results.repeat ? INS_FLAG_REPEAT : 0);
      irrecv.resume();
    }
    return 1000;
  }
};

Delegate delegate;

RxDatalink86 datalink86Decoder(LOW, &delegate);

void setup()
{
  Serial.begin(115200);

  while (!Serial)
    delay(50);

  irrecv.enableIRIn();

  Serial.print("Main IR input pin: ");
  Serial.println(kMainReceivePin);
  Serial.print("Datalink 86 input pin: ");
  Serial.println(kDatalink86RecvPin);
  Serial.print("IR output pin: ");
  Serial.println(kIRSendPin);
  Serial.flush();

  scheduler.begin();
  scheduler.add(&printer);

  scheduler.add(&datalink86Decoder, kDatalink86RecvPin);
}

void loop()
{
  ins_micros_t now = fastMicros();

#if DEBUG_FULL_TIMING
  TimeInserter tInserter(tAcc, now);
#endif
#if DEBUG_CYCLE_TIMING
  cCheck.tick(now);
#endif

  scheduler.poll();

  if (timekeeper.microsSinceReset(now) < 5000000L)
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
