// Copyright (c) 2024 Daniel Wallner

// Implements the IR protocols except for B&O 455 kHz decoding

#define IR_QUEUE_LENGTH 128
#define FILTER_UNKNOWN_IR 0

#define DECODE_NEC 1
#define SEND_NEC 1
#define DECODE_RC5 1
#define SEND_RC5 1
#define DECODE_RC6 1
#define SEND_RC6 1
#define DECODE_SONY 1
#define SEND_SONY 1
#define DECODE_PANASONIC 1
#define SEND_PANASONIC 1
#define DECODE_JVC 1
#define SEND_JVC 1
#define DECODE_SAMSUNG 1
#define SEND_SAMSUNG 1
#define DECODE_LG 1
#define SEND_LG 1
#define DECODE_SHARP 1
#define SEND_SHARP 1
#define DECODE_DENON 1
#define SEND_DENON 1
#define DECODE_PIONEER 1
#define SEND_PIONEER 1
#define DECODE_BANG_OLUFSEN 1
#define SEND_BANG_OLUFSEN 1

enum ins_decode_type_t {
  ESI = 253,
  BEO36,
  DATALINK86,
  SWITCH = 256,
  SWITCH_TOGGLE,
  TRIGGER,
  NEC2, // NEC with full message repeat
  DATALINK80,
  TECHNICS_SC,
};

#include <Inseparates.h>

#include <FastTime.h>

#include <map>

struct Message
{
  uint64_t value;
  const char *protocol_name;
  int16_t protocol;
  uint8_t repeat;
  uint8_t bits;
  uint8_t bus;
  uint8_t flags;
  uint8_t logTarget;

  void setValue(uint64_t value_) { value = value_;  flags |= 1; }
  void setAddress(uint16_t address) { value |= (uint32_t(address) << 16); flags |= 2; }
  void setCommand(uint16_t command) { value |= command; flags |= 2; }
  void setExtended(uint16_t extended) { value |= (uint64_t(extended) << 32); flags |= 4; }
  void makeDummy() { flags = 0; }

  uint16_t address() const { return value >> 16; }
  uint16_t command() const { return value & 0xffff; }
  uint16_t extended() const { return value >> 32; }

  bool addressAndCommandSet() const  { return flags & 2; }
  bool extendedSet() const { return flags & 4; }
  bool dummy() const { return !flags; }
};

#if ENABLE_IRREMOTE
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <IRsend.h>

IRrecv irrecv(kIRReceivePin);
IRsend irsend(kIRSendPin, IR_SEND_ACTIVE == LOW);
decode_results results;
inseparates::LockFreeFIFO<Message, 128> irFIFO;
#endif

void sendInseparates(Message &message);
void publish(Message &message);

#if !ENABLE_IRREMOTE
enum decode_type_t
{
  UNKNOWN = -1,
  UNUSED = 0,
  RC5,
  NEC = 3,
  SONY,
};
String typeToString(const decode_type_t protocol, const bool isRepeat = false)
{
  switch (protocol)
  {
  case RC5:
    return "RC5";
  case NEC:
    return "NEC";
  case SONY:
    return "SONY";
  }
  return "UNKNOWN";
}

decode_type_t strToDecodeType(const char *name)
{
  if (!strcmp(name, "RC5")) return RC5;
  else if (!strcmp(name, "NEC")) return NEC;
  else if (!strcmp(name, "SONY")) return SONY;
  return UNKNOWN;
}
#endif

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
    if (message.protocol > TRIGGER && message.bus < 1)
      message.bus = 1;
    sendInseparates(message);
  }
  else
  {
#if ENABLE_IRREMOTE
    message.protocol_name = nullptr;
    irFIFO.writeRef() = message;
    irFIFO.push();
#else
    sendInseparates(message);
#endif
  }
}

#if ENABLE_IRREMOTE
inseparates::Timekeeper16 irTimekeeper;
Message repeatMessage;

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

void loopIR()
{
  if (irTimekeeper.microsSinceReset() > 1000)
  {
    if (irrecv.decode(&results))
    {
#if FILTER_UNKNOWN_IR
      if (results.decode_type <= 0)
      {
        irrecv.resume();
        return;
      }
#endif
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

  if (!repeatMessage.dummy())
  {
    // We are repeating.
    if (repeatMessage.repeat == 0xff)
    {
      // Indefinite repeat.
      if (!irFIFO.empty() && irFIFO.readRef().protocol == repeatMessage.protocol)
        repeatMessage.makeDummy();
    }
    else
    {
      if (repeatMessage.repeat > 0)
      {
        --repeatMessage.repeat;
      }
    }
  }

  if (repeatMessage.dummy() && irFIFO.empty())
    return;

  const Message message = repeatMessage.dummy() ? irFIFO.readRef() : repeatMessage;

  if (repeatMessage.dummy())
  {
    irFIFO.pop();
    if (message.repeat)
      repeatMessage = message;
  }
  else
  {
    if (!repeatMessage.repeat)
      repeatMessage.makeDummy();
  }

  if (message.dummy())
  {
    return;
  }

  bool result = false;
  uint64_t value;
  uint8_t repeat = 0;

  static bool toggleRC5;
  static bool toggleRC6;

  if (message.addressAndCommandSet())
    switch (message.protocol)
    {
    case NEC:
      result = true;
      value = irsend.encodeNEC(message.address(), message.command());
      irsend.sendNEC(value, message.bits, repeat);
      break;
    case RC5:
      result = true;
      value = irsend.encodeRC5X(message.address(), message.command());
      toggleRC5 = !toggleRC5;
      if (toggleRC5)
        value = irsend.toggleRC5(value);
      irsend.sendRC5(value, message.bits, repeat);
      break;
    case RC6:
      result = true;
      value = irsend.encodeRC6(message.address(), message.command());
      toggleRC6 = !toggleRC6;
      if (toggleRC6)
        value = irsend.toggleRC6(value);
      irsend.sendRC6(value, message.bits, repeat);
      break;
    case SONY:
      result = true;
      value = irsend.encodeSony(message.bits, message.command(), message.address(), message.extendedSet());
      irsend.sendSony(value, message.bits, repeat);
      break;
    case PANASONIC:
      result = true;
      value = irsend.encodePanasonic(message.extended(), message.address() >> 8, message.address(), message.command());
      irsend.sendPanasonic(value, message.bits, repeat);
      break;
    case JVC:
      result = true;
      value = irsend.encodeJVC(message.address(), message.command());
      irsend.sendJVC(value, message.bits, repeat);
      break;
    case SAMSUNG:
      result = true;
      value = irsend.encodeSAMSUNG(message.address(), message.command());
      irsend.sendSAMSUNG(value, message.bits, repeat);
      break;
    case LG:
      result = true;
      value = irsend.encodeLG(message.address(), message.command());
      irsend.sendLG(value, message.bits, repeat);
      break;
    //case SHARP:
    //case DENON:
    case PIONEER:
      result = true;
      value = irsend.encodePioneer(message.address(), message.command());
      irsend.sendPioneer(value, message.bits, repeat);
      break;
    //case BANG_OLUFSEN:
    }
  else
    result = irsend.send((decode_type_t)message.protocol, message.value, message.bits, repeat);

  if (!result)
  {
    if (!repeatMessage.dummy())
      repeatMessage.makeDummy();
    logLine("UNHANDLED IR", repeatMessage.logTarget);
  }
}
#endif
