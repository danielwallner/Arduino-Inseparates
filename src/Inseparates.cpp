// Copyright (c) 2024 Daniel Wallner

#include "Inseparates.h"

namespace inseparates
{

void Scheduler::run(SteppedTask *task)
{
	uint32_t targetTime = fastMicros();
	uint16_t delta = 0;
	for (;;)
	{
		uint32_t now = fastMicros();
		delta = task->SteppedTask_step(now);
		if (delta == kInvalidDelta)
			return;
		targetTime += delta;
		int32_t offset = targetTime - now;
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
		uint32_t now = micros();
		delta = task->SteppedTask_step(now);
		if (delta == kInvalidDelta)
			return;
		targetTime += delta;
		int32_t offset = targetTime - now;
		if (offset > 0)
		{
			safeDelayMicros((uint16_t)offset);
		}
	}
}
#endif

}
