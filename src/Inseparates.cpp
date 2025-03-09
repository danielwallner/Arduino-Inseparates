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

}
