// Copyright (c) 2024 Daniel Wallner

#ifndef _INS_FAST_TIME_H_
#define _INS_FAST_TIME_H_

#include <stdint.h>

#if UNIT_TEST
#include "../test/Dummies.h"
#else
#include <Arduino.h>
#endif

namespace inseparates
{

#if INS_FAST_TIME && AVR

#define INS_FAST_COUNT 1

static_assert((uint32_t(F_CPU) / 8000000L) * 8000000L == F_CPU);

static const uint8_t kFastCountsPerMicro = F_CPU / 8000000L;
static const uint16_t kFastCountMaxMicros = 32768;

// Setup timer 1 as 0.5 us counter and disable timer 0 interrupts.
// micros()/milllis() will no longer work after running this.
inline void setupFastTime()
{
	TCCR1A = 0;
	TCCR1B = 2; // Prescaler = 8
	TCNT1 = 0;
	// Disable timer 0/1 interrupts
	TIMSK0 = 0; // Timer 0
	TIMSK1 = 0; // Timer 1
}

inline uint16_t fastCount()
{
#if INS_SAFE_FAST_COUNT
	uint8_t oldSREG = SREG;
	cli();
	uint16_t cnt = TCNT1;
	SREG = oldSREG;
	return cnt;
#else
	// If some interrupt runs that use the temp register this could fail.
	return TCNT1;
#endif
}

// Must must be called more often than kFastCountMaxMicros with some margin!
inline uint16_t fastMicros()
{
	static uint16_t micros;
	static uint16_t oldCnt;
	uint16_t now = fastCount();
	uint16_t diff = (now - oldCnt) / kFastCountsPerMicro;
	micros += diff;
	oldCnt += diff * kFastCountsPerMicro;
	return micros;
}

#else

#define setupFastTime() ((void)0)

inline uint16_t fastMicros()
{
	return micros();
}

#endif

inline void safeDelayMicros(int32_t microsDelay)
{
	// https://www.arduino.cc/reference/en/language/functions/time/delaymicroseconds/
	while (microsDelay > 0)
	{
#ifndef UNIT_TEST
		if (microsDelay <= 10)
		{
			uint16_t start = fastMicros();
			while(fastMicros() - start < uint16_t(microsDelay));
			return;
		}
#endif
		if (microsDelay <= 16383)
		{
			delayMicroseconds(microsDelay);
			return;
		}
#if INS_FAST_TIME && AVR
		fastMicros();
#endif
		delayMicroseconds(16383);
		microsDelay -= 16383;
	}
}

// Helper for easier timekeeping when fastMicros() 16 bits are not enough.
class Timekeeper
{
	uint32_t _start;
	uint32_t _micros32;
public:
	Timekeeper() : _start(fastMicros()) {}

	void reset() { _start = _micros32 = 0; }

	void tick(uint16_t micros)
	{
		uint16_t diff = micros - uint16_t(_micros32);
		_micros32 += diff;
	}
	uint32_t microsSinceReset(uint16_t micros) { tick(micros); return _micros32 - _start; }

	void tick()
	{
		tick(fastMicros());
	}
	uint32_t microsSinceReset() { tick(); return _micros32 - _start; }
};

// Less resource demanding version of the Timekeeper that only use 16 bits.
class Timekeeper16
{
	uint16_t _start;
public:
	Timekeeper16() : _start(fastMicros()) {}

	void reset() { _start = fastMicros(); }

	uint16_t microsSinceReset(uint16_t micros) { return micros - _start; }

	uint16_t microsSinceReset() { return fastMicros() - _start; }
};

}

#endif
