// Copyright (c) 2024 Daniel Wallner

#ifndef _INSEPARATES_H_
#define _INSEPARATES_H_

#include "FastTime.h"
#ifndef AVR
#include <atomic>
#endif

#ifndef INS_SEQUENCER_MAX_NUM_TASKS
#define INS_SEQUENCER_MAX_NUM_TASKS 8
#endif
#ifndef INS_SEQUENCER_MAX_NUM_INPUTS
#define INS_SEQUENCER_MAX_NUM_INPUTS 8
#endif
#ifndef INS_INPUT_FIFO_LENGTH
#define INS_INPUT_FIFO_LENGTH 32
#endif

#define INS_STR_(v) #v
#define INS_STR(v) INS_STR_(v)

#ifdef IRAM_ATTR
#define INS_IRAM_ATTR IRAM_ATTR
#else
#define INS_IRAM_ATTR
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
	friend class InterruptScheduler;
public:
	class Delegate
	{
	public:
		virtual void SchedulerDelegate_done(SteppedTask *task) = 0;
	};

private:
#ifdef AVR
	typedef uint8_t pin_usage_t;
#else
	typedef uint16_t pin_usage_t;
#endif

	SteppedTask *_tasks_task[INS_SEQUENCER_MAX_NUM_TASKS];
	Delegate *_tasks_delegate[INS_SEQUENCER_MAX_NUM_TASKS];
	uint16_t _tasks_targetTime[INS_SEQUENCER_MAX_NUM_TASKS];
	uint8_t _maxTask = 0;

	Decoder *_inputs_decoder[INS_SEQUENCER_MAX_NUM_INPUTS];
	uint16_t _inputs_lastTransitionMicros[INS_SEQUENCER_MAX_NUM_INPUTS];
	uint16_t _inputs_nextTimeoutMicros[INS_SEQUENCER_MAX_NUM_INPUTS];
	uint8_t _inputs_pinState[INS_SEQUENCER_MAX_NUM_INPUTS];
	uint8_t _maxInput = 0;

	uint8_t _pins_pin[INS_SEQUENCER_MAX_NUM_INPUTS]; // Used in both ISR and polling context! This is probably ok as it is used.
	uint8_t _pins_pinState[INS_SEQUENCER_MAX_NUM_INPUTS];
	pin_usage_t _pins_usage[INS_SEQUENCER_MAX_NUM_INPUTS];
	uint8_t _maxPin = 0;

	static const uint8_t PIN_STATE_TIMEOUT = 0x2;
	static const uint8_t PIN_STATE_REPORTED = 0x1;
	inline bool timeoutPinState(uint8_t pinState) { return !!((pinState & PIN_STATE_TIMEOUT) >> 1); }
	inline uint8_t reportedPinState(uint8_t pinState) { return (pinState & PIN_STATE_REPORTED); }

public:
	Scheduler()
	{
		// All these signals that the slot is in use.
		memset(_tasks_task, 0, sizeof(_tasks_task));
		memset(_inputs_decoder, 0, sizeof(_inputs_decoder));
		memset(_pins_usage, 0, sizeof(_pins_usage));
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
		for (uint8_t i = 0; i < INS_SEQUENCER_MAX_NUM_TASKS; ++i)
		{
			if (_tasks_task[i])
				continue;
			_tasks_task[i] = task;
			_tasks_delegate[i] = delegate;
			_tasks_targetTime[i] = fastMicros();
			_tasks_targetTime[i] += task->SteppedTask_step();
			if (i + 1 > _maxTask)
				_maxTask = i + 1;
			return true;
		}
		InsError(*(uint32_t*)"tovf");
		return false;
	}

	// Add task after microseconds.
	bool addDelayed(SteppedTask *task, uint16_t delayUS, Delegate *delegate = nullptr)
	{
		for (uint8_t i = 0; i < INS_SEQUENCER_MAX_NUM_TASKS; ++i)
		{
			if (_tasks_task[i])
				continue;
			_tasks_task[i] = task;
			_tasks_delegate[i] = delegate;
			_tasks_targetTime[i] = fastMicros();
			_tasks_targetTime[i] += delayUS;
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
		for (uint8_t i = 0; i < _maxTask; ++i)
		{
			if (_tasks_task[i] != task)
				continue;
			_tasks_task[i] = nullptr;
			found = true;
			break;
		}
		if (!found)
		{
			InsError(*(uint32_t*)"nstk");
			return false;
		}
		for (uint8_t i = _maxTask; i; --i)
		{
			if (_tasks_task[i - 1])
				return true;
			_maxTask = i;
		}
		return true;
	}

	// Check if task is active.
	bool active(SteppedTask *task)
	{
		for (uint8_t i = 0; i < _maxTask; ++i)
		{
			if (_tasks_task[i] != task)
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
			if (_inputs_decoder[i])
				continue;
			_inputs_decoder[i] = decoder;
			uint16_t now = fastMicros();
			_inputs_lastTransitionMicros[i] = now;
			_inputs_nextTimeoutMicros[i] = now;
			if (i + 1 > _maxInput)
				_maxInput = i + 1;

			int p = 0;
			// Try to find matching slot.
			for (; p < _maxPin; ++p)
			{
				if (!_pins_usage[p] || _pins_pin[p] != pin)
					continue;
				break;
			}
			// Else try to find free slot.
			if (p == _maxPin)
			{
				for (p = 0; p < INS_SEQUENCER_MAX_NUM_INPUTS; ++p)
				{
					if (_pins_usage[p])
						continue;
					_pins_pin[p] = pin;
#if INS_ENABLE_INPUT_FILTER
					_pins_pinState[p] = 3 * digitalRead(pin);
#else
					_pins_pinState[p] = digitalRead(pin);
#endif
					break;
				}
				if (p == INS_SEQUENCER_MAX_NUM_INPUTS)
				{
					InsError(*(uint32_t*)"dovf");
					return false;
				}
			}
#if INS_ENABLE_INPUT_FILTER
			_inputs_pinState[i] = _pins_pinState[p] > 1;
#else
			_inputs_pinState[i] = !!_pins_pinState[p];
#endif
			_pins_usage[p] |= 1 << i;
			if (i + 1 > _maxPin)
				_maxPin = i + 1;
			return true;
		}
		InsError(*(uint32_t*)"dovf");
		return false;
	}

	// Remove decoder.
	bool remove(Decoder *decoder)
	{
		int i = 0;
		for (; i < _maxInput; ++i)
		{
			if (_inputs_decoder[i] != decoder)
				continue;
			_inputs_decoder[i] = nullptr;
			break;
		}
		if (i == _maxInput)
		{
			InsError(*(uint32_t*)"nsdc");
			return false;
		}

		pin_usage_t mask = 1 << i;
		for (int p = 0; p < _maxPin; ++p)
		{
			if (!(_pins_usage[p] & mask))
				continue;
			_pins_usage[p] &= ~mask;
			break;
		}
		for (int i = _maxInput; i; --i)
		{
			if (_inputs_decoder[i - 1])
				break;
			_maxInput = i;
		}
		for (int p = _maxPin; p; --p)
		{
			if (_pins_usage[p - 1])
				break;
			_maxPin = p;
		}
		return true;
	}

	// Check if decoder is active.
	bool active(Decoder *decoder)
	{
		for (int i = 0; i < _maxInput; ++i)
		{
			if (_inputs_decoder[i] != decoder)
				continue;
			return true;
		}
		return false;
	}

	// Iterate all active tasks.
	void poll()
	{
		pollInputs();
		pollTasks();
		pollInputs();
		pollTimeouts();
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
		for (int p = 0; p < _maxPin; ++p)
		{
			if (!_pins_usage[p])
				continue;
#if INS_ENABLE_INPUT_FILTER // Removes single glitches
			bool oldPinState = _pins_pinState[p] > 1;
			uint8_t pinState = digitalRead(_pins_pin[p]);
			if (pinState && _pins_pinState[p] < 3)
				 ++_pins_pinState[p];
			else if (!pinState && _pins_pinState[p] > 0)
				--_pins_pinState[p];
			bool newPinState = _pins_pinState[p] > 1;
#else
			uint8_t oldPinState = _pins_pinState[p];
			uint8_t newPinState = digitalRead(_pins_pin[p]);
			_pins_pinState[p] = newPinState;
#endif
			if (newPinState == oldPinState)
			{
				continue;
			}
#ifdef INS_SAMPLE_DEBUG_PIN
			static uint8_t s_sampleToggle;
			s_sampleToggle ^= 1;
			digitalWrite(INS_SAMPLE_DEBUG_PIN, s_sampleToggle);
#endif
			pin_usage_t usageLeft = _pins_usage[p];
			for (int i = 0; usageLeft && i < _maxInput; ++i)
			{
				pin_usage_t mask = 1 << i;
				if (!(_pins_usage[p] & mask))
					continue;
				usageLeft &= ~mask;

				if (newPinState == reportedPinState(_inputs_pinState[i]))
				{
					continue;
				}
				uint16_t timeToReport = now -_inputs_lastTransitionMicros[i];
				if (timeoutPinState(_inputs_pinState[i]))
				{
					timeToReport = 0;
				}
				else if (timeToReport == 0)
				{
					timeToReport = 1;
				}
				uint16_t delta = _inputs_decoder[i]->Decoder_pulse(reportedPinState(_inputs_pinState[i]), timeToReport);
				_inputs_pinState[i] = newPinState; // Resets timeout state
				_inputs_nextTimeoutMicros[i] = now + delta;
				_inputs_lastTransitionMicros[i] = now;
			}
			now = fastMicros();
		}
	}

	void pollTimeouts()
	{
		uint16_t now = fastMicros();
		for (int i = 0; i < _maxInput; ++i)
		{
			if (!_inputs_decoder[i])
				continue;

			if (_inputs_nextTimeoutMicros[i] != _inputs_lastTransitionMicros[i])
			{
				if (int16_t(_inputs_nextTimeoutMicros[i] - now) < 0)
				{
#ifdef INS_TIMEOUT_DEBUG_PIN
					static uint8_t s_timeoutToggle;
					s_timeoutToggle ^= 1;
					digitalWrite(INS_TIMEOUT_DEBUG_PIN, s_timeoutToggle);
#endif
					_inputs_decoder[i]->Decoder_timeout(reportedPinState(_inputs_pinState[i]));
					_inputs_pinState[i] |= PIN_STATE_TIMEOUT;
				}
			}
		}
	}

	void pollTasks()
	{
		uint16_t now = fastMicros();
		for (int i = 0 ; i < _maxTask; ++i)
		{
			if (!_tasks_task[i])
				continue;
			int16_t timeLeft = _tasks_targetTime[i] - now;
			if (timeLeft > 0)
			{
				continue;
			}
			uint16_t delta = _tasks_task[i]->SteppedTask_step();
			_tasks_targetTime[i] += delta;
			now = fastMicros();
			if (delta == SteppedTask::kInvalidDelta)
			{
				SteppedTask *task = _tasks_task[i];
				_tasks_task[i] = nullptr;
				if (_tasks_delegate[i])
					_tasks_delegate[i]->SchedulerDelegate_done(task);
				continue;
			}
		}
	}
};

#ifdef AVR
template<typename T, size_t N>
class LockFreeFIFO {
private:
	T _data[N];
	volatile uint8_t _writePos = 0;
	volatile uint8_t _readPos = 0;
public:
	bool full() const { return (_writePos + 1) % N == _readPos; }
	T& writeRef() { return _data[_writePos]; }
	void push() { _writePos = (_writePos + 1) % N; }

	bool empty() const { return _readPos == _writePos; }
	const T& readRef() const { return _data[_readPos]; }
	void pop() { _readPos = (_readPos + 1) % N; }
};
#else
template<typename T, size_t N>
class LockFreeFIFO {
private:
	T data[N];
	std::atomic<uint8_t> _writePos = {0};
	std::atomic<uint8_t> _readPos = {0};
public:
	bool full() const
	{
		uint8_t currentWritePos = _writePos.load(std::memory_order_relaxed);
		uint8_t nextWritePos = (currentWritePos + 1) % N;
		uint8_t currenReadPos = _readPos.load(std::memory_order_acquire);
		return nextWritePos == currenReadPos;
	}
	T& writeRef()
	{
		uint8_t currentWritePos = _writePos.load(std::memory_order_relaxed);
		return data[currentWritePos];
	}
	void push()
	{
		uint8_t currentWritePos = _writePos.load(std::memory_order_relaxed);
		uint8_t nextWritePos = (currentWritePos + 1) % N;
		_writePos.store(nextWritePos, std::memory_order_release);
	}

	bool empty() const
	{
		return _readPos.load(std::memory_order_relaxed) == _writePos.load(std::memory_order_acquire);
	}
	const T& readRef() const
	{
		uint8_t currentReadPos = _readPos.load(std::memory_order_relaxed);
		return data[currentReadPos];
	}
	void pop()
	{
		uint8_t currentReadPos = _readPos.load(std::memory_order_relaxed);
		uint8_t nextReadPos = (currentReadPos + 1) % N;
		_readPos.store(nextReadPos, std::memory_order_release);
	}
};

// Input polling and task scheduling
class InterruptScheduler : public Scheduler
{
private:
	struct InputData
	{
		uint16_t micros;
		uint8_t pin;
		uint8_t state;
	};

	LockFreeFIFO<InputData, INS_INPUT_FIFO_LENGTH> _inputFIFO;
	static InterruptScheduler *s_this;
public:
	InterruptScheduler() { s_this = this; }

	bool add(SteppedTask *task, Delegate *delegate = nullptr) { return Scheduler::add(task, delegate); }

	bool add(Decoder *decoder, uint8_t pin)
	{
		bool retval = Scheduler::add(decoder, pin);
		for (int p = 0; p < _maxPin; ++p)
		{
			if (pin != _pins_pin[p])
				continue;
#ifdef UNIT_TEST
			attachInterrupt(pin, pinISR, 0);
#else
			attachInterrupt(digitalPinToInterrupt(pin), pinISR, CHANGE);
#endif
			break;
		}
		return retval;
	}

	bool remove(Decoder *decoder)
	{
		for (int i = 0; i < _maxInput; ++i)
		{
			if (_inputs_decoder[i] != decoder)
				continue;
			pin_usage_t mask = 1 << i;
			for (int p = 0; p < _maxPin; ++p)
			{
				if (!(_pins_usage[p] & mask))
					continue;
#ifdef UNIT_TEST
				detachInterrupt(_pins_pin[p]);
#else
				detachInterrupt(digitalPinToInterrupt(_pins_pin[p]));
#endif
				break;
			}
			break;
		}
		return Scheduler::remove(decoder);
	}

	void poll()
	{
		pollTasks();
		pollInputFIFOs();
		pollTimeouts();
	}

private:
	void pollInputFIFOs()
	{
		while (!_inputFIFO.empty())
		{
			uint16_t now = _inputFIFO.readRef().micros;
			uint8_t pin = _inputFIFO.readRef().pin;
			uint8_t newPinState = _inputFIFO.readRef().state;
			_inputFIFO.pop();

			for (int p = 0; p < _maxPin; ++p)
			{
				if (pin != _pins_pin[p] || !_pins_usage[p])
					continue;
				_pins_pinState[p] = newPinState;
				pin_usage_t usageLeft = _pins_usage[p];
				for (int i = 0; usageLeft && i < _maxInput; ++i)
				{
					pin_usage_t mask = 1 << i;
					if (!(_pins_usage[p] & mask))
						continue;
					usageLeft &= ~mask;

					if (newPinState == reportedPinState(_inputs_pinState[i]))
					{
						continue;
					}
					uint16_t timeToReport = now -_inputs_lastTransitionMicros[i];
					if (timeoutPinState(_inputs_pinState[i]))
					{
						timeToReport = 0;
					}
					else if (timeToReport == 0)
					{
						timeToReport = 1;
					}
					uint16_t delta = _inputs_decoder[i]->Decoder_pulse(reportedPinState(_inputs_pinState[i]), timeToReport);
					_inputs_pinState[i] = newPinState; // Resets timeout state
					_inputs_nextTimeoutMicros[i] = now + delta;
					_inputs_lastTransitionMicros[i] = now;
				}
			}
		}
	}

	void INS_IRAM_ATTR pollInputsISR()
	{
		for (int p = 0; p < _maxPin; ++p)
		{
			if (!_pins_usage[p])
				continue;
			uint8_t oldPinState = _pins_pinState[p];
			uint8_t newPinState = digitalRead(_pins_pin[p]);
			_pins_pinState[p] = newPinState;
			if (newPinState == oldPinState)
			{
				continue;
			}
			// Pray we don't overflow.
			_inputFIFO.writeRef().micros = fastMicros();
			_inputFIFO.writeRef().pin = _pins_pin[p];
			_inputFIFO.writeRef().state = newPinState;
			_inputFIFO.push();
			break;
		}
	}

	static void INS_IRAM_ATTR pinISR()
	{
		s_this->pollInputsISR();
	}
};
#endif

}
#endif
