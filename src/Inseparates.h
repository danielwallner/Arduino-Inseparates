// Copyright (c) 2024 Daniel Wallner

#ifndef _INSEPARATES_H_
#define _INSEPARATES_H_

#include "FastTime.h"

#ifndef INS_SEQUENCER_MAX_NUM_TASKS
#define INS_SEQUENCER_MAX_NUM_TASKS 8
#endif

void InsError(uint32_t error);

namespace inseparates
{

// Cooperative multitasking based on tasks that runs non-blocking code
// and sleeps the returned microseconds after each step.

class SteppedTask
{
public:
	static const uint16_t kInvalidDelta = (uint16_t)-1;
	static const uint16_t kMaxSleepMicros = (uint16_t)-2;

	// Must be non-blocking.
	// Returns number of microseconds to wait until next call.
	// Returning kInvalidDelta stops the task.
	// Input value must be fastMicros()
	virtual uint16_t SteppedTask_step(uint32_t now) = 0;
};

class TimeKeeper
{
	uint32_t _start;
public:
	TimeKeeper() : _start(fastMicros()) {}

	void reset(uint32_t now) { _start = now; }
	void reset() { reset(fastMicros()); }

	uint32_t microsSinceReset(uint32_t now = fastMicros()) { return now - _start; }
	uint16_t millisSinceReset(uint32_t now = fastMicros()) { return microsSinceReset(now) / 1000; }
	uint16_t secondsSinceReset(uint32_t now = fastMicros()) { return microsSinceReset(now) / 1000000UL; }
};

// A scheduler is itself also a task that can be stepped.

class Scheduler : public SteppedTask
{
public:
	class Delegate
	{
	public:
		virtual void SchedulerDelegate_done(SteppedTask *task) = 0;
	};

private:
	struct TaskState
	{
		SteppedTask *task;
		Delegate *delegate;
		uint32_t targetTime;
	};

	TaskState _tasks[INS_SEQUENCER_MAX_NUM_TASKS];
	uint8_t _maxCheck = 0;

public:
	Scheduler()
	{
		memset(_tasks, 0, sizeof(_tasks));
	}

	void begin()
	{
#if INS_FAST_COUNT
		setupFastTime();
#endif
	}

	// Add and step task.
	bool add(SteppedTask *task, Delegate *delegate = nullptr)
	{
		for (int i = 0; i < INS_SEQUENCER_MAX_NUM_TASKS; ++i)
		{
			if (_tasks[i].task)
				continue;
			_tasks[i].task = task;
			_tasks[i].delegate = delegate;
			_tasks[i].targetTime = fastMicros();
			_tasks[i].targetTime += task->SteppedTask_step(_tasks[i].targetTime);
			if (i + 1 > _maxCheck)
				_maxCheck = i + 1;
			return true;
		}
		InsError(*(uint32_t*)"sovf");
		return false;
	}

	// Remove task.
	bool remove(SteppedTask *task)
	{
		for (int i = 0; i < _maxCheck; ++i)
		{
			if (_tasks[i].task != task)
				continue;
			_tasks[i].task = nullptr;
			return true;
		}
		InsError(*(uint32_t*)"nstk");
		return false;
	}

	// Check if task is active.
	bool active(SteppedTask *task)
	{
		for (int i = 0; i < _maxCheck; ++i)
		{
			if (_tasks[i].task != task)
				continue;
			return true;
		}
		return false;
	}

	// Iterate all active tasks.
	uint16_t SteppedTask_step(uint32_t now) override
	{
		uint16_t minimum = -1;
		for (int i = 0 ; i < _maxCheck; ++i)
		{
			if (!_tasks[i].task)
			{
				if (i + 1 == _maxCheck)
					--_maxCheck;
				continue;
			}
			int32_t timeLeft = _tasks[i].targetTime - now;
			if (timeLeft > 0)
			{
				if (timeLeft < minimum)
					minimum = timeLeft;
				continue;
			}
			uint16_t delta = _tasks[i].task->SteppedTask_step(now);
			now = fastMicros();
			if (delta == kInvalidDelta)
			{
				if (_tasks[i].delegate)
					_tasks[i].delegate->SchedulerDelegate_done(_tasks[i].task);
				_tasks[i].task = nullptr;
				continue;
			}
			_tasks[i].targetTime += delta;
			timeLeft = _tasks[i].targetTime - now;
			if (timeLeft < minimum)
				minimum = timeLeft;
		}
		return minimum;
	}

	// Simple blocking wrapper of step() that runs until finished.
	static void run(SteppedTask *task);

#ifdef UNIT_TEST
	static void runFor(SteppedTask *task, unsigned steps);
#endif
};

}

#endif
