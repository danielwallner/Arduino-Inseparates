// Copyright (c) 2024 Daniel Wallner

// Implements the IR protocols except for B&O 455 kHz decoding

#define DECODE_NEC 1
#define SEND_NEC 1
#define DECODE_RC5 1
#define SEND_RC5 1
#define DECODE_RC6 1
#define SEND_RC6 1
#define DECODE_RCMM 1
#define SEND_RCMM 1
#define DECODE_SONY 1
#define SEND_SONY 1
#define DECODE_PANASONIC 1
#define SEND_PANASONIC 1
#define DECODE_JVC 1
#define SEND_JVC 1
#define DECODE_SAMSUNG 1
#define SEND_SAMSUNG 1
#define DECODE_SAMSUNG36 1
#define SEND_SAMSUNG36 1
#define DECODE_LG 1
#define SEND_LG 1
#define DECODE_SANYO 1
#define SEND_SANYO 1
#define DECODE_MITSUBISHI 1
#define SEND_MITSUBISHI 1
#define DECODE_SHARP 1
#define SEND_SHARP 1
#define DECODE_DENON 1
#define SEND_DENON 1
#define DECODE_PIONEER 1
#define SEND_PIONEER 1
#define DECODE_BOSE 1
#define SEND_BOSE 1
#define DECODE_BANG_OLUFSEN 1
#define SEND_BANG_OLUFSEN 1

enum ins_decode_type_t {
  SWITCH = 256,
  SWITCH_TOGGLE,
  TRIGGER,
  NEC2, // NEC with full message repeat
  ESI,
  BEO36,
  DATALINK80,
  DATALINK86,
  TECHNICS_SC,
};

#if REV_A
const uint16_t kIRReceivePin = D_2;
const uint16_t kIRSendPin = D_3;
#else
const uint16_t kIRReceivePin = D_3;
const uint16_t kIRSendPin = D_9;
#endif

#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <IRsend.h>

#include <FastTime.h>

#include <map>

IRrecv irrecv(kIRReceivePin);
IRsend irsend(kIRSendPin, true);

struct Message
{
  uint64_t value;
  const char *protocol_name;
  int16_t protocol;
  uint8_t repeat;
  uint8_t bits;
  uint8_t bus;
};

void sendInseparates(Message &message);
void publish(Message &message);

void received(Message &message)
{
  if (!message.protocol_name && message.protocol > 0 && message.protocol < SWITCH)
  {
    String protocol_name = typeToString((decode_type_t)message.protocol, message.repeat);
    message.protocol_name = protocol_name.c_str();
  }
  publish(message);
}

void dispatch(Message &message)
{
  if (message.protocol <= 1 && message.protocol_name)
  {
    decode_type_t protocol = strToDecodeType(message.protocol_name);
    if (protocol != UNKNOWN)
    {
      message.protocol = protocol;
    }
    else
    {
      if (!strcmp(message.protocol_name, "SWITCH")) message.protocol = SWITCH;
      else if (!strcmp(message.protocol_name, "SWITCH_TOGGLE")) message.protocol = SWITCH_TOGGLE;
      else if (!strcmp(message.protocol_name, "TRIGGER")) message.protocol = TRIGGER;
      else if (!strcmp(message.protocol_name, "NEC2")) message.protocol = NEC2;
      else if (!strcmp(message.protocol_name, "ESI")) message.protocol = ESI;
      else if (!strcmp(message.protocol_name, "BEO36")) message.protocol = BEO36;
      else if (!strcmp(message.protocol_name, "DATALINK80")) message.protocol = DATALINK80;
      else if (!strcmp(message.protocol_name, "DATALINK86")) message.protocol = DATALINK86;
      else if (!strcmp(message.protocol_name, "TECHNICS_SC")) message.protocol = TECHNICS_SC;
    }
  }

  if (message.bus > 0 || message.protocol >= SWITCH)
  {
    sendInseparates(message);
  }
  else
  {
    bool result = irsend.send((decode_type_t)message.protocol, message.value, message.bits, message.repeat);
    if (!result)
      logLine("UNHANDLED IR");
  }
}

inseparates::Timekeeper16 irTimekeeper;

void setupIR()
{
  irrecv.enableIRIn();
  irTimekeeper.reset();

  irsend.begin();

  Serial.print("IR receiver active on pin ");
  Serial.println(kIRReceivePin);

  Serial.print("IR transmitter active on pin ");
  Serial.println(kIRSendPin);
}

decode_results results;

void loopIR()
{
  if (irTimekeeper.microsSinceReset() > 1000)
  {
    if (irrecv.decode(&results))
    {
      if (results.decode_type <= 0)
      {
        irrecv.resume();
      }
      String protocol_name = typeToString(results.decode_type);
      Message message;
      message.value = results.value;
      message.repeat = results.repeat;
      message.bus = 0;
      message.bits = results.bits;
      message.protocol = results.decode_type;
      message.protocol_name = protocol_name.c_str();

      irrecv.resume();

      received(message);
    }
  }
}
