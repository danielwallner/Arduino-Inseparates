// Copyright (c) 2024 Daniel Wallner

// A simple send example that doesn't use full scheduling.
// Sends single messages, one at a time.

#define INS_FAST_TIME 1
#define MODULATE 1

#define TEST_PWM 0 // Sends long marks for PWM tests.

#include <Inseparates.h>
#include <ProtocolRC5.h>

#if defined(ESP8266) // WEMOS D1 R2
static const uint8_t D_3  = 5;
static const uint8_t D_8  = 12;
static const uint8_t D_9  = 13;
#elif defined(CONFIG_IDF_TARGET_ESP32C3) // ESP32 C3 16 pin supermini
static const uint8_t D_3  = 10; // IR RECEIVE
static const uint8_t D_8  = 0; // RC5
static const uint8_t D_9  = 2; // IR SEND
#elif defined(ESP32) // WEMOS D1 R32
static const uint8_t D_3  = 25;
static const uint8_t D_8  = 12;
static const uint8_t D_9  = 13;
#else
static const uint8_t D_3  = 3;
static const uint8_t D_8  = 8;
static const uint8_t D_9  = 9;
#endif

#if MODULATE
// Send IR
#define HW_PWM 1 // Will use timer 2 on AVR.
#define SW_PWM 0 // This requires running two tasks in parallel, see below.
#define ACTIVE LOW
#define OFF_MODE INPUT_PULLUP
const uint16_t kIRSendPin = D_9;
#else
// RC-5 connector compatible signal
#define ACTIVE HIGH
#ifdef INPUT_PULLDOWN
#define OFF_MODE INPUT_PULLDOWN
#else
#define OFF_MODE INPUT
#endif
const uint16_t kIRSendPin = D_8;
#endif


using namespace inseparates;

#if HW_PWM
PWMPinWriter pinWriter(kIRSendPin, ACTIVE);
#elif SW_PWM
Scheduler scheduler;
SoftPWMPinWriter pinWriter(kIRSendPin, ACTIVE);
#else
OpenDrainPinWriter pinWriter(kIRSendPin, ACTIVE, OFF_MODE);
#endif

#if TEST_PWM
TxJam tx(&pinWriter, ACTIVE, 5000);
#else
TxRC5 tx(&pinWriter, ACTIVE);
#endif

// All errors end up here.
// This function must be defined or there will be a linker error.
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

void setup()
{
  Serial.begin(115200);

  while (!Serial)
    delay(50);
  Serial.println();
  Serial.print("IR send pin: ");
  Serial.println(kIRSendPin);

#if HW_PWM
  pinWriter.prepare(36000, 30);
#elif SW_PWM
  pinWriter.prepare(36000, 30);
  scheduler.add(&pinWriter);
#endif
}

void loop()
{
#if !TEST_PWM
  static const uint8_t address = 16; // Preamp
  static const uint8_t command = 13; // Mute
  static uint8_t toggle = (~toggle) & 1;
  uint16_t encodedValue = tx.encodeRC5(0, address, command);
  tx.prepare(encodedValue);
#endif

#if SW_PWM && !HW_PWM
  // When using software PWM we need to run two tasks in parallel.
  // That means we must run the normal scheduling mechanism.
  scheduler.add(&tx);
  // Run the scheduler for as long as the send is active.
  while (scheduler.active(&tx))
    scheduler.poll();

#else

  // Run single task to completion without full scheduling.
  Scheduler::run(&tx);
#endif

#if !TEST_PWM
  Serial.print("Did send: ");
  Serial.println(encodedValue, HEX);

  // When running concurrent tasks loop() should only run Scheduler::SteppedTask_step() and never any form of delay.
  // When sending individual messages with Scheduler::run() as above, delay can be used normally.
  delay(100);
#endif
}
