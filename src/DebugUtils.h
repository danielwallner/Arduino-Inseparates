// Copyright (c) 2024 Daniel Wallner

#ifndef _INS_DEBUG_UTILS_H_
#define _INS_DEBUG_UTILS_H_

#include "FastTime.h"

#ifdef INS_DEBUGGING
	void INS_DEBUGF(const char *format, ...);
	inline void INS_DEBUG(const char *s) { INS_DEBUGF("%s\n", s); }
	inline void INS_DEBUG(uint8_t v, uint8_t b = 10) { INS_DEBUGF(b == 16 ? "%hhX\n" : "%hhu\n", v); }
	inline void INS_DEBUG(int v, uint8_t b = 10) { INS_DEBUGF(b == 16 ? "%X\n" : "%d\n", v); }
	inline void INS_DEBUG(unsigned v, uint8_t b = 10) { INS_DEBUGF(b == 16 ? "%Xn" : "%u\n", v); }
	inline void INS_DEBUG(long v, uint8_t b = 10) { INS_DEBUGF(b == 16 ? "%lX\n" : "%ld\n", v); }
	inline void INS_DEBUG(unsigned long v, uint8_t b = 10) { INS_DEBUGF(b == 16 ? "%lX\n" : "%lu\n", v); }
#	ifdef UNIT_TEST
#		define DEC 10
#		define HEX 16
#		include <assert.h>
#		define INS_ASSERT(c) (assert(c))
#	else
#		define INS_ASSERT(c) do{if(!(c)){Serial.println("INS_ASSERT("#c") failed!");InsError(*(uint32_t*)"asrt");}}while(0)
#	endif
#else
#	define INS_DEBUGF(...) ((void)0)
#	define INS_DEBUG(...) ((void)0)
#	define INS_ASSERT(c) ((void)0)
#endif

namespace inseparates
{

// Serial print that isn't using interrupts on AVR.
// Will affect timing a bit less than Serial.print on AVR.
// Has a very short buffer, cannot print long strings!
class DebugPrinter : public SteppedTask
{
	static const uint8_t kBufferLength = 64;
	char _string[kBufferLength];
	uint8_t _pos = -1;
public:
	int printf(const char *format, ...)
	{
		uint8_t p = 0;
		if (_pos < kBufferLength && _string[_pos])
		{
			// Append
			for (p = _pos; p < kBufferLength && _string[p]; ++p);
		}
		else
		{
			_pos = 0;
		}
		va_list argptr;
		int ret;
		va_start(argptr, format);
		ret = vsnprintf(_string + p, kBufferLength - p, format, argptr);
		va_end(argptr);
		return ret;
	}

	void print(const char *string)
	{
		uint8_t p = 0;
		if (_pos < kBufferLength && _string[_pos])
		{
			// Append
			for (p = _pos;  p < kBufferLength && _string[p]; ++p);
		}
		else
		{
			_pos = 0;
		}

		uint8_t i = 0;
		for (; p + i < kBufferLength && string[i]; ++i)
		{
			_string[p + i] = string[i];
		}

		if (p + i < kBufferLength)
		{
			_string[p + i] = '\0';
		}
	}

	void println(const char *string)
	{
		uint8_t p = 0;
		if (_pos < kBufferLength && _string[_pos])
		{
			// Append
			for (p = _pos;  p < kBufferLength && _string[p]; ++p);
		}
		else
		{
			_pos = 0;
		}

		uint8_t i = 0;
		for (; p + i < kBufferLength && i < string[i]; ++i)
		{
			_string[p + i] = string[i];
		}

		if (p + i + 1 < kBufferLength)
		{
			_string[p + i] = '\n';
			_string[p + i + 1] = '\0';
		}
		else if (p + i < kBufferLength)
		{
			_string[p + i] = '\0';
		}
	}

#ifndef UNIT_TEST
	void print(const String &string) { print(string.c_str()); }
	void println(const String &string) { println(string.c_str()); }
#endif

	bool empty() const { return !(_pos < kBufferLength && _string[_pos]); }

	void flush() { while(!empty()) SteppedTask_step(); }

	uint16_t SteppedTask_step() override
	{
#if AVR
		if (!(UCSR0A & (1 << UDRE0)))
			return 20;
#endif
		if (_pos < kBufferLength && _string[_pos])
		{
			char c = _string[_pos];
#if AVR
			UDR0 = c;
#else
#ifndef UNIT_TEST
			Serial.print(c);
#else
			putchar(c);
#endif
#endif
			++_pos;
		}
		return 100;
	}
};

// Measures time between begin() to end() and end() to begin().
// Used together with TimeInserter.
// As it will itself affect timing use the results to investigate and to compare and not as actual truth.
class TimeAccumulator
{
	uint32_t _accumulatedOutsideTime;
	uint32_t _accumulatedInsideTime;
	uint16_t _maxOutsideTime;
	uint16_t _maxInsideTime;
	uint32_t _rounds;
	uint32_t _lastTime;
public:
	TimeAccumulator()
	{
		reset();
	}

	void reset()
	{
		_accumulatedOutsideTime = 0;
		_accumulatedInsideTime = 0;
		_maxOutsideTime = 0;
		_maxInsideTime = 0;
		_rounds = 0;
	}

	void begin(uint16_t now)
	{
#if INS_FAST_COUNT
		now = fastCount();
#endif
		if (!_rounds)
		{
			_lastTime = now;
		}
		++_rounds;
		uint16_t sinceLast = now - _lastTime;
		_lastTime = now;
		_accumulatedOutsideTime += sinceLast;
		if (sinceLast > _maxOutsideTime)
		{
			_maxOutsideTime = sinceLast;
		}
	}

	void end()
	{
		if (!_rounds)
		{
			return;
		}
#if INS_FAST_COUNT
		uint16_t now = fastCount();
#else
		uint16_t now = fastMicros();
#endif
		uint16_t sinceLast = now - _lastTime;
		_lastTime = now;
		_accumulatedInsideTime += sinceLast;
		if (sinceLast > _maxInsideTime)
		{
			_maxInsideTime = sinceLast;
		}
	}

	uint32_t rounds() { return _rounds; }

	void report(DebugPrinter &printer)
	{
		uint16_t meanOutsideTime = _accumulatedOutsideTime / _rounds;
		uint16_t meanInsideTime = _accumulatedInsideTime / _rounds;
		uint16_t maxOutsideTime = _maxOutsideTime;
		uint16_t maxInsideTime = _maxInsideTime;
#if INS_FAST_COUNT
		meanOutsideTime /= kFastCountsPerMicro;
		meanInsideTime /= kFastCountsPerMicro;
		maxOutsideTime /= kFastCountsPerMicro;
		maxInsideTime /= kFastCountsPerMicro;
#endif

		printer.printf("o%dm%d i%dm%d\n", meanOutsideTime, maxOutsideTime, meanInsideTime, maxInsideTime);
		reset();
	}

#ifndef UNIT_TEST
	void report()
	{
		uint16_t meanOutsideTime = _accumulatedOutsideTime / _rounds;
		uint16_t meanInsideTime = _accumulatedInsideTime / _rounds;
		uint16_t maxOutsideTime = _maxOutsideTime;
		uint16_t maxInsideTime = _maxInsideTime;
#if INS_FAST_COUNT
		meanOutsideTime /= kFastCountsPerMicro;
		meanInsideTime /= kFastCountsPerMicro;
		maxOutsideTime /= kFastCountsPerMicro;
		maxInsideTime /= kFastCountsPerMicro;
#endif

		Serial.print('o');
		Serial.print(meanOutsideTime);
		Serial.print('m');
		Serial.print(meanInsideTime);
		Serial.print(" i");
		Serial.print(meanInsideTime);
		Serial.print('m');
		Serial.println(maxInsideTime);
		reset();
	}
#endif
};

class TimeInserter
{
	TimeAccumulator &_acc;
public:
	TimeInserter(TimeAccumulator &acc, uint16_t now) : _acc(acc) { _acc.begin(now); }
	~TimeInserter() { _acc.end(); }
};

// Check time between tick().
// Has a bit less overhead than TimeAccumulator and is therefore slightly more accurate, see above.
class CycleChecker
{
	uint32_t _accumulatedTime;
	uint16_t _maxTime;
	uint32_t _rounds;
	uint32_t _lastTime;
public:
	CycleChecker()
	{
		reset();
	}

	void reset()
	{
		_accumulatedTime = 0;
		_maxTime = 0;
		_rounds = 0;
	}

	void tick(uint16_t now)
	{
#if INS_FAST_COUNT
		now = fastCount();
#endif
		if (!_rounds)
		{
			_lastTime = now;
		}
		++_rounds;
		uint16_t sinceLast = now - _lastTime;
		_lastTime = now;
		_accumulatedTime += sinceLast;
		if (sinceLast > _maxTime)
		{
			_maxTime = sinceLast;
		}
	}

	uint32_t rounds() { return _rounds; }

	void report(DebugPrinter &printer)
	{
		uint16_t meanCycleTime = _accumulatedTime / _rounds;
		uint16_t maxTime = _maxTime;
#if INS_FAST_COUNT
		meanCycleTime /= kFastCountsPerMicro;
		maxTime /= kFastCountsPerMicro;
#endif

		printer.printf("c%dm%d\n", meanCycleTime, maxTime);
		reset();
	}

#ifndef UNIT_TEST
	void report()
	{
		uint16_t meanCycleTime = _accumulatedTime / _rounds;
		uint16_t maxTime = _maxTime;
#if INS_FAST_COUNT
		meanCycleTime /= kFastCountsPerMicro;
		maxTime /= kFastCountsPerMicro;
#endif

		Serial.print('c');
		Serial.print(meanCycleTime);
		Serial.print('m');
		Serial.println(maxTime);
		reset();
	}
#endif
};

}

#endif
