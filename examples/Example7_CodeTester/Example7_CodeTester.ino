// Copyright (c) 2024 Daniel Wallner

// Sends messages when space bar is presssed.
// Modify address and command with W A S D or 8 4 5 6 keys.
// Works best with a standalone serial monitor where you don't have to press enter to trigger send.

// Many devices support more commands than those available on the remote.
// Set the address to a known good value and test different commands with this sketch.

#define INS_FAST_TIME 1

#define HW_PWM 0 // Will use timer 2 on AVR.
#define SW_PWM 0 // Define HW_PWM or SW_PWM to modulate the IR output.

#include <Inseparates.h>
#include <DebugUtils.h>

#include <ProtocolUtils.h>
#include <ProtocolRC5.h>
#include <ProtocolNEC.h>

#if !defined(D3) && defined(PD3)
#define D3 PD3
#endif

const uint16_t kIRSendPin = D3;

using namespace inseparates;

DebugPrinter printer;
Scheduler scheduler;

#if HW_PWM
PWMPinWriter irPinWriter(kIRSendPin, LOW);
#elif SW_PWM
SoftPWMPinWriter irPinWriter(kIRSendPin, LOW);
#else
PushPullPinWriter irPinWriter(kIRSendPin);
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

class MainTask  : public SteppedTask
{
  TxRC5 _txRC5;
  Timekeeper _time;

  uint8_t address = 0;
  uint8_t command = 0;
  uint8_t toggle = 0;

public:
  MainTask() :
    _txRC5(&irPinWriter, LOW)
  {
  }

  void begin()
  {
    scheduler.add(this);
  }

  uint16_t SteppedTask_step() override
  {
    if (Serial.available())
    {
      int c = Serial.read();
      switch (c)
      {
      case 'A':
      case 'a':
      case '4':
        --address;
        break;

      case 'D':
      case 'd':
      case '6':
        ++address;
        break;

      case 'W':
      case 'w':
      case '8':
        ++command;
        break;

      case 'S':
      case 's':
      case '5':
      case '2':
        --command;
        break;

      case ' ':
        {
          printer.println("Send");
          toggle ^= 1;
          uint16_t encodedMessage = _txRC5.encodeRC5(toggle, address, command);
          _txRC5.prepare(encodedMessage);
          scheduler.add(&_txRC5);
          return 10000;
        }
      default:
        return 10000;
      }
      printer.printf("A:%hhd C%hhd\n", address, command);
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

  Serial.print("IR output pin: ");
  Serial.println(kIRSendPin);
  Serial.flush();

  scheduler.begin();
  scheduler.add(&printer);
  mainTask.begin();

#if HW_PWM || SW_PWM
  irPinWriter.prepare(36000, 30);
#endif
}

void loop()
{
  uint16_t now = fastMicros();

  scheduler.poll();
}
