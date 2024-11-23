// Copyright (c) 2024 Daniel Wallner

#include "Inseparates.h"
#include "ProtocolUtils.h"

#ifdef ARDUINO_ARCH_SAMD
#include <SAMDTimerInterrupt.h>
#endif

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
uint8_t Scheduler::PinStatePusher::s_pinUsage[MAX_PIN_CALLBACKS] = { (uint8_t)-1 };
#endif

}
