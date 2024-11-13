// Copyright (c) 2024 Daniel Wallner

#ifndef _INSEPARATES_H_
#define _INSEPARATES_H_

#include "FastTime.h"

#ifndef INS_SEQUENCER_MAX_NUM_TASKS
#define INS_SEQUENCER_MAX_NUM_TASKS 8
#endif
#ifndef INS_SEQUENCER_MAX_NUM_INPUTS
#define INS_SEQUENCER_MAX_NUM_INPUTS 8
#endif

#define INS_STR_(v) #v
#define INS_STR(v) INS_STR_(v)

void InsError(uint32_t error);

namespace inseparates
{

// Cooperative multitasking based on tasks that runs non-blocking code
// and sleeps the returned microseconds after each step.

class SteppedTask
{
public:
	static const uint16_t kInvalidDelta = (uint16_t)-1;
	static const uint16_t kMaxSleepMicros = 0x7FFF;

	// Must be non-blocking.
	// Returns number of microseconds to wait until next call.
	// Returning kInvalidDelta stops the task.
	virtual uint16_t SteppedTask_step() = 0;
};

class DummyTask : public SteppedTask
{
public:
	uint16_t SteppedTask_step() override { return kInvalidDelta; }
};

class Decoder
{
public:
	static const uint16_t kInvalidTimeout = (uint16_t)0;
	static const uint16_t kMaxTimeout = 0x7FFF;

	// Must be non-blocking.
	// Note that state here is the state _before_ the current state transistion.
	// Should return the updated number of microseconds to timeout.
	virtual uint16_t Decoder_pulse(uint8_t state, uint16_t pulseWidth) = 0;

	// This is called when no input transition has happend during the returned timeout.
	virtual void Decoder_timeout(uint8_t pinState) = 0;
};

// Input polling and task scheduling
class Scheduler
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
		uint16_t targetTime;
	};

	struct InputState
	{
		uint8_t pin;
		uint8_t pinState;
		Decoder *decoder;
		uint16_t lastTransitionMicros;
		uint16_t nextTimeoutMicros;
	};

	TaskState _tasks[INS_SEQUENCER_MAX_NUM_TASKS];
	uint8_t _maxTask = 0;
	InputState _inputs[INS_SEQUENCER_MAX_NUM_INPUTS];
	uint8_t _maxInput = 0;

public:
	Scheduler()
	{
		memset(_tasks, 0, sizeof(_tasks));
		memset(_inputs, 0, sizeof(_inputs));
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
			_tasks[i].targetTime += task->SteppedTask_step();
			if (i + 1 > _maxTask)
				_maxTask = i + 1;
			return true;
		}
		InsError(*(uint32_t*)"tovf");
		return false;
	}

	// Remove task.
	bool remove(SteppedTask *task)
	{
		bool found = false;
		for (int i = 0; i < _maxTask; ++i)
		{
			if (_tasks[i].task != task)
				continue;
			_tasks[i].task = nullptr;
			found = true;
			break;
		}
		if (!found)
		{
			InsError(*(uint32_t*)"nstk");
			return false;
		}
		for (int i = _maxTask; i; --i)
		{
			if (_tasks[i - 1].task)
				return true;
			_maxTask = i;
		}
		return true;
	}

	// Check if task is active.
	bool active(SteppedTask *task)
	{
		for (int i = 0; i < _maxTask; ++i)
		{
			if (_tasks[i].task != task)
				continue;
			return true;
		}
		return false;
	}

	// Add Decoder.
	bool add(Decoder *decoder, uint8_t pin)
	{
		for (int i = 0; i < INS_SEQUENCER_MAX_NUM_INPUTS; ++i)
		{
			if (_inputs[i].decoder)
				continue;
			_inputs[i].decoder = decoder;
			_inputs[i].pin = pin;
#if INS_ENABLE_INPUT_FILTER
			_inputs[i].pinState = 3 * digitalRead(pin);
#else
			_inputs[i].pinState = digitalRead(pin);
#endif
			uint16_t now = fastMicros();
			_inputs[i].lastTransitionMicros = now;
			_inputs[i].nextTimeoutMicros = now;
			if (i + 1 > _maxInput)
				_maxInput = i + 1;
			return true;
		}
		InsError(*(uint32_t*)"dovf");
		return false;
	}

	// Remove decoder.
	bool remove(Decoder *decoder)
	{
		bool found = false;
		for (int i = 0; i < _maxInput; ++i)
		{
			if (_inputs[i].decoder != decoder)
				continue;
			_inputs[i].decoder = nullptr;
			found = true;
			break;
		}
		if (!found)
		{
			InsError(*(uint32_t*)"nsdc");
			return false;
		}
		for (int i = _maxInput; i; --i)
		{
			if (_inputs[i - 1].decoder)
				return true;
			_maxInput = i;
		}
		return true;
	}

	// Check if decoder is active.
	bool active(Decoder *decoder)
	{
		for (int i = 0; i < _maxInput; ++i)
		{
			if (_inputs[i].decoder != decoder)
				continue;
			return true;
		}
		return false;
	}

	// Iterate all active tasks.
	void poll()
	{
		pollInputs();

		uint16_t now = fastMicros();
		for (int i = 0 ; i < _maxTask; ++i)
		{
			if (!_tasks[i].task)
				continue;
			int16_t timeLeft = _tasks[i].targetTime - now;
			if (timeLeft > 0)
			{
				continue;
			}
			uint16_t delta = _tasks[i].task->SteppedTask_step();
			_tasks[i].targetTime += delta;
			now = fastMicros();
			if (delta == SteppedTask::kInvalidDelta)
			{
				if (_tasks[i].delegate)
					_tasks[i].delegate->SchedulerDelegate_done(_tasks[i].task);
				_tasks[i].task = nullptr;
				continue;
			}
		}

		pollInputs();
	}

	// Simple blocking wrapper of step() that runs until finished.
	static void run(SteppedTask *task);

#ifdef UNIT_TEST
	static void runFor(SteppedTask *task, unsigned steps);
#endif

private:
	void pollInputs()
	{
		uint16_t now = fastMicros();
		for (int i = 0; i < _maxInput; ++i)
		{
			if (!_inputs[i].decoder)
				continue;
#if INS_ENABLE_INPUT_FILTER // Removes single glitches
			bool oldPinState = _inputs[i].pinState > 1;
			uint8_t pinState = digitalRead(_inputs[i].pin);
			if (pinState && _inputs[i].pinState < 3)
				 ++_inputs[i].pinState;
			else if (!pinState && _inputs[i].pinState > 0)
				--_inputs[i].pinState;
			bool newPinState = _inputs[i].pinState > 1;
#else
			uint8_t oldPinState = _inputs[i].pinState;
			uint8_t newPinState = digitalRead(_inputs[i].pin);
			_inputs[i].pinState = newPinState;
#endif

			if (newPinState == oldPinState)
			{
				if (_inputs[i].nextTimeoutMicros != _inputs[i].lastTransitionMicros)
				{
					if (int16_t(_inputs[i].nextTimeoutMicros - now) < 0)
					{
#ifdef INS_TIMEOUT_DEBUG_PIN
						static uint8_t s_timeoutToggle;
						s_timeoutToggle ^= 1;
						digitalWrite(INS_TIMEOUT_DEBUG_PIN, s_timeoutToggle);
#endif
						_inputs[i].decoder->Decoder_timeout(newPinState);
					}
				}
				continue;
			}

#ifdef INS_SAMPLE_DEBUG_PIN
			static uint8_t s_sampleToggle;
			s_sampleToggle ^= 1;
			digitalWrite(16, s_sampleToggle);
#endif
			uint16_t delta = _inputs[i].decoder->Decoder_pulse(oldPinState, now - _inputs[i].lastTransitionMicros);
			_inputs[i].nextTimeoutMicros = now + delta;
			_inputs[i].lastTransitionMicros = now;
			now = fastMicros();
		}
	}
};

}
#endif
