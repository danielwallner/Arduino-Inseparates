// Copyright (c) 2024 Daniel Wallner

#include "Inseparates.h"
#include "ProtocolUtils.h"

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

#if INS_HAVE_HW_TIMER || UNIT_TEST
InterruptScheduler *InterruptScheduler::s_this;
InterruptWriteScheduler *InterruptWriteScheduler::s_this;
#endif

}
