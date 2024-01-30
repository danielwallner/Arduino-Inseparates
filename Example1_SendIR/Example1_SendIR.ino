// Copyright (c) 2024 Daniel Wallner

// A simple send example that doesn't use full scheduling.
// Sends nonconcurrent messages, one at a time.

//#define INS_FAST_TIME 1
#define HW_PWM 1 // Will use timer 2 on AVR.
#define SW_PWM 0 // This requires running two tasks in parallel, see below.

#define TEST_PWM 0 // Sends long marks for PWM tests.

#define ACTIVE HIGH

#include <Inseparates.h>
#include <ProtocolRC5.h>

const uint16_t kIRSendPin = 26; // Use pin 3 when using HW_PWM on AVR.

using namespace inseparates;

#if HW_PWM
PWMPinWriter pinWriter(kIRSendPin, ACTIVE);
#elif SW_PWM
Scheduler scheduler;
SoftPWMPinWriter pinWriter(kIRSendPin, ACTIVE);
#else
OpenDrainPinWriter pinWriter(kIRSendPin, ACTIVE);
#endif

#if TEST_PWM
TxJam tx(&pinWriter, ACTIVE, 5000);
#else
TxRC5 tx(&pinWriter, ACTIVE);
#endif

// All errors end up here.
// This function must be defined or there will be linker errors.
void InsError(uint32_t error)
{
  char errorMsg[5];
  strncpy(errorMsg, (const char*)&error, 4);
  errorMsg[4] = 0;
  Serial.print("ERROR: ");
  Serial.println(errorMsg);
  Serial.flush();
  for(;;); // Halt. If there's a watchdog timer this will trigger restart.
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
  pinWriter.prepare(315000, 30);
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

#if SW_PWM
  // When using software PWM we need to run two tasks in parallel.
  // That means we must run the normal scheduling mechanism.
  scheduler.add(&tx);
  // Run the scheduler for as long as the send is active.
  while (scheduler.active(&tx))
    scheduler.SteppedTask_step(fastMicros());

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
