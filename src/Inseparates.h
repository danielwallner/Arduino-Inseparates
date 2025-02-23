// Copyright (c) 2024 Daniel Wallner

#ifndef _INSEPARATES_H_
#define _INSEPARATES_H_

#include "FastTime.h"

#ifndef AVR
#include <atomic>
#endif
#ifdef UNIT_TEST
#include <assert.h>
#elif defined(ESP8266) || defined(ESP32)
#define USE_FUNCTIONAL_INTERRUPT 0
#include <functional>
#include <FunctionalInterrupt.h>
#endif

#ifndef INS_SEQUENCER_MAX_NUM_TASKS
#ifdef AVR
#define INS_SEQUENCER_MAX_NUM_TASKS 8
#else
#define INS_SEQUENCER_MAX_NUM_TASKS 16
#endif
#endif

#ifndef INS_SEQUENCER_MAX_DECODERS
#ifdef AVR
#define INS_SEQUENCER_MAX_DECODERS 8
#else
#define INS_SEQUENCER_MAX_DECODERS 16
#endif
#endif

#ifndef INS_SEQUENCER_MAX_NUM_INPUTS
#ifdef AVR
#define INS_SEQUENCER_MAX_NUM_INPUTS 8
#else
#define INS_SEQUENCER_MAX_NUM_INPUTS 16
#endif
#endif

#ifndef INS_INPUT_FIFO_LENGTH
#ifdef AVR
#define INS_INPUT_FIFO_LENGTH 64
#else
#define INS_INPUT_FIFO_LENGTH 1024
#endif
#endif

#ifndef AVR
#include <map>
#include <memory>
#endif

#define INS_STR_(v) #v
#define INS_STR(v) INS_STR_(v)

#if defined(ARDUINO_ISR_ATTR)
#define INS_IRAM_ATTR ARDUINO_ISR_ATTR
#elif defined(IRAM_ATTR)
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
	INS_IRAM_ATTR bool full() const
	{
		uint8_t currentWritePos = _writePos.load(std::memory_order_relaxed);
		uint8_t nextWritePos = (currentWritePos + 1) % N;
		uint8_t currenReadPos = _readPos.load(std::memory_order_acquire);
		return nextWritePos == currenReadPos;
	}
	INS_IRAM_ATTR T& writeRef()
	{
		uint8_t currentWritePos = _writePos.load(std::memory_order_relaxed);
		return data[currentWritePos];
	}
	INS_IRAM_ATTR void push()
	{
		uint8_t currentWritePos = _writePos.load(std::memory_order_relaxed);
		uint8_t nextWritePos = (currentWritePos + 1) % N;
		_writePos.store(nextWritePos, std::memory_order_release);
	}
	INS_IRAM_ATTR bool empty() const
	{
		return _readPos.load(std::memory_order_relaxed) == _writePos.load(std::memory_order_acquire);
	}
	INS_IRAM_ATTR const T& readRef() const
	{
		uint8_t currentReadPos = _readPos.load(std::memory_order_relaxed);
		return data[currentReadPos];
	}
	INS_IRAM_ATTR void pop()
	{
		uint8_t currentReadPos = _readPos.load(std::memory_order_relaxed);
		uint8_t nextReadPos = (currentReadPos + 1) % N;
		_readPos.store(nextReadPos, std::memory_order_release);
	}
};
#endif

extern "C"
{
INS_IRAM_ATTR void pinISR0();
INS_IRAM_ATTR void pinISR1();
INS_IRAM_ATTR void pinISR2();
#ifndef AVR
INS_IRAM_ATTR void pinISR3();
INS_IRAM_ATTR void pinISR4();
INS_IRAM_ATTR void pinISR5();
INS_IRAM_ATTR void pinISR6();
INS_IRAM_ATTR void pinISR7();
#endif
}

// Input polling and task scheduling
class Scheduler
{
public:
	class Delegate
	{
	public:
		virtual void SchedulerDelegate_done(SteppedTask *task) = 0;
	};

	struct InputData
	{
		ins_micros_t micros;
		uint8_t pin;
		uint8_t state;
	};

#ifdef AVR
#define MAX_PIN_CALLBACKS 3
#else
#define MAX_PIN_CALLBACKS 8
#endif
	class PinStatePusher
	{
		Scheduler *_scheduler;
		uint8_t _pin;
	public:
#if !(UNIT_TEST || USE_FUNCTIONAL_INTERRUPT)
		static uint8_t s_pinUsage[MAX_PIN_CALLBACKS];
#endif
		PinStatePusher(Scheduler *scheduler, uint8_t pin) : _scheduler(scheduler), _pin(pin)
		{
#if UNIT_TEST || USE_FUNCTIONAL_INTERRUPT
			attachInterrupt(digitalPinToInterrupt(_pin), std::bind(&PinStatePusher::pinISR, this), CHANGE);
#else
			schedulerInstance(_scheduler);

			uint8_t i = 0;
			for (; i < MAX_PIN_CALLBACKS; ++i)
			{
				if (s_pinUsage[i] == (uint8_t)-1)
				{
					s_pinUsage[i] = _pin;
					void (*isr)();
					switch (i)
					{
					case 0: isr = pinISR0; break;
					case 1: isr = pinISR1; break;
					case 2: isr = pinISR2; break;
#ifndef AVR
					case 3: isr = pinISR3; break;
					case 4: isr = pinISR4; break;
					case 5: isr = pinISR5; break;
					case 6: isr = pinISR6; break;
					case 7: isr = pinISR7; break;
#endif
					default: InsError(*(uint32_t*)"icnt");
					}
					attachInterrupt(digitalPinToInterrupt(_pin), isr, CHANGE);
					break;
				}
			}
			if (i == MAX_PIN_CALLBACKS)
				InsError(*(uint32_t*)"insp");
#endif
		}
		~PinStatePusher()
		{
			detachInterrupt(digitalPinToInterrupt(_pin));
#if !(UNIT_TEST || USE_FUNCTIONAL_INTERRUPT)
			for (uint8_t i = 0; i < MAX_PIN_CALLBACKS; ++i)
			{
				if (s_pinUsage[i] == _pin)
				{
					s_pinUsage[i] = (uint8_t)-1;
					break;
				}
			}
#endif
		}

		uint8_t pin() { return _pin; }

#if UNIT_TEST || USE_FUNCTIONAL_INTERRUPT
		/*INS_IRAM_ATTR*/ void pinISR()
		{
			auto &inputFifo = _scheduler->_inputFIFO;
			auto &w = inputFifo.writeRef();
			w.micros = fastMicros();
			w.pin = _pin;
			w.state = digitalRead(_pin);
			inputFifo.push();
		}
#endif
		INS_IRAM_ATTR static Scheduler *schedulerInstance(Scheduler *inst = nullptr)
		{
			static Scheduler *s_instance;
			if (inst)
				s_instance = inst;
			return s_instance;
		}
	};

private:
	friend class PinStatePusher;

#if INS_SEQUENCER_MAX_NUM_TASKS <= 8
	typedef uint8_t task_flags_t;
#elif INS_SEQUENCER_MAX_NUM_TASKS <= 16
	typedef uint16_t task_flags_t;
#elif INS_SEQUENCER_MAX_NUM_TASKS <= 32
	typedef uint32_t task_flags_t;
#elif INS_SEQUENCER_MAX_NUM_TASKS <= 64
	typedef uint64_t task_flags_t;
#else
	Too many tasks
#endif
#if INS_SEQUENCER_MAX_DECODERS <= 8
	typedef uint8_t pin_flags_t;
#elif INS_SEQUENCER_MAX_DECODERS <= 16
	typedef uint16_t pin_flags_t;
#elif INS_SEQUENCER_MAX_DECODERS <= 32
	typedef uint32_t pin_flags_t;
#elif INS_SEQUENCER_MAX_DECODERS <= 64
	typedef uint64_t pin_flags_t;
#else
	Too many inputs
#endif
#if INS_SEQUENCER_MAX_NUM_INPUTS <= 8
	typedef uint8_t pin_usage_t;
#elif INS_SEQUENCER_MAX_NUM_INPUTS <= 16
	typedef uint16_t pin_usage_t;
#elif INS_SEQUENCER_MAX_NUM_INPUTS <= 32
	typedef uint32_t pin_usage_t;
#elif INS_SEQUENCER_MAX_NUM_INPUTS <= 64
	typedef uint64_t pin_usage_t;
#else
	Too many decoders
#endif

	LockFreeFIFO<InputData, INS_INPUT_FIFO_LENGTH> _inputFIFO;

	SteppedTask *_tasks_task[INS_SEQUENCER_MAX_NUM_TASKS] = { 0 };
	Delegate *_tasks_delegate[INS_SEQUENCER_MAX_NUM_TASKS];
	ins_micros_t _tasks_targetTime[INS_SEQUENCER_MAX_NUM_TASKS];
	task_flags_t _taskIsAbsolute = 0;
	uint8_t _maxTask = 0;

	Decoder *_decoders[INS_SEQUENCER_MAX_DECODERS] = { 0 };
	ins_micros_t _decoders_lastTransitionMicros[INS_SEQUENCER_MAX_DECODERS];
	ins_micros_t _decoders_nextTimeoutMicros[INS_SEQUENCER_MAX_DECODERS];
	uint8_t _decoders_pinState[INS_SEQUENCER_MAX_DECODERS];
	uint8_t _maxDecoder = 0;

	// This is used both for polled and interrupt driven pins.
	// That is somewhat less efficient when mixing pin types.
	uint8_t _pins_pin[INS_SEQUENCER_MAX_NUM_INPUTS];
	uint8_t _pins_pinState[INS_SEQUENCER_MAX_NUM_INPUTS];
	// Since there can be multiple decoders using a single pin this is a bit matrix where for each pin the bits corresponds to the decoder index
	pin_usage_t _pins_usage[INS_SEQUENCER_MAX_NUM_INPUTS] = { 0 };
	pin_flags_t _pins_isInterrupt = 0;
	uint8_t _maxPolledPin = 0;
	uint8_t _maxInterruptPin = 0;

	static const uint8_t PIN_STATE_TIMEOUT = 0x2;
	static const uint8_t PIN_STATE_REPORTED = 0x1;
	inline bool timeoutPinState(uint8_t pinState) { return !!((pinState & PIN_STATE_TIMEOUT) >> 1); }
	inline uint8_t reportedPinState(uint8_t pinState) { return (pinState & PIN_STATE_REPORTED); }

#if AVR
	PinStatePusher *_pinInterrupts[MAX_PIN_CALLBACKS] = { nullptr };
#else
	std::map<uint8_t, std::unique_ptr<PinStatePusher>> _pinInterrupts;
#endif

public:
	Scheduler()
	{
#if !(UNIT_TEST || USE_FUNCTIONAL_INTERRUPT)
		for (uint8_t i = 0; i < MAX_PIN_CALLBACKS; ++i)
		{
			PinStatePusher::s_pinUsage[i] = (uint8_t)-1;
		}
#endif
	}

	INS_IRAM_ATTR LockFreeFIFO<InputData, INS_INPUT_FIFO_LENGTH> &inputFIFO() { return _inputFIFO; };

	void begin()
	{
#if INS_FAST_COUNT
		setupFastTime();
#endif
	}

	// Add and step task.
	bool add(SteppedTask *task, Delegate *delegate = nullptr, bool absolute = true)
	{
		for (uint8_t i = 0; i < INS_SEQUENCER_MAX_NUM_TASKS; ++i)
		{
			if (_tasks_task[i])
				continue;
			_tasks_task[i] = task;
			_tasks_delegate[i] = delegate;
			_tasks_targetTime[i] = fastMicros();
			_tasks_targetTime[i] += task->SteppedTask_step();
			task_flags_t bitMask = 1ULL << i;
			_taskIsAbsolute &= ~bitMask;
			if (absolute)
				_taskIsAbsolute |= bitMask;
			if (i + 1 > _maxTask)
				_maxTask = i + 1;
			return true;
		}
		InsError(*(uint32_t*)"tovf");
		return false;
	}

	// Add task after microseconds.
	bool addDelayed(SteppedTask *task, ins_micros_t delayUS, Delegate *delegate = nullptr, bool absolute = true)
	{
		for (uint8_t i = 0; i < INS_SEQUENCER_MAX_NUM_TASKS; ++i)
		{
			if (_tasks_task[i])
				continue;
			_tasks_task[i] = task;
			_tasks_delegate[i] = delegate;
			_tasks_targetTime[i] = fastMicros();
			_tasks_targetTime[i] += delayUS;
			task_flags_t bitMask = 1ULL << i;
			_taskIsAbsolute &= ~bitMask;
			if (absolute)
				_taskIsAbsolute |= bitMask;
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
	bool add(Decoder *decoder, uint8_t pin, bool interrupt = false)
	{
#ifdef AVR
		// TODO: make this correct for other versions than 328P
		if (pin < 2 || pin > 3)
			interrupt = false; // This should perhaps be reported as an error instead.
#endif
#ifdef ESP8266
		if (pin == 16)
			interrupt = false;
#endif
		for (uint8_t i = 0; i < INS_SEQUENCER_MAX_NUM_INPUTS; ++i)
		{
			if (_decoders[i] == decoder)
			{
				InsError(*(uint32_t*)"dupl");
				return false;
			}
			if (_decoders[i])
				continue;
			_decoders[i] = decoder;
			ins_micros_t now = fastMicros();
			_decoders_lastTransitionMicros[i] = now;
			_decoders_nextTimeoutMicros[i] = now;
			if (i + 1 > _maxDecoder)
				_maxDecoder = i + 1;

			uint8_t p = pinIndex(pin);
			bool newPin = !_pins_usage[p];
			if (newPin)
			{
				_pins_pin[p] = pin;
#if INS_ENABLE_INPUT_FILTER
				_pins_pinState[p] = 3 * digitalRead(pin);
#else
				_pins_pinState[p] = digitalRead(pin);
#endif
			}
#if INS_ENABLE_INPUT_FILTER
			_decoders_pinState[i] = _pins_pinState[p] > 1;
#else
			_decoders_pinState[i] = !!_pins_pinState[p];
#endif
			_pins_usage[p] |= 1ULL << i;
			if (interrupt)
			{
				if (i + 1 > _maxInterruptPin)
					_maxInterruptPin = i + 1;

				pin_flags_t pinIndexMask = 1ULL << p;
				_pins_isInterrupt |= pinIndexMask;
				if (newPin)
				{
#ifdef UNIT_TEST
					assert(!_pinInterrupts.count(pin));
#endif
#if AVR
					for (uint8_t psp = 0; psp < MAX_PIN_CALLBACKS; ++psp)
					{
						if (_pinInterrupts[psp])
						{
							continue;
						}
						_pinInterrupts[psp] = new PinStatePusher(this, pin);
						break;
					}
#else
					_pinInterrupts[pin] = std::unique_ptr<PinStatePusher>(new PinStatePusher(this, pin));
#endif
				}
			}
			else
			{
				if (i + 1 > _maxPolledPin)
					_maxPolledPin = i + 1;
			}
			return true;
		}
		InsError(*(uint32_t*)"dovf");
		return false;
	}

	// Remove decoder.
	bool remove(Decoder *decoder)
	{
		for (uint8_t i = 0; i < _maxDecoder; ++i)
		{
			if (_decoders[i] != decoder)
				continue;
			pin_usage_t decoderBitMask = 1ULL << i;
			pin_flags_t pinIndexMask = 1ULL;
			for (uint8_t p = 0; p < _maxPolledPin || p < _maxInterruptPin; ++p, pinIndexMask <<= 1)
			{
				if (!(_pins_isInterrupt & pinIndexMask))
				{
					continue;
				}
				if (!(_pins_usage[p] & decoderBitMask))
					continue;
				_pins_isInterrupt &= ~pinIndexMask;
#if AVR
				for (uint8_t psp = 0; psp < MAX_PIN_CALLBACKS; ++psp)
				{
					if (_pinInterrupts[psp] && _pinInterrupts[psp]->pin() == _pins_pin[p])
					{
						delete _pinInterrupts[psp];
						_pinInterrupts[psp] = nullptr;
						break;
					}
				}
#else
				_pinInterrupts.erase(_pins_pin[p]);
#endif
				break;
			}
			break;
		}
		uint8_t i = 0;
		for (; i < _maxDecoder; ++i)
		{
			if (_decoders[i] != decoder)
				continue;
			_decoders[i] = nullptr;
			break;
		}
		if (i == _maxDecoder)
		{
			InsError(*(uint32_t*)"nsdc");
			return false;
		}

		pin_usage_t decoderBitMask = 1ULL << i;
		for (uint8_t p = 0;  p < _maxPolledPin || p < _maxInterruptPin; ++p)
		{
			if (!(_pins_usage[p] & decoderBitMask))
				continue;
			_pins_usage[p] &= ~decoderBitMask;
			break;
		}
		for (uint8_t i = _maxDecoder; i; --i)
		{
			if (_decoders[i - 1])
				break;
			_maxDecoder = i;
		}
		for (uint8_t p = _maxPolledPin; p; --p)
		{
			pin_flags_t pinIndexMask = 1ULL << (p - 1);
			if (_pins_usage[p - 1] && !(pinIndexMask & _pins_isInterrupt))
				break;
			_maxPolledPin = p;
		}
		for (uint8_t p = _maxInterruptPin; p; --p)
		{
			pin_flags_t pinIndexMask = 1ULL << (p - 1);
			if (_pins_usage[p - 1] && (pinIndexMask & _pins_isInterrupt))
				break;
			_maxInterruptPin = p;
		}
		return true;
	}

	// Check if decoder is active.
	bool active(Decoder *decoder)
	{
		for (uint8_t i = 0; i < _maxDecoder; ++i)
		{
			if (_decoders[i] != decoder)
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
		pollInputFIFOs();
		pollTimeouts();
	}

	// Simple blocking wrapper of step() that runs until finished.
	static void run(SteppedTask *task);

#ifdef UNIT_TEST
	static void runFor(SteppedTask *task, unsigned steps);
#endif

private:
	uint8_t pinIndex(uint8_t pin, bool findOnly = false)
	{
		uint8_t p = 0;
		// Try to find matching slot.
		for (; p < _maxPolledPin || p < _maxInterruptPin; ++p)
		{
			if (!_pins_usage[p] || _pins_pin[p] != pin)
				continue;
			return p;
		}
		if (findOnly)
		{
			InsError(*(uint32_t*)"pnfd");
			return -1;
		}
		// Else try to find free slot.
		for (p = 0; p < INS_SEQUENCER_MAX_NUM_INPUTS; ++p)
		{
			if (_pins_usage[p])
				continue;
			return p;
		}
		if (p == INS_SEQUENCER_MAX_NUM_INPUTS)
		{
			InsError(*(uint32_t*)"povf");
			return -1;
		}
		return p;
	}

	void pollInputs()
	{
		pin_flags_t pinIndexMask = 1ULL;
		ins_micros_t now = fastMicros();
		for (uint8_t p = 0; p < _maxPolledPin || p < _maxInterruptPin; ++p, pinIndexMask <<= 1)
		{
			if (_pins_isInterrupt & pinIndexMask)
				continue;
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
			for (uint8_t i = 0; usageLeft && i < _maxDecoder; ++i)
			{
				pin_usage_t decoderBitMask = 1ULL << i;
				if (!(usageLeft & decoderBitMask))
					continue;
				usageLeft &= ~decoderBitMask;

				if (newPinState == reportedPinState(_decoders_pinState[i]))
				{
					continue;
				}
				uint16_t timeToReport = now -_decoders_lastTransitionMicros[i];
				if (timeoutPinState(_decoders_pinState[i]))
				{
					timeToReport = 0;
				}
				else if (timeToReport == 0)
				{
					timeToReport = 1;
				}

				uint16_t delta = _decoders[i]->Decoder_pulse(reportedPinState(_decoders_pinState[i]), timeToReport);
#ifdef UNIT_TEST
				assert(delta <= SteppedTask::kMaxSleepMicros);
#endif
				_decoders_pinState[i] = newPinState; // Resets timeout state
				_decoders_nextTimeoutMicros[i] = now + delta;
				_decoders_lastTransitionMicros[i] = now;
			}
			now = fastMicros();
		}
	}

	void pollInputFIFOs()
	{
		while (!_inputFIFO.empty())
		{
			ins_micros_t now = _inputFIFO.readRef().micros;
			uint8_t pin = _inputFIFO.readRef().pin;
			uint8_t newPinState = _inputFIFO.readRef().state;
			_inputFIFO.pop();

			pin_flags_t pinIndexMask = 1ULL;
			for (uint8_t p = 0; p < _maxPolledPin || p < _maxInterruptPin; ++p, pinIndexMask <<= 1)
			{
				if (pin != _pins_pin[p] || !_pins_usage[p])
					continue;
				_pins_pinState[p] = newPinState;
				pin_usage_t usageLeft = _pins_usage[p];
				for (uint8_t i = 0; usageLeft && i < _maxDecoder; ++i)
				{
					pin_usage_t decoderBitMask = 1ULL << i;
					if (!(usageLeft & decoderBitMask))
						continue;
					usageLeft &= ~decoderBitMask;

					if (newPinState == reportedPinState(_decoders_pinState[i]))
					{
						continue;
					}
					uint16_t timeToReport = now -_decoders_lastTransitionMicros[i];
					if (timeoutPinState(_decoders_pinState[i]))
					{
						timeToReport = 0;
					}
					else if (timeToReport == 0)
					{
						timeToReport = 1;
					}
					uint16_t delta = _decoders[i]->Decoder_pulse(reportedPinState(_decoders_pinState[i]), timeToReport);
#ifdef UNIT_TEST
					assert(delta <= SteppedTask::kMaxSleepMicros);
#endif
					_decoders_pinState[i] = newPinState; // Resets timeout state
					_decoders_nextTimeoutMicros[i] = now + delta;
					_decoders_lastTransitionMicros[i] = now;
				}
			}
		}
	}

	void pollTimeouts()
	{
		ins_micros_t now = fastMicros();
		for (uint8_t i = 0; i < _maxDecoder; ++i)
		{
			if (!_decoders[i])
				continue;

			if (_decoders_nextTimeoutMicros[i] != _decoders_lastTransitionMicros[i])
			{
				if (ins_smicros_t(_decoders_nextTimeoutMicros[i] - now) < 0)
				{
#ifdef INS_TIMEOUT_DEBUG_PIN
					static uint8_t s_timeoutToggle;
					s_timeoutToggle ^= 1;
					digitalWrite(INS_TIMEOUT_DEBUG_PIN, s_timeoutToggle);
#endif
					_decoders[i]->Decoder_timeout(reportedPinState(_decoders_pinState[i]));
					_decoders_pinState[i] |= PIN_STATE_TIMEOUT;
					_decoders_nextTimeoutMicros[i] = _decoders_lastTransitionMicros[i];
				}
			}
		}
	}

	void pollTasks()
	{
		ins_micros_t now = fastMicros();
		for (uint8_t i = 0 ; i < _maxTask; ++i)
		{
			if (!_tasks_task[i])
				continue;
			ins_smicros_t timeLeft = _tasks_targetTime[i] - now;
			if (timeLeft > 0)
			{
				continue;
			}
			uint16_t delta = _tasks_task[i]->SteppedTask_step();
			now = fastMicros();
			if (_taskIsAbsolute & (1ULL << i))
				// Try to keep up with absolute time.
				// This may lead to shorter delays when attempting to keep up.
				_tasks_targetTime[i] += delta;
			else
				// Always wait at least the delay
				_tasks_targetTime[i] = now + delta;
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

}
#endif
