// Copyright (c) 2024 Daniel Wallner

#include "Dummies.h"

#include <assert.h>

std::vector<uint32_t> g_delayMicrosecondsLog;
std::map<uint8_t, std::vector<uint8_t>> g_digitalWriteStateLog;
std::map<uint8_t, std::vector<uint32_t>> g_digitalWriteTimeLog;
std::map<uint8_t, uint8_t> g_pinStates;
std::map<uint8_t, uint32_t> g_lastWrite;
#if 1
// For wraparound debugging.
const uint32_t kStartTime = 0xFFFFC000;
#else
// For easier timing debugging as micros() returns elapsed time directly.
const uint32_t kStartTime = 0;
#endif

void delayMicroseconds(unsigned int us)
{
	if (g_delayMicrosecondsLog.size())
		g_delayMicrosecondsLog.push_back(g_delayMicrosecondsLog.back() + us);
	else
		g_delayMicrosecondsLog.push_back(kStartTime + us);
}

uint32_t micros()
{
	if (!g_delayMicrosecondsLog.size())
		return kStartTime;
	return g_delayMicrosecondsLog.back();
}

void pinMode(uint8_t /*pin*/, uint8_t /*mode*/)
{
}

int digitalRead(uint8_t pin)
{
	return g_pinStates[pin];
}

// TODO: Store only if pinmode is correct too!
void digitalWrite(uint8_t pin, uint8_t value)
{
	g_pinStates[pin] = value;
	g_digitalWriteStateLog[pin].push_back(value);
	if (g_digitalWriteTimeLog[pin].size())
	{
		uint32_t diff = micros() - g_lastWrite[pin];
		g_digitalWriteTimeLog[pin].push_back(diff);
	}
	else
	{
		g_digitalWriteTimeLog[pin].push_back(0);
	}
	g_lastWrite[pin] = micros();
}

void resetLogs()
{
	g_delayMicrosecondsLog.clear();
	g_digitalWriteStateLog.clear();
	g_digitalWriteTimeLog.clear();
}

uint32_t totalDelay()
{
	if (!g_delayMicrosecondsLog.size())
		return 0;
	return g_delayMicrosecondsLog.back() - kStartTime;
}

void tone(uint8_t /*_pin*/, unsigned int /*frequency*/, unsigned long /*duration*/)
{
}

void noTone(uint8_t /*_pin*/)
{
}

void InsError(uint32_t error)
{
	char errorMsg[5];
	strncpy(errorMsg, (const char*)&error, 4);
	errorMsg[4] = 0;
	assert(0);
}
