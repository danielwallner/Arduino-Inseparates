// Copyright (c) 2024 Daniel Wallner

#include "Inseparates.h"
#include "ProtocolUtils.h"
#include "PlatformTimers.h"

namespace inseparates
{

void Scheduler::run(SteppedTask *task)
{
	uint16_t targetTime = fastMicros();
	uint16_t delta = 0;
	for (;;)
	{
		delta = task->SteppedTask_step();
		if (delta == SteppedTask::kInvalidDelta)
			return;
		targetTime += delta;
		int16_t offset = targetTime - fastMicros();
		if (offset > 0)
		{
			safeDelayMicros((uint16_t)offset);
		}
	}
}

#ifdef UNIT_TEST
void Scheduler::runFor(SteppedTask *task, unsigned steps)
{
	uint32_t targetTime = micros();
	uint16_t delta = 0;
	for (unsigned i = 0; i < steps; ++i)
	{
		delta = task->SteppedTask_step();
		if (delta == SteppedTask::kInvalidDelta)
			return;
		targetTime += delta;
		int32_t offset = targetTime - micros();
		if (offset > 0)
		{
			safeDelayMicros((uint16_t)offset);
		}
	}
}
#endif

#if !(UNIT_TEST || USE_FUNCTIONAL_INTERRUPT)
uint8_t Scheduler::PinStatePusher::s_pinUsage[MAX_PIN_CALLBACKS];

INS_IRAM_ATTR void pinISR(uint8_t pinIndex)
{
	auto &inputFifo = Scheduler::PinStatePusher::schedulerInstance()->inputFIFO();
	auto &w = inputFifo.writeRef();
	w.micros = fastMicros();
	uint8_t pin = Scheduler::PinStatePusher::s_pinUsage[pinIndex];
	w.pin = pin;
	w.state = digitalRead(pin);
	inputFifo.push();
}

INS_IRAM_ATTR void pinISR0() { pinISR(0); }
INS_IRAM_ATTR void pinISR1() { pinISR(1); }
INS_IRAM_ATTR void pinISR2() { pinISR(2); }
#ifndef AVR
INS_IRAM_ATTR void pinISR3() { pinISR(3); }
INS_IRAM_ATTR void pinISR4() { pinISR(4); }
INS_IRAM_ATTR void pinISR5() { pinISR(5); }
INS_IRAM_ATTR void pinISR6() { pinISR(6); }
INS_IRAM_ATTR void pinISR7() { pinISR(7); }
#endif
#endif

#ifdef ARDUINO_ARCH_SAMD
HWTimer::CallbackFunction HWTimer::s_callback;

#if INS_USE_TC4
extern "C" void TC4_Handler()
{
	if (TC4->COUNT16.INTFLAG.bit.MC0)
	{
		TC4->COUNT16.INTFLAG.reg = TC_INTFLAG_MC0;
		if (HWTimer::s_callback)
		{
			HWTimer::s_callback();
		}
	}
}
#else
extern "C" void TC3_Handler()
{
	if (TC3->COUNT16.INTFLAG.bit.MC0)
	{
		TC3->COUNT16.INTFLAG.reg = TC_INTFLAG_MC0;
		if (HWTimer::s_callback)
		{
			HWTimer::s_callback();
		}
	}
}
#endif
#endif

#if (INS_HAVE_HW_TIMER || UNIT_TEST) && INS_OUTPUT_FIFO_CHANNEL_COUNT
INS_IRAM_ATTR void timerISR()
{
	auto *s_this = InterruptWriteScheduler::instance();
	for (uint8_t i = 0; i < INS_OUTPUT_FIFO_CHANNEL_COUNT; ++i)
	{
		static int count;
		if (!(count%100000))
		{
			count = 0;
		}
		++count;
		auto &fifo = s_this->outputFIFO(i);
		if (fifo.empty())
		{
			continue;
		}
		ins_micros_t now = fastMicros();
		auto &r = fifo.readRef();
		ins_micros_t targetMicros = r.micros;
		ins_smicros_t timeLeft = targetMicros - now;
		if (timeLeft > s_this->_pollIntervalMicros / 2)
		{
			continue;
		}
		uint8_t pin = r.pin;
		if (pin == (uint8_t)-1)
		{
			fifo.pop();
			continue;
		}
		static int count2;
		if ((count2%100) == 99)
		{
			count2 = 0;
		}
		++count2;
		uint8_t state = r.state;
		uint8_t mode = r.mode;
		digitalWrite(pin, state);
		if (mode == OUTPUT)
		{
			digitalWrite(pin, state);
			pinMode(pin, mode);
		}
		else
		{
			pinMode(pin, OUTPUT);
		}
		fifo.pop();
	}
}
#endif

int Serial_printf(const char *format, ...)
{
#ifdef UNIT_TEST
	va_list argptr;
	va_start(argptr, format);
	int ret = vprintf(format, argptr);
	va_end(argptr);
	return ret;
#else
	static const uint8_t kBufferLength = 64;
	char string[kBufferLength];
	va_list argptr;
	int ret;
	va_start(argptr, format);
	ret = vsnprintf(string, kBufferLength, format, argptr);
	va_end(argptr);
	Serial.print(string);
	return ret;
#endif
}

int DebugPrinter::printf(const char *format, ...)
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

void DebugPrinter::print(const char *string)
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

void DebugPrinter::println(const char *string)
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

uint16_t DebugPrinter::SteppedTask_step()
{
#ifdef AVR
	if (!(UCSR0A & (1 << UDRE0)))
		return 20;
	if (_pos < kBufferLength && _string[_pos])
	{
		UDR0 = _string[_pos];
		++_pos;
	}
#else
#if !defined(UNIT_TEST)
	if (Serial.availableForWrite() < 1)
		return 100;
#endif
	if (_pos < kBufferLength && _string[_pos])
	{
#if defined(UNIT_TEST)
		putchar(_string[_pos]);
#else
		size_t length = strnlen(_string + _pos, kBufferLength - _pos);
		if (length < kBufferLength - _pos && Serial.availableForWrite() > length)
		{
			Serial.print(_string + _pos);
			_pos = kBufferLength;
			return 100;
		}
		Serial.print(char(_string[_pos]));
#endif
		++_pos;
	}
#endif
	return 100;
}

}
