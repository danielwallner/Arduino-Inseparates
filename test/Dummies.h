// Copyright (c) 2024 Daniel Wallner

#ifndef _INS_TESTDUMMIES_H_
#define _INS_TESTDUMMIES_H_

#include <map>
#include <vector>
#include <stdint.h>

// Mock functions for Arduino.h

#define HIGH 1
#define LOW 0

#define INPUT 0
#define OUTPUT 1

#define CHANGE 0

// In Arduino the return type of micros() is unsigned long
// that would make it a 64-bit type on some 64-bit platforms.
// uint32_t is returned here to avoid that.
uint32_t micros();
void delayMicroseconds(unsigned int us);

void pinMode(uint8_t pin, uint8_t mode);
int digitalRead(uint8_t pin);
void digitalWrite(uint8_t pin, uint8_t value);

#define digitalPinToInterrupt(p)  (p)

void attachInterrupt(uint8_t interruptNum, std::function<void(void)> userFunc, int mode);
void attachInterrupt(uint8_t interruptNum, void (*userFunc)(void), int mode);
void detachInterrupt(uint8_t interruptNum);

void attachInterruptInterval(uint8_t interval, void (*userFunc)(void));

void tone(uint8_t _pin, unsigned int frequency, unsigned long duration = 0);
void noTone(uint8_t _pin);

// Log information for tests

void resetLogs();
uint32_t totalDelay();

extern std::vector<uint32_t> g_delayMicrosecondsLog;
extern std::map<uint8_t, std::vector<uint8_t>> g_digitalWriteStateLog;
extern std::map<uint8_t, std::vector<uint32_t>> g_digitalWriteTimeLog;
extern std::map<uint8_t, uint8_t> g_pinStates;
extern std::map<uint8_t, uint32_t> g_lastWrite;

#endif
